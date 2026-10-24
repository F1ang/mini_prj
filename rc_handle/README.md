# rc_handle

ROS2 Humble 手柄工作空间，读取 `/dev/input/js0`，直接发布和 `jstest`
一致的原始数值。轴保留 **s16 / int16**，按键保留 **u8 / uint8**；
不做归一化、软件死区、符号反转或重新标定。

```text
src/
├── rc_handle/       # Python 手柄驱动、订阅示例和 launch 配置
├── rc_handle_cpp/   # C++17 手柄驱动、订阅示例和 launch 配置
└── rc_handle_msgs/  # 两种实现共用的 JoyRaw：int16[] axes、uint8[] buttons
```

## 编译与启动

### C++ 版本

`rc_handle_cpp` 使用 **C++17 + ament_cmake + rclcpp**，包含发布节点、
C++ 订阅示例、launch 文件和 YAML 配置，并复用 `rc_handle_msgs` 消息包。

终端 1：编译并启动 C++ 发布节点，保持该终端运行。

```bash
source /opt/ros/humble/setup.bash
cd /home/fyp/rc_handle
colcon build --symlink-install --packages-up-to rc_handle_cpp
source install/setup.bash
ros2 launch rc_handle_cpp joy.launch.py
```

终端 2：运行 C++ 订阅示例，轴或按键变化时打印原始整数。

```bash
source /home/fyp/rc_handle/install/setup.bash
ros2 run rc_handle_cpp joy_raw_listener
```

也可以在新终端查看 topic 数据：

```bash
source /home/fyp/rc_handle/install/setup.bash
ros2 topic echo /joy_raw rc_handle_msgs/msg/JoyRaw
```

主要代码：

- [joystick_device.cpp](src/rc_handle_cpp/src/joystick_device.cpp)：读取 Linux 手柄事件，保存 s16/u8 原值。
- [joy_node.cpp](src/rc_handle_cpp/src/joy_node.cpp)：定时发布、断线清零与重连。
- [joy_raw_listener.cpp](src/rc_handle_cpp/src/joy_raw_listener.cpp)：C++ 订阅示例。
- [joy.launch.py](src/rc_handle_cpp/launch/joy.launch.py)：启动入口。

### Python 版本

```bash
source /opt/ros/humble/setup.bash
cd /home/fyp/rc_handle
colcon build --symlink-install --packages-up-to rc_handle
source install/setup.bash
ros2 launch rc_handle joy.launch.py
```

Python 和 C++ 发布节点选择其中一个运行，避免向同一 topic 重复发布。
切换实现时，先在当前发布节点的启动终端按 `Ctrl+C`，再启动另一个实现。
两种实现的消息、topic、原始数值、参数和断线重连行为一致。

| 实现 | ROS2 包 | 发布程序 | 默认发布节点 | 原始数据订阅程序 |
| --- | --- | --- | --- | --- |
| C++ | `rc_handle_cpp` | `joy_node` | `/rc_handle_cpp_node` | `joy_raw_listener` |
| Python | `rc_handle` | `joy_node` | `/rc_handle_node` | `joy_raw_listener` |

在已有工作空间中使用时，把需要的驱动包和 `rc_handle_msgs` 一起放入
该工作空间的 `src/` 目录，使用 `--packages-up-to` 自动编译消息依赖。
C++ 包说明见 [rc_handle_cpp/README.md](src/rc_handle_cpp/README.md)。

## 原始整数 topic：/joy_raw

消息类型 `rc_handle_msgs/msg/JoyRaw`：

```text
std_msgs/Header header
int16[] axes
uint8[] buttons
```

`axes[i]` 是设备返回的有符号 16 位整数，保留 `[-32768, 32767]`
范围内的原值及方向，例如 `-32767`、`1234`、`32767`，不会缩放到 ±1。
`buttons[i]` 是无符号 8 位整数，正常手柄为松开 `0`、按下 `1`。
索引和 `jstest /dev/input/js0` 相同，轴和按键数量由设备自动查询。

新终端查看消息、类型和发布频率：

```bash
source /home/fyp/rc_handle/install/setup.bash
ros2 topic echo /joy_raw rc_handle_msgs/msg/JoyRaw
# 在另一个终端执行：
# ros2 interface show rc_handle_msgs/msg/JoyRaw
# ros2 topic hz /joy_raw
```

## 兼容 topic：/joy

保留 `/joy`，类型为标准 `sensor_msgs/msg/Joy`。
该标准消息的字段类型固定为 `float32[] axes` 和 `int32[] buttons`，
无法直接改成 s16/u8；因此这里只转换存储类型，**数值保持原始量级**，
例如原始 `-32768` 发布为 `-32768.0`。全部 int16 数值都可被 float32 精确表示。
需要严格的 s16/u8 字段类型时订阅 `/joy_raw`。

```bash
ros2 topic echo /joy
```

两个 topic 具有相同的时间戳和索引，每次发布的都是完整状态，默认 50 Hz。
同一读取周期内的短暂按下再松开可能合并为最终状态，可提高 `publish_rate`。

## 其他节点订阅 s16/u8

直接运行原始整数订阅示例：

```bash
source /home/fyp/rc_handle/install/setup.bash
ros2 run rc_handle joy_raw_listener
```

在自己的 Python 节点中：

