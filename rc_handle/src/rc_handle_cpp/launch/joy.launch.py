"""Start the C++ raw joystick driver."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'params_file',
            default_value=PathJoinSubstitution([
                FindPackageShare('rc_handle_cpp'), 'config', 'joy.yaml']),
            description='Path to the joystick parameter YAML file'),
        DeclareLaunchArgument(
            'joy_topic', default_value='joy', description='Standard Joy output topic'),
        DeclareLaunchArgument(
            'joy_raw_topic', default_value='joy_raw', description='Raw int16/uint8 output topic'),
        Node(
            package='rc_handle_cpp', executable='joy_node', name='rc_handle_cpp_node',
            output='screen', parameters=[LaunchConfiguration('params_file')],
            remappings=[
                ('joy', LaunchConfiguration('joy_topic')),
                ('joy_raw', LaunchConfiguration('joy_raw_topic')),
            ],
        ),
    ])
