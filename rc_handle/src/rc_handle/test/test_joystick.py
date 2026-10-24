"""Verify unchanged kernel values, the event protocol, and descriptor cleanup."""

import errno
import os
from types import SimpleNamespace

import pytest

from rc_handle import joystick


@pytest.fixture
def fake_kernel(monkeypatch):
    state = SimpleNamespace(closed=[], flags=None, reads=[], ioctl_error=False)

    def open_device(path, flags):
        state.flags = flags
        return 23

    def ioctl(fd, request, buffer, mutate):
        if state.ioctl_error:
            raise OSError(errno.ENODEV, 'gone during open')
        if request == joystick.JSIOCGAXES:
            buffer[0] = 3
        elif request == joystick.JSIOCGBUTTONS:
            buffer[0] = 2
        else:
            name = b'Test controller\0'
            buffer[:len(name)] = name

    def read(fd, size):
        if not state.reads:
            raise BlockingIOError(errno.EAGAIN, 'queue drained')
        item = state.reads.pop(0)
        if isinstance(item, BaseException):
            raise item
        return item

    # Replace this module's OS adapter without altering pytest's own os calls.
    monkeypatch.setattr(joystick, 'os', SimpleNamespace(
        open=open_device, read=read, close=state.closed.append,
        O_RDONLY=os.O_RDONLY, O_NONBLOCK=os.O_NONBLOCK, O_CLOEXEC=os.O_CLOEXEC))
    monkeypatch.setattr(joystick, 'fcntl', SimpleNamespace(ioctl=ioctl))
    return state


def event(value, kind, number):
    return joystick.JS_EVENT.pack(100, value, kind, number)


@pytest.mark.parametrize('raw', [-32768, -32767, -1, 0, 1, 2048, 32767])
def test_axis_s16_is_preserved_exactly(fake_kernel, raw):
    device = joystick.JoystickDevice('/dev/input/js0')
    try:
        device.open()
        fake_kernel.reads = [event(raw, 0x82, 0)]
        device.read_state()
        assert device.axes[0] == raw
        assert type(device.axes[0]) is int
    finally:
        device.close()


@pytest.mark.parametrize('raw', [0, 1, 2, 255])
def test_button_u8_is_preserved_without_boolean_conversion(fake_kernel, raw):
    device = joystick.JoystickDevice('/dev/input/js0')
    try:
        device.open()
        fake_kernel.reads = [event(raw, 0x81, 0)]
        device.read_state()
        assert device.buttons[0] == raw
    finally:
        device.close()


def test_initial_events_full_vectors_and_held_state(fake_kernel):
    device = joystick.JoystickDevice('/dev/input/js0')
    try:
        device.open()
        assert device.name == 'Test controller'
        assert fake_kernel.flags & os.O_NONBLOCK
        fake_kernel.reads = [
            event(-32767, 0x82, 0) + event(1, 0x81, 1),
            event(32767, 0x02, 2),
        ]
        device.read_state()
        assert device.axes == [-32767, 0, 32767]
        assert device.buttons == [0, 1]
        device.read_state()  # No events must preserve a held button.
        assert device.buttons == [0, 1]
        fake_kernel.reads = [event(0, 0x01, 1)]
        device.read_state()
        assert device.buttons == [0, 0]
    finally:
        device.close()
        device.close()
    assert fake_kernel.closed == [23]


def test_reconnect_resets_state_and_ignores_invalid_indexes(fake_kernel):
    device = joystick.JoystickDevice('/dev/input/js0')
    try:
        device.open()
        fake_kernel.reads = [event(1, 0x01, 0) + event(32767, 0x02, 99)
                             + event(1, 0x01, 99) + event(1, 0x04, 0)]
        device.read_state()
        assert device.axes == [0, 0, 0]
        assert device.buttons == [1, 0]
        device.open()
        assert device.buttons == [0, 0]
    finally:
        device.close()
    assert fake_kernel.closed == [23, 23]


@pytest.mark.parametrize('result, expected_errno', [
    (b'', errno.ENODEV),
    (b'bad', errno.EIO),
    (OSError(errno.ENODEV, 'unplugged'), errno.ENODEV),
])
def test_read_errors_are_reported(fake_kernel, result, expected_errno):
    device = joystick.JoystickDevice('/dev/input/js0')
    try:
        device.open()
        fake_kernel.reads = [result]
        with pytest.raises(OSError) as error:
            device.read_state()
        assert error.value.errno == expected_errno
    finally:
        device.close()


def test_open_failure_closes_descriptor(fake_kernel):
    fake_kernel.ioctl_error = True
    device = joystick.JoystickDevice('/dev/input/js0')
    with pytest.raises(OSError):
        device.open()
    assert not device.is_open
    assert fake_kernel.closed == [23]