```python
from rclpy.node import Node
from rc_handle_msgs.msg import JoyRaw


class Controller(Node):
    def __init__(self):
        super().__init__('controller')
        self.joy_sub = self.create_subscription(
            JoyRaw, '/joy_raw', self.on_joy, 10)

    def on_joy(self, msg):
        axis_0 = msg.axes[0] if msg.axes else 0          # 原始 int16
        button_0 = msg.buttons[0] if msg.buttons else 0  # uint8
        self.get_logger().info(f'axis_0={axis_0}, button_0={button_0}')
```

订阅包的 `package.xml` 需声明 `rclpy` 和 `rc_handle_msgs` 依赖。
C++ 使用 `#include "rc_handle_msgs/msg/joy_raw.hpp"` 和
`rc_handle_msgs::msg::JoyRaw`，并通过 `find_package(rc_handle_msgs REQUIRED)`
及 `ament_target_dependencies(your_node rclcpp rc_handle_msgs)` 添加依赖。

包内 C++ 订阅示例可直接运行：

```bash
ros2 run rc_handle_cpp joy_raw_listener
```

完整代码见 [joy_raw_listener.cpp](src/rc_handle_cpp/src/joy_raw_listener.cpp)。
原来的标准 Joy 订阅示例也保留：`ros2 run rc_handle joy_listener`。
完整入口见 [joy_listener.py](src/rc_handle/rc_handle/joy_listener.py)。

## 参数与 topic 重映射

Python 配置：[src/rc_handle/config/joy.yaml](src/rc_handle/config/joy.yaml)。
C++ 配置：[src/rc_handle_cpp/config/joy.yaml](src/rc_handle_cpp/config/joy.yaml)。
参数均在启动时生效，原来的 `deadzone`、`invert_axes` 参数已移除。

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| `dev` | `/dev/input/js0` | 设备路径，也支持 `/dev/input/by-id/*-joystick` 稳定路径 |
| `publish_rate` | `50.0` | 读取/发布频率，范围 `[1.0, 1000.0]` Hz |
| `reconnect_interval` | `1.0` | 断线重试间隔，秒，须大于 0 |
| `frame_id` | `joy` | `header.frame_id` |

C++ 版本直接运行和重映射示例：

```bash
ros2 run rc_handle_cpp joy_node --ros-args \
  -p dev:=/dev/input/js0 -p publish_rate:=50.0

# 或使用 launch 指定配置与 topic：
ros2 launch rc_handle_cpp joy.launch.py \
  params_file:=/home/fyp/rc_handle/src/rc_handle_cpp/config/joy.yaml \
  joy_raw_topic:=/controller/joy_raw joy_topic:=/controller/joy

# 另一个终端运行对应订阅程序：
ros2 run rc_handle_cpp joy_raw_listener --ros-args -r joy_raw:=/controller/joy_raw
```

Python 版本对应示例：

```bash
ros2 run rc_handle joy_node --ros-args \
  -p dev:=/dev/input/js0 -p publish_rate:=50.0

# 或通过 launch 指定配置文件和 topic：
ros2 launch rc_handle joy.launch.py \
  params_file:=/home/fyp/rc_handle/src/rc_handle/config/joy.yaml \
  joy_raw_topic:=/controller/joy_raw joy_topic:=/controller/joy

# 原始整数订阅示例对应的重映射：
ros2 run rc_handle joy_raw_listener --ros-args -r joy_raw:=/controller/joy_raw
```

QoS 为 Reliable / Volatile、深度 10，也兼容 best-effort 的传感器订阅。
拔出或读取错误时，两个 topic 各发布一次同维度全零状态，然后停止发布并
定时重连。全零消息用于清除上次输入，不代表扳机的物理静止值。
本包原样转发 joydev 返回值，内核中已有的设备校准不由本包修改。

## 测试与排查

在工作空间根目录执行：

```bash
cd /home/fyp/rc_handle
source /home/fyp/rc_handle/install/setup.bash
colcon test --packages-select rc_handle --event-handlers console_direct+
# C++ 驱动测试：
colcon test --packages-select rc_handle_cpp --event-handlers console_direct+
colcon test-result --verbose
```

测试涵盖 int16 极值、微小轴值、uint8 值保持、真实 DDS 消息字段类型和
数值、两个 topic 的断线清零与重连。

已完成的 C++ 验证：**17 项 Google Test 用例通过**；本机飞智手柄实测
**8 个轴、20 个按键、50 Hz**，C++ 发布和订阅程序均能传输原始整数。

### topic 未发布或无法确定类型

如果出现：

```text
WARNING: topic [/joy_raw] does not appear to be published yet
Could not determine the type for the passed topic
```

先检查发布节点是否正在运行：

```bash
source /home/fyp/rc_handle/install/setup.bash
ros2 node list
ros2 topic info /joy_raw
```

使用 C++ 版本时，应能看到 `/rc_handle_cpp_node`，且 `/joy_raw` 的
`Publisher count` 为 `1`。没有发布节点时，先运行
`ros2 launch rc_handle_cpp joy.launch.py`，再在另一个终端订阅。
`ros2 topic echo` 只负责订阅，不会启动手柄驱动；显式指定消息类型也不会
自动启动发布节点。关闭发布终端或重启环境后，需要重新启动驱动。

### 设备与权限

```bash
ls -l /dev/input/js* /dev/input/by-id/*-joystick
jstest /dev/input/js0
ros2 topic info /joy_raw --verbose
```

如果提示 `Permission denied`，可执行
`sudo usermod -aG input "$USER"` 后重新登录。

接口依据：[标准 Joy 消息定义](https://github.com/ros2/common_interfaces/blob/humble/sensor_msgs/msg/Joy.msg)、
[Linux joydev 接口](https://docs.kernel.org/input/joydev/joystick-api.html)。
