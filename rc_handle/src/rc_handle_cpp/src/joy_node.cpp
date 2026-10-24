#include "rc_handle_cpp/joy_node.hpp"

#include <cmath>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "rcl_interfaces/msg/parameter_descriptor.hpp"

namespace rc_handle_cpp
{

JoyNode::JoyNode(
  const rclcpp::NodeOptions & options, std::unique_ptr<JoystickDevice> device)
: Node("rc_handle_cpp_node", options)
{
  rcl_interfaces::msg::ParameterDescriptor descriptor;
  descriptor.read_only = true;
  const auto path = declare_parameter<std::string>("dev", "/dev/input/js0", descriptor);
  const double rate = declare_parameter<double>("publish_rate", 50.0, descriptor);
  const double interval = declare_parameter<double>("reconnect_interval", 1.0, descriptor);
  frame_id_ = declare_parameter<std::string>("frame_id", "joy", descriptor);
  if (path.empty()) {
    throw std::invalid_argument("dev must not be empty");
  }
  if (!std::isfinite(rate) || rate < 1.0 || rate > 1000.0) {
    throw std::invalid_argument("publish_rate must be finite and in [1.0, 1000.0] Hz");
  }
  if (!std::isfinite(interval) || interval <= 0.0) {
    throw std::invalid_argument("reconnect_interval must be finite and > 0.0 seconds");
  }
  reconnect_interval_ = std::chrono::duration<double>(interval);
  device_ = device ? std::move(device) : std::make_unique<JoystickDevice>(path);
  raw_publisher_ = create_publisher<rc_handle_msgs::msg::JoyRaw>("joy_raw", 10);
  joy_publisher_ = create_publisher<sensor_msgs::msg::Joy>("joy", 10);
  // Wall timer and reconnect clock continue even if simulated ROS time pauses.
  timer_ = create_wall_timer(std::chrono::duration<double>(1.0 / rate), [this]() {tick();});
  RCLCPP_INFO(
    get_logger(), "Publishing raw int16/uint8 on %s and unscaled Joy on %s at %g Hz; device: %s",
    raw_publisher_->get_topic_name(), joy_publisher_->get_topic_name(), rate,
    device_->path().c_str());
}

void JoyNode::tick()
{
  if (!device_->is_open()) {
    if (SteadyClock::now() < next_open_) {
      return;
    }
    next_open_ = SteadyClock::now() + reconnect_interval_;
    try {
      device_->open();
    } catch (const std::system_error & error) {
      if (last_open_error_ != error.what()) {
        RCLCPP_WARN(
          get_logger(), "Cannot open %s: %s; retrying every %g s",
          device_->path().c_str(), error.what(), reconnect_interval_.count());
      }
      last_open_error_ = error.what();
      return;
    }
    last_open_error_.clear();
    RCLCPP_INFO(
      get_logger(), "Connected: %s; %zu axes, %zu buttons", device_->name().c_str(),
      device_->state().axes.size(), device_->state().buttons.size());
  }
  try {
    device_->read_state();
  } catch (const std::system_error & error) {
    RCLCPP_WARN(get_logger(), "Joystick disconnected/read failed: %s", error.what());
    device_->close();
    publish_state(true);
    next_open_ = SteadyClock::now() + reconnect_interval_;
    return;
  }
  publish_state();
}

void JoyNode::publish_state(bool neutral)
{
  rc_handle_msgs::msg::JoyRaw raw;
  raw.header.stamp = now();
  raw.header.frame_id = frame_id_;
  const auto & state = device_->state();
  if (neutral) {
    raw.axes.assign(state.axes.size(), 0);
    raw.buttons.assign(state.buttons.size(), 0);
  } else {
    raw.axes = state.axes;
    raw.buttons = state.buttons;
  }
  raw_publisher_->publish(raw);

  // Preserve the raw values on the fixed float32/int32 standard Joy schema.
  sensor_msgs::msg::Joy joy;
  joy.header = raw.header;
  joy.axes.assign(raw.axes.begin(), raw.axes.end());
  joy.buttons.assign(raw.buttons.begin(), raw.buttons.end());
  joy_publisher_->publish(joy);
}

}  // namespace rc_handle_cpp
