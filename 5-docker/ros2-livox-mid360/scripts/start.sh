#!/usr/bin/env bash
set -eo pipefail

set +u
source /opt/ros/humble/setup.bash
source /opt/ws_livox/install/setup.bash
set -u

export LD_LIBRARY_PATH="/usr/local/lib:${LD_LIBRARY_PATH:-}"
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-0}"
export RMW_IMPLEMENTATION="${RMW_IMPLEMENTATION:-rmw_fastrtps_cpp}"

exec ros2 launch livox_ros_driver2 msg_MID360_launch.py
