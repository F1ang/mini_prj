#include "rc_handle_cpp/joystick_device.hpp"

#include <unistd.h>

#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include "gtest/gtest.h"

namespace
{

js_event event(std::int16_t value, std::uint8_t type, std::uint8_t index)
{
  return js_event{100, value, type, index};
}

class RawAxisTest : public ::testing::TestWithParam<std::int16_t> {};

TEST_P(RawAxisTest, PreservesS16WithoutNormalizationDeadzoneOrSignChange)
{
  rc_handle_cpp::JoystickState state;
  state.reset(2, 2);
  state.apply(event(GetParam(), JS_EVENT_AXIS | JS_EVENT_INIT, 1));
  EXPECT_EQ(state.axes[1], GetParam());
  EXPECT_EQ(state.axes[0], 0);
}

INSTANTIATE_TEST_SUITE_P(
  RawValues, RawAxisTest, ::testing::Values(-32768, -32767, -1, 0, 1, 2048, 32767));

class RawButtonTest : public ::testing::TestWithParam<std::int16_t> {};

TEST_P(RawButtonTest, PreservesU8WithoutBooleanConversion)
{
  rc_handle_cpp::JoystickState state;
  state.reset(2, 2);
  state.apply(event(GetParam(), JS_EVENT_BUTTON | JS_EVENT_INIT, 1));
  EXPECT_EQ(state.buttons[1], GetParam());
}

INSTANTIATE_TEST_SUITE_P(RawValues, RawButtonTest, ::testing::Values(0, 1, 2, 255));

TEST(JoystickState, HoldsStateUntilReleaseAndResetsOnReconnect)
{
  rc_handle_cpp::JoystickState state;
  state.reset(2, 2);
  state.apply(event(1, JS_EVENT_BUTTON | JS_EVENT_INIT, 0));
  state.apply(event(-1234, JS_EVENT_AXIS, 1));
  EXPECT_EQ(state.buttons, (std::vector<std::uint8_t>{1, 0}));
  EXPECT_EQ(state.axes, (std::vector<std::int16_t>{0, -1234}));
  state.apply(event(0, JS_EVENT_BUTTON, 0));
  EXPECT_EQ(state.buttons, (std::vector<std::uint8_t>{0, 0}));
  state.reset(3, 1);
  EXPECT_EQ(state.axes, (std::vector<std::int16_t>{0, 0, 0}));
  EXPECT_EQ(state.buttons, (std::vector<std::uint8_t>{0}));
}

TEST(JoystickState, IgnoresUnknownEventsAndOutOfRangeIndexes)
{
  rc_handle_cpp::JoystickState state;
  state.reset(1, 1);
  state.apply(event(123, 0x04, 0));
  state.apply(event(123, JS_EVENT_AXIS, 255));
  state.apply(event(1, JS_EVENT_BUTTON, 255));
  EXPECT_EQ(state.axes, (std::vector<std::int16_t>{0}));
  EXPECT_EQ(state.buttons, (std::vector<std::uint8_t>{0}));
}

TEST(JoystickState, RejectsValuesThatCannotBeRepresentedAsU8)
{
  rc_handle_cpp::JoystickState state;
  state.reset(1, 1);
  EXPECT_THROW(state.apply(event(-1, JS_EVENT_BUTTON, 0)), std::system_error);
  EXPECT_THROW(state.apply(event(256, JS_EVENT_BUTTON, 0)), std::system_error);
  EXPECT_EQ(state.buttons[0], 0);
}

TEST(JoystickDevice, MissingDeviceCanBeRetriedAndClosed)
{
  rc_handle_cpp::JoystickDevice device(
    "/dev/input/rc_handle_cpp_missing_" + std::to_string(::getpid()));
  EXPECT_FALSE(device.is_open());
  EXPECT_THROW(device.open(), std::system_error);
  EXPECT_FALSE(device.is_open());
  EXPECT_THROW(device.open(), std::system_error);
  EXPECT_THROW(device.read_state(), std::system_error);
  device.close();
  device.close();
}

}  // namespace
