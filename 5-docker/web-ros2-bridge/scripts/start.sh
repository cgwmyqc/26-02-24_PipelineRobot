#!/usr/bin/env bash
set -eo pipefail

set +u
source /opt/ros/humble/setup.bash
set -u

exec ros2 launch rosbridge_server rosbridge_websocket_launch.xml port:="${ROSBRIDGE_PORT:-9090}"
