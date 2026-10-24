"""Read the same Linux joydev interface used by jstest, without extra libraries."""

import errno
import fcntl
import os
import struct


# Linux UAPI: <linux/joystick.h>. Native byte order, fixed-width 8-byte js_event.
JS_EVENT = struct.Struct('=IhBB')
JS_EVENT_BUTTON = 0x01
JS_EVENT_AXIS = 0x02
JS_EVENT_INIT = 0x80
JSIOCGAXES = 0x80016A11
JSIOCGBUTTONS = 0x80016A12
JSIOCGNAME_128 = 0x80806A13


class JoystickDevice:
    """Keep a full state vector, including initial-state events and held buttons."""

    def __init__(self, path):
        self.path = path
        self.name = path
        self.axes = []
        self.buttons = []
        self._fd = None

    @property
    def is_open(self):
        return self._fd is not None

    def open(self):
        """Query vector sizes on every connection; close if initialization fails."""
        self.close()
        fd = os.open(self.path, os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC)
        try:
            axes = bytearray(1)
            buttons = bytearray(1)
            fcntl.ioctl(fd, JSIOCGAXES, axes, True)
            fcntl.ioctl(fd, JSIOCGBUTTONS, buttons, True)
            name = bytearray(128)
            try:
                fcntl.ioctl(fd, JSIOCGNAME_128, name, True)
                self.name = name.split(b'\0', 1)[0].decode('utf-8', errors='replace')
            except OSError:
                self.name = self.path
        except BaseException:
            os.close(fd)
            raise
        self.axes = [0] * axes[0]
        self.buttons = [0] * buttons[0]
        self._fd = fd

    def read_state(self):
        """Drain pending events without blocking; bound work per executor tick."""
        if self._fd is None:
            raise OSError(errno.EBADF, 'joystick is not open')
        # Up to 4096 events per tick, well above the kernel's usual queue size.
        for _ in range(16):
            try:
                data = os.read(self._fd, JS_EVENT.size * 256)
            except BlockingIOError:
                return
            except InterruptedError:
                continue
            if not data:
                raise OSError(errno.ENODEV, 'joystick disconnected')
            if len(data) % JS_EVENT.size:
                raise OSError(errno.EIO, 'incomplete joystick event')
            for _, value, event_type, number in JS_EVENT.iter_unpack(data):
                event_type &= ~JS_EVENT_INIT
                if event_type == JS_EVENT_AXIS and number < len(self.axes):
                    self.axes[number] = value
                elif event_type == JS_EVENT_BUTTON and number < len(self.buttons):
                    if not 0 <= value <= 255:
                        raise OSError(errno.EIO, 'button value does not fit uint8')
                    self.buttons[number] = value

    def close(self):
        """Release the descriptor; it is safe to call this more than once."""
        if self._fd is not None:
            fd, self._fd = self._fd, None
            os.close(fd)
