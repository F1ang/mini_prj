#ifndef RC_HANDLE_CPP__JOYSTICK_DEVICE_HPP_
#define RC_HANDLE_CPP__JOYSTICK_DEVICE_HPP_

#include <linux/joystick.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rc_handle_cpp
{

struct JoystickState
{
  std::vector<std::int16_t> axes;
  std::vector<std::uint8_t> buttons;

  void reset(std::size_t axis_count, std::size_t button_count);
  void apply(const js_event & event);
};

class JoystickDevice
{
public:
  explicit JoystickDevice(std::string path);
  virtual ~JoystickDevice();

  JoystickDevice(const JoystickDevice &) = delete;
  JoystickDevice & operator=(const JoystickDevice &) = delete;

  virtual void open();
  virtual void read_state();
  virtual void close() noexcept;
  virtual bool is_open() const noexcept;

  const JoystickState & state() const noexcept {return state_;}
  const std::string & path() const noexcept {return path_;}
  const std::string & name() const noexcept {return name_;}

protected:
  JoystickState state_;

private:
  std::string path_;
  std::string name_;
  int fd_{-1};
};

}  // namespace rc_handle_cpp

#endif  // RC_HANDLE_CPP__JOYSTICK_DEVICE_HPP_
