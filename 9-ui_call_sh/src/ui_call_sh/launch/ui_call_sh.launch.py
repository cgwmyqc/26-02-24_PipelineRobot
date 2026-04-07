from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    return LaunchDescription([
        Node(
            package='ui_call_sh',
            executable='ui_call_sh_node',
            name='ui_call_sh_node',
            output='screen',
        )
    ])
