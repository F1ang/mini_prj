"""Start the joystick publisher with a YAML configuration and topic remapping."""

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
                FindPackageShare('rc_handle'), 'config', 'joy.yaml']),
            description='Path to the joystick parameter YAML file'),
        DeclareLaunchArgument(
            'joy_topic', default_value='joy', description='Output topic name'),
        DeclareLaunchArgument(
            'joy_raw_topic', default_value='joy_raw',
            description='Output topic for int16 axes and uint8 buttons'),
        Node(
            package='rc_handle',
            executable='joy_node',
            name='rc_handle_node',
            output='screen',
            parameters=[LaunchConfiguration('params_file')],
            remappings=[
                ('joy', LaunchConfiguration('joy_topic')),
                ('joy_raw', LaunchConfiguration('joy_raw_topic')),
            ],
        ),
    ])
