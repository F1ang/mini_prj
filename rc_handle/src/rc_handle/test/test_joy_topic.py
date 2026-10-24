"""Use real ROS serialization/DDS to check publishing and reconnect behavior."""

import errno
import time
from uuid import uuid4

import rclpy
from rclpy.context import Context
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from rclpy.parameter import Parameter
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Joy
from rc_handle_msgs.msg import JoyRaw

from rc_handle import joy_node


class ScriptedDevice:
    def __init__(self, path):
        self.path = path
        self.name = 'Scripted joystick'
        self.axes = []
        self.buttons = []
        self.is_open = False
        self.available = True
        self.open_count = 0

    def open(self):
        if not self.available:
            raise OSError(errno.ENOENT, 'not connected')
        self.is_open = True
        self.open_count += 1
        self.axes = [-32768, 32767, 1, -1]
        self.buttons = [1, 255]

    def read_state(self):
        if not self.available:
            raise OSError(errno.ENODEV, 'unplugged')

    def close(self):
        self.is_open = False


def test_raw_types_values_joy_compatibility_disconnect_and_reconnect(monkeypatch):
    monkeypatch.setattr(joy_node, 'JoystickDevice', ScriptedDevice)
    context = Context()
    rclpy.init(context=context)
    namespace = '/rc_handle_test_' + uuid4().hex
    driver = None
    subscriber = None
    executor = SingleThreadedExecutor(context=context)
    reliable_messages = []
    sensor_messages = []
    raw_messages = []

    def wait_until(condition, timeout=5.0):
        deadline = time.monotonic() + timeout
        while not condition() and time.monotonic() < deadline:
            executor.spin_once(timeout_sec=0.02)
        assert condition(), 'Timed out waiting for a Joy message'

    try:
        driver = joy_node.JoyNode(
            context=context, namespace=namespace,
            parameter_overrides=[
                Parameter('publish_rate', value=100.0),
                Parameter('reconnect_interval', value=0.05),
                Parameter('frame_id', value='test_joy'),
            ])
        subscriber = Node('test_subscriber', context=context, namespace=namespace)
        reliable_sub = subscriber.create_subscription(
            Joy, 'joy', reliable_messages.append, 10)
        sensor_sub = subscriber.create_subscription(
            Joy, 'joy', sensor_messages.append, qos_profile_sensor_data)
        raw_sub = subscriber.create_subscription(
            JoyRaw, 'joy_raw', raw_messages.append, 10)
        executor.add_node(driver)
        executor.add_node(subscriber)

        wait_until(lambda: len(reliable_messages) >= 3 and len(sensor_messages) >= 3
                   and len(raw_messages) >= 3)
        msg = reliable_messages[-1]
        assert list(msg.axes) == [-32768.0, 32767.0, 1.0, -1.0]
        assert list(msg.buttons) == [1, 255]
        raw = raw_messages[-1]
        assert list(raw.axes) == [-32768, 32767, 1, -1]
        assert list(raw.buttons) == [1, 255]
        assert raw.axes.typecode == 'h'
        assert raw.buttons.typecode == 'B'
        assert raw.axes.itemsize == 2
        assert raw.buttons.itemsize == 1
        assert raw.header.frame_id == 'test_joy'
        assert msg.header.frame_id == 'test_joy'
        assert msg.header.stamp.sec > 0
        assert reliable_messages[-1].header.stamp != reliable_messages[0].header.stamp

        driver._device.available = False
        wait_until(lambda: list(reliable_messages[-1].buttons) == [0, 0]
                   and list(raw_messages[-1].buttons) == [0, 0])
        assert list(reliable_messages[-1].axes) == [0.0] * 4
        assert list(raw_messages[-1].axes) == [0] * 4
        # After the one clearing message, stale held inputs must not repeat.
        deadline = time.monotonic() + 0.15
        while time.monotonic() < deadline:
            executor.spin_once(timeout_sec=0.01)
        count = len(reliable_messages)
        raw_count = len(raw_messages)
        for _ in range(10):
            executor.spin_once(timeout_sec=0.01)
        assert len(reliable_messages) == count
        assert len(raw_messages) == raw_count

        driver._device.available = True
        wait_until(lambda: list(reliable_messages[-1].buttons) == [1, 255]
                   and list(raw_messages[-1].buttons) == [1, 255])
        assert driver._device.open_count == 2
        assert list(reliable_messages[-1].axes) == [-32768.0, 32767.0, 1.0, -1.0]
        assert list(raw_messages[-1].axes) == [-32768, 32767, 1, -1]
        # Keep explicit references alive for the duration of the DDS test.
        assert reliable_sub is not None and sensor_sub is not None and raw_sub is not None
    finally:
        executor.shutdown()
        if subscriber is not None:
            subscriber.destroy_node()
        if driver is not None:
            driver.destroy_node()
            assert not driver._device.is_open
        context.shutdown()
