"""Examples of subscribing to unscaled Joy or typed int16/uint8 JoyRaw data."""

import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from sensor_msgs.msg import Joy
from rc_handle_msgs.msg import JoyRaw


class JoyListener(Node):
    def __init__(self, raw=False):
        super().__init__('joy_raw_listener' if raw else 'joy_listener')
        self._last_state = None
        self._subscription = self.create_subscription(
            JoyRaw if raw else Joy, 'joy_raw' if raw else 'joy', self._on_joy, 10)

    def _on_joy(self, msg):
        state = (tuple(msg.axes), tuple(msg.buttons))
        if state != self._last_state:
            self.get_logger().info(
                f'axes={list(msg.axes)} buttons={list(msg.buttons)}')
            self._last_state = state


def _run(args=None, raw=False):
    rclpy.init(args=args)
    node = JoyListener(raw=raw)
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


def main(args=None):
    _run(args=args)


def raw_main(args=None):
    _run(args=args, raw=True)
