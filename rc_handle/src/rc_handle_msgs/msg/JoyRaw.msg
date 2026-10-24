# Unscaled joystick state, with the same axis/button indexes as jstest.
std_msgs/Header header

# Original signed 16-bit axis values from /dev/input/js*, without software
# deadzone, sign inversion, clipping, or normalization.
int16[] axes

# Unsigned 8-bit button values (normally 0: released, 1: pressed).
uint8[] buttons
