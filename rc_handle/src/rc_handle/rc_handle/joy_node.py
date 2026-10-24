"""Publish unscaled Linux joystick values with int16 axes and uint8 buttons."""

import math
import sys
import time

import rclpy
from rcl_interfaces.msg import ParameterDescriptor
from rclpy.clock import Clock, ClockType
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from sensor_msgs.msg import Joy
from rc_handle_msgs.msg import JoyRaw # 包+消息类别+消息类型名->对象

from rc_handle.joystick import JoystickDevice


class JoyNode(Node):
    def __init__(self, **kwargs):
        super().__init__('rc_handle_node', **kwargs)
        defaults = {
            'dev': '/dev/input/js0',
            'publish_rate': 50.0,
            'reconnect_interval': 1.0,
            'frame_id': 'joy',
        }
        values = {
            name: self.declare_parameter(
                name, default, ParameterDescriptor(read_only=True)).value
            for name, default in defaults.items()
        }
        if not values['dev']:
            raise ValueError('dev must not be empty')
        rate = values['publish_rate']
        if not math.isfinite(rate) or not 1.0 <= rate <= 1000.0:
            raise ValueError('publish_rate must be finite and in [1.0, 1000.0] Hz')
        interval = values['reconnect_interval']
        if not math.isfinite(interval) or interval <= 0.0:
            raise ValueError('reconnect_interval must be finite and > 0.0 seconds')

        self._device = JoystickDevice(values['dev'])
        self._frame_id = values['frame_id']
        self._reconnect_interval = interval
        self._next_open = 0.0
        self._last_open_error = None
        # Reliable + volatile, depth 10: compatible with ordinary ROS subscribers
        # as well as best-effort sensor-data subscribers.
        self._publisher = self.create_publisher(Joy, 'joy', 10)
        self._raw_publisher = self.create_publisher(JoyRaw, 'joy_raw', 10)
        # Hardware polling and reconnection must continue even if /clock pauses.
        self._poll_clock = Clock(clock_type=ClockType.STEADY_TIME)
        self._timer = self.create_timer(1.0 / rate, self._tick, clock=self._poll_clock)
        self.get_logger().info(
            f'Publishing raw int16/uint8 on {self._raw_publisher.topic_name} '
            f'and unscaled Joy on {self._publisher.topic_name} '
            f'at {rate:g} Hz; device: {values["dev"]}')

    def _tick(self):
        if not self._device.is_open:
            if time.monotonic() < self._next_open:
                return
            self._next_open = time.monotonic() + self._reconnect_interval
            try:
                self._device.open()
            except OSError as exc:
                error = str(exc)
                if error != self._last_open_error:
                    self.get_logger().warning(
                        f'Cannot open {self._device.path}: {exc}; '
                        f'retrying every {self._reconnect_interval:g} s')
                self._last_open_error = error
                return
            self._last_open_error = None
            self.get_logger().info(
                f'Connected: {self._device.name}; '
                f'{len(self._device.axes)} axes, {len(self._device.buttons)} buttons')

        try:
            self._device.read_state()
        except OSError as exc:
            self.get_logger().warning(f'Joystick disconnected/read failed: {exc}')
            self._device.close()
            # Clear held inputs once, then stop publishing until reconnection.
            self._publish(neutral=True)
            self._next_open = time.monotonic() + self._reconnect_interval
            return
        self._publish()

    def _publish(self, neutral=False):
        raw = JoyRaw()
        raw.header.stamp = self.get_clock().now().to_msg()
        raw.header.frame_id = self._frame_id
        raw.axes = [0] * len(self._device.axes) if neutral else self._device.axes
        raw.buttons = [0] * len(self._device.buttons) if neutral else self._device.buttons
        self._raw_publisher.publish(raw)

        # The fixed standard Joy schema uses float32/int32. Every int16 value is
        # exactly representable in float32; convert the type without scaling.
        msg = Joy()
        msg.header = raw.header
        msg.axes = [float(value) for value in raw.axes]
        msg.buttons = list(raw.buttons)
        self._publisher.publish(msg)

    def destroy_node(self):
        self._device.close()
        return super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = None
    try:
        node = JoyNode()
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    except ValueError as exc:
        print(f'Invalid rc_handle parameter: {exc}', file=sys.stderr)
        return 1
    finally:
        if node is not None:
            node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()
    return 0
