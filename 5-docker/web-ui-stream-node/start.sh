#!/usr/bin/env bash
set -eo pipefail

export AMENT_TRACE_SETUP_FILES="${AMENT_TRACE_SETUP_FILES-}"
source /opt/ros/humble/setup.bash
set -u

python3 "$(cd "$(dirname "$0")" && pwd)/ui_stream_bridge.py"
