#include <memory>
#include <sstream>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rc_handle_msgs/msg/joy_raw.hpp"

class JoyRawListener : public rclcpp::Node
{
public:
  JoyRawListener()
  : Node("joy_raw_listener_cpp")
  {
    subscription_ = create_subscription<rc_handle_msgs::msg::JoyRaw>(
      "joy_raw", 10, [this](const rc_handle_msgs::msg::JoyRaw & msg) {
        if (received_ && msg.axes == axes_ && msg.buttons == buttons_) {
          return;
        }
        std::ostringstream output;
        output << "axes=[";
        for (std::size_t index = 0; index < msg.axes.size(); ++index) {
          output << (index == 0 ? "" : ", ") << msg.axes[index];
        }
        output << "] buttons=[";
        for (std::size_t index = 0; index < msg.buttons.size(); ++index) {
          // uint8_t streams as a character unless explicitly promoted.
          output << (index == 0 ? "" : ", ") << static_cast<unsigned int>(msg.buttons[index]);
        }
        output << "]";
        RCLCPP_INFO(get_logger(), "%s", output.str().c_str());
        axes_ = msg.axes;
        buttons_ = msg.buttons;
        received_ = true;
      });
  }

private:
  bool received_{false};
  std::vector<std::int16_t> axes_;
  std::vector<std::uint8_t> buttons_;
  rclcpp::Subscription<rc_handle_msgs::msg::JoyRaw>::SharedPtr subscription_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<JoyRawListener>());
  if (rclcpp::ok()) {
    rclcpp::shutdown();
  }
  return 0;
}
