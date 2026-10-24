#ifndef RC_HANDLE_CPP__JOY_NODE_HPP_
#define RC_HANDLE_CPP__JOY_NODE_HPP_

#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "rc_handle_msgs/msg/joy_raw.hpp"
#include "sensor_msgs/msg/joy.hpp"
#include "rc_handle_cpp/joystick_device.hpp"

namespace rc_handle_cpp
{

class JoyNode : public rclcpp::Node
{
public:
  explicit JoyNode(
    const rclcpp::NodeOptions & options = rclcpp::NodeOptions(),
    std::unique_ptr<JoystickDevice> device = nullptr);

private:
  void tick();
  void publish_state(bool neutral = false);

  using SteadyClock = std::chrono::steady_clock;
  using SteadyTime = std::chrono::time_point<SteadyClock, std::chrono::duration<double>>;
  std::unique_ptr<JoystickDevice> device_;
  std::string frame_id_;
  std::string last_open_error_;
  std::chrono::duration<double> reconnect_interval_{1.0};
  SteadyTime next_open_{};
  rclcpp::Publisher<rc_handle_msgs::msg::JoyRaw>::SharedPtr raw_publisher_;
  rclcpp::Publisher<sensor_msgs::msg::Joy>::SharedPtr joy_publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace rc_handle_cpp

#endif  // RC_HANDLE_CPP__JOY_NODE_HPP_
