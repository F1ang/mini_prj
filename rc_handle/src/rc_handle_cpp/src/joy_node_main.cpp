#include <exception>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "rc_handle_cpp/joy_node.hpp"

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  int result = 0;
  try {
    rclcpp::spin(std::make_shared<rc_handle_cpp::JoyNode>());
  } catch (const std::exception & error) {
    RCLCPP_FATAL(rclcpp::get_logger("rc_handle_cpp"), "%s", error.what());
    result = 1;
  }
  if (rclcpp::ok()) {
    rclcpp::shutdown();
  }
  return result;
}
