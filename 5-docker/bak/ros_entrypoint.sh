#!/bin/bash
set -e

source /opt/ros/noetic/setup.bash
source /opt/ros2_humble_ws/install/setup.bash
source /opt/bridge_ws/install/setup.bash

export ROS_MASTER_URI=${ROS_MASTER_URI:-http://127.0.0.1:11311}
export ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-0}
export RMW_IMPLEMENTATION=${RMW_IMPLEMENTATION:-rmw_fastrtps_cpp}

exec "$@"
