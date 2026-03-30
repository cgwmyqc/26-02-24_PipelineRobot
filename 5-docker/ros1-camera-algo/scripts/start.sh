#!/usr/bin/env bash
set -euo pipefail

cleanup() {
  if [[ -n "${ROSCORE_PID:-}" ]] && kill -0 "${ROSCORE_PID}" 2>/dev/null; then
    kill "${ROSCORE_PID}" 2>/dev/null || true
    wait "${ROSCORE_PID}" 2>/dev/null || true
  fi
}

trap cleanup EXIT INT TERM

set +u
source /opt/ros/noetic/setup.bash
set -u

export ROS_MASTER_URI="${ROS_MASTER_URI:-http://ros1_camera_algo:11311}"
export ROS_HOSTNAME="${ROS_HOSTNAME:-ros1_camera_algo}"

if [[ -f /opt/camera_ws/devel/setup.bash ]]; then
  set +u
  source /opt/camera_ws/devel/setup.bash
  set -u
fi

roscore >/tmp/roscore.log 2>&1 &
ROSCORE_PID=$!

for _ in $(seq 1 20); do
  if grep -q "started core service \[/rosout\]" /tmp/roscore.log 2>/dev/null; then
    break
  fi
  sleep 1
done

if [[ -n "${CAMERA_ALGO_START_COMMAND:-}" ]]; then
  bash -lc "${CAMERA_ALGO_START_COMMAND}"
else
  wait "${ROSCORE_PID}"
fi
