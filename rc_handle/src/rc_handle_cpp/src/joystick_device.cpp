#include "rc_handle_cpp/joystick_device.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <system_error>
#include <utility>

namespace rc_handle_cpp
{

static_assert(sizeof(js_event) == 8, "Unexpected Linux joystick event layout");

void JoystickState::reset(std::size_t axis_count, std::size_t button_count)
{
  axes.assign(axis_count, 0);
  buttons.assign(button_count, 0);
}

void JoystickState::apply(const js_event & event)
{
  const auto type = event.type & ~JS_EVENT_INIT;
  if (type == JS_EVENT_AXIS && event.number < axes.size()) {
    axes[event.number] = event.value;
  } else if (type == JS_EVENT_BUTTON && event.number < buttons.size()) {
    if (event.value < 0 || event.value > 255) {
      throw std::system_error(EIO, std::generic_category(), "button value does not fit uint8");
    }
    buttons[event.number] = static_cast<std::uint8_t>(event.value);
  }
}

JoystickDevice::JoystickDevice(std::string path)
: path_(std::move(path)), name_(path_)
{
}

JoystickDevice::~JoystickDevice()
{
  close();
}

void JoystickDevice::open()
{
  close();
  const int descriptor = ::open(path_.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (descriptor < 0) {
    throw std::system_error(errno, std::generic_category(), "open " + path_);
  }
  try {
    std::uint8_t axis_count = 0;
    std::uint8_t button_count = 0;
    if (::ioctl(descriptor, JSIOCGAXES, &axis_count) < 0 ||
      ::ioctl(descriptor, JSIOCGBUTTONS, &button_count) < 0)
    {
      throw std::system_error(errno, std::generic_category(), "query joystick dimensions");
    }
    std::array<char, 128> name{};
    if (::ioctl(descriptor, JSIOCGNAME(name.size()), name.data()) >= 0) {
      name.back() = '\0';
      name_ = name.data();
    } else {
      name_ = path_;
    }
    state_.reset(axis_count, button_count);
  } catch (...) {
    ::close(descriptor);
    throw;
  }
  fd_ = descriptor;
}

void JoystickDevice::read_state()
{
  if (fd_ < 0) {
    throw std::system_error(EBADF, std::generic_category(), "joystick is not open");
  }
  // Bound work to 4096 events per timer tick so ROS callbacks remain responsive.
  std::array<js_event, 256> events{};
  for (int batch = 0; batch < 16; ++batch) {
    const auto bytes = ::read(fd_, events.data(), sizeof(events));
    if (bytes < 0) {
      const int error = errno;
      if (error == EAGAIN || error == EWOULDBLOCK) {
        return;
      }
      if (error == EINTR) {
        continue;
      }
      throw std::system_error(error, std::generic_category(), "read joystick");
    }
    if (bytes == 0) {
      throw std::system_error(ENODEV, std::generic_category(), "joystick disconnected");
    }
    if (bytes % sizeof(js_event) != 0) {
      throw std::system_error(EIO, std::generic_category(), "incomplete joystick event");
    }
    const auto count = static_cast<std::size_t>(bytes) / sizeof(js_event);
    for (std::size_t index = 0; index < count; ++index) {
      state_.apply(events[index]);
    }
  }
}

void JoystickDevice::close() noexcept
{
  if (fd_ >= 0) {
    const int descriptor = fd_;
    fd_ = -1;
    ::close(descriptor);
  }
}

bool JoystickDevice::is_open() const noexcept
{
  return fd_ >= 0;
}

}  // namespace rc_handle_cpp
