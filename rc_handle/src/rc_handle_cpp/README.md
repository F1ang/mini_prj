# rc_handle_cpp

Linux / ROS2 Humble 的 C++17 手柄驱动包，直接读取 `/dev/input/js0`。
运行时不需要 Python 驱动，复用 `rc_handle_msgs` 消息包。

在工作空间根目录编译：

```bash
source /opt/ros/humble/setup.bash
colcon build --symlink-install --packages-up-to rc_handle_cpp
source install/setup.bash
ros2 launch rc_handle_cpp joy.launch.py
```

同一手柄的 Python 和 C++ 发布节点选择其中一个运行，避免重复发布。

- `/joy_raw`：`rc_handle_msgs/msg/JoyRaw`，`int16[] axes`、`uint8[] buttons`。
- `/joy`：`sensor_msgs/msg/Joy`，保留原始数值，字段按标准消息转换为 float32/int32。

轴值原样保留，不做归一化、死区、截断或符号反转。索引与 `jstest` 一致。
参数为 `dev`、`publish_rate`、`reconnect_interval`、`frame_id`，
默认读取 `/dev/input/js0`，以 50 Hz 发布，断线后每 1 秒尝试重连。
断线时两个 topic 各发一次全零状态，然后停止发布，直到恢复连接。

直接启动及订阅示例：

```bash
ros2 run rc_handle_cpp joy_node --ros-args -p dev:=/dev/input/js0 -p publish_rate:=50.0
# 另一个终端 source install/setup.bash 后：
ros2 run rc_handle_cpp joy_raw_listener
```

修改 topic：

```bash
ros2 launch rc_handle_cpp joy.launch.py \
  joy_raw_topic:=/controller/joy_raw joy_topic:=/controller/joy
ros2 run rc_handle_cpp joy_raw_listener --ros-args -r joy_raw:=/controller/joy_raw
```

配置见 `config/joy.yaml`，可通过 launch 的 `params_file` 指定其他 YAML。
订阅程序见 `src/joy_raw_listener.cpp`；字段分别对应
`std::vector<int16_t>`、`std::vector<uint8_t>`。

测试：

```bash
colcon test --packages-select rc_handle_cpp --event-handlers console_direct+
colcon test-result --verbose
```
