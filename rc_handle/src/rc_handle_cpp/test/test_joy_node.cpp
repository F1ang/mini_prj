#include "rc_handle_cpp/joy_node.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "rclcpp/executors/single_threaded_executor.hpp"

namespace
{

using Raw = rc_handle_msgs::msg::JoyRaw;
using Joy = sensor_msgs::msg::Joy;
static_assert(std::is_same_v<decltype(Raw{}.axes), std::vector<std::int16_t>>);
static_assert(std::is_same_v<decltype(Raw{}.buttons), std::vector<std::uint8_t>>);

class ScriptedDevice : public rc_handle_cpp::JoystickDevice
{
public:
  ScriptedDevice() : JoystickDevice("/scripted/joystick") {}

  void open() override
  {
    ++open_attempts;
    if (!available) {
      throw std::system_error(ENOENT, std::generic_category(), "not connected");
    }
    state_.axes = next_axes;
    state_.buttons = next_buttons;
    connected = true;
    ++connections;
  }

  void read_state() override
  {
    if (!available) {
      throw std::system_error(ENODEV, std::generic_category(), "unplugged");
    }
  }

  void close() noexcept override {connected = false;}
  bool is_open() const noexcept override {return connected;}

  bool available{false};
  bool connected{false};
  int open_attempts{0};
  int connections{0};
  std::vector<std::int16_t> next_axes{-32768, 32767, 1, -1};
  std::vector<std::uint8_t> next_buttons{1, 255};
};

class JoyNodeTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    context_ = std::make_shared<rclcpp::Context>();
    context_->init(0, nullptr);
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    namespace_ = "/rc_handle_cpp_test_" + std::to_string(unique);
  }

  void TearDown() override
  {
    context_->shutdown("test complete");
  }

  rclcpp::NodeOptions options() const
  {
    return rclcpp::NodeOptions().context(context_).arguments(
      {"--ros-args", "-r", "__ns:=" + namespace_});
  }

  std::shared_ptr<rclcpp::Context> context_;
  std::string namespace_;
};

TEST_F(JoyNodeTest, PublishesRawTypesAndRecoversFromMissingOrDisconnectedDevice)
{
  auto device = std::make_unique<ScriptedDevice>();
  auto * scripted = device.get();
  auto settings = options();
  settings.parameter_overrides({
    rclcpp::Parameter("publish_rate", 100.0),
    rclcpp::Parameter("reconnect_interval", 0.05),
    rclcpp::Parameter("frame_id", "test_joy")});
  auto driver = std::make_shared<rc_handle_cpp::JoyNode>(settings, std::move(device));
  auto listener = std::make_shared<rclcpp::Node>("test_listener", options());
  std::vector<Raw> raw_messages;
  std::vector<Raw> sensor_messages;
  std::vector<Joy> joy_messages;
  auto raw_sub = listener->create_subscription<Raw>(
    "joy_raw", 10, [&raw_messages](const Raw & msg) {raw_messages.push_back(msg);});
  auto sensor_sub = listener->create_subscription<Raw>(
    "joy_raw", rclcpp::SensorDataQoS(),
    [&sensor_messages](const Raw & msg) {sensor_messages.push_back(msg);});
  auto joy_sub = listener->create_subscription<Joy>(
    "joy", 10, [&joy_messages](const Joy & msg) {joy_messages.push_back(msg);});
  rclcpp::ExecutorOptions executor_options;
  executor_options.context = context_;
  rclcpp::executors::SingleThreadedExecutor executor(executor_options);
  executor.add_node(driver);
  executor.add_node(listener);

  auto spin_until = [&executor](auto predicate, double seconds = 5.0) {
      const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::duration<double>(seconds);
      while (std::chrono::steady_clock::now() < deadline) {
        executor.spin_some();
        if (predicate()) {
          return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      return predicate();
    };

  ASSERT_TRUE(spin_until([&]() {return scripted->open_attempts > 0;}));
  EXPECT_TRUE(raw_messages.empty());
  EXPECT_TRUE(joy_messages.empty());
  scripted->available = true;
  ASSERT_TRUE(spin_until([&]() {
    return raw_messages.size() >= 3 && sensor_messages.size() >= 3 && joy_messages.size() >= 3;
  }));
  EXPECT_EQ(raw_messages.back().axes, scripted->next_axes);
  EXPECT_EQ(raw_messages.back().buttons, scripted->next_buttons);
  EXPECT_EQ(joy_messages.back().axes, (std::vector<float>{-32768, 32767, 1, -1}));
  EXPECT_EQ(joy_messages.back().buttons, (std::vector<std::int32_t>{1, 255}));
  EXPECT_EQ(raw_messages.back().header.frame_id, "test_joy");
  EXPECT_GT(raw_messages.back().header.stamp.sec, 0);
  bool matching_headers = false;
  for (const auto & raw : raw_messages) {
    matching_headers = matching_headers || std::any_of(
      joy_messages.begin(), joy_messages.end(),
      [&raw](const Joy & joy) {return raw.header == joy.header;});
  }
  EXPECT_TRUE(matching_headers);

  scripted->available = false;
  ASSERT_TRUE(spin_until([&]() {
    return raw_messages.back().buttons == std::vector<std::uint8_t>{0, 0} &&
           joy_messages.back().buttons == std::vector<std::int32_t>{0, 0};
  }));
  EXPECT_EQ(raw_messages.back().axes, (std::vector<std::int16_t>{0, 0, 0, 0}));
  EXPECT_EQ(joy_messages.back().axes, (std::vector<float>{0, 0, 0, 0}));
  spin_until([]() {return false;}, 0.15);
  const auto raw_count = raw_messages.size();
  const auto joy_count = joy_messages.size();
  spin_until([]() {return false;}, 0.1);
  EXPECT_EQ(raw_messages.size(), raw_count);
  EXPECT_EQ(joy_messages.size(), joy_count);

  scripted->next_axes = {-1234, 2345};
  scripted->next_buttons = {0, 1, 0};
  scripted->available = true;
  ASSERT_TRUE(spin_until([&]() {
    return raw_messages.back().axes == scripted->next_axes &&
           raw_messages.back().buttons == scripted->next_buttons &&
           joy_messages.back().axes == std::vector<float>{-1234, 2345};
  }));
  EXPECT_EQ(scripted->connections, 2);
  EXPECT_EQ(joy_messages.back().buttons, (std::vector<std::int32_t>{0, 1, 0}));
}

TEST_F(JoyNodeTest, RejectsInvalidParameters)
{
  const std::vector<rclcpp::Parameter> invalid{
    rclcpp::Parameter("publish_rate", 0.0),
    rclcpp::Parameter("publish_rate", 1001.0),
    rclcpp::Parameter("publish_rate", std::numeric_limits<double>::quiet_NaN()),
    rclcpp::Parameter("publish_rate", std::numeric_limits<double>::infinity()),
    rclcpp::Parameter("reconnect_interval", 0.0),
    rclcpp::Parameter("reconnect_interval", -1.0),
    rclcpp::Parameter("reconnect_interval", std::numeric_limits<double>::quiet_NaN()),
    rclcpp::Parameter("reconnect_interval", std::numeric_limits<double>::infinity()),
    rclcpp::Parameter("dev", std::string{})};
  for (const auto & parameter : invalid) {
    auto settings = options();
    settings.parameter_overrides({parameter});
    EXPECT_THROW(std::make_shared<rc_handle_cpp::JoyNode>(settings), std::invalid_argument);
  }
}

}  // namespace
