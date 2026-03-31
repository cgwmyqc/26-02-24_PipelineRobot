#!/usr/bin/env bash
set -euo pipefail

cleanup() {
  for pid_var in IPCAMERA_PID LIVOX_PID ROSCORE_PID; do
    pid="${!pid_var:-}"
    if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
      kill "${pid}" 2>/dev/null || true
      wait "${pid}" 2>/dev/null || true
    fi
  done
}

wait_for_roscore() {
  for _ in $(seq 1 30); do
    if rosnode list >/dev/null 2>&1; then
      return 0
    fi
    sleep 1
  done
  return 1
}

trap cleanup EXIT INT TERM

set +u
source /opt/ros/noetic/setup.bash
source /opt/camera_ws/devel/setup.bash
set -u

export MID360_IP="${MID360_IP:-192.168.1.20}"
export MID360_HOST_IP="${MID360_HOST_IP:-192.168.1.199}"
export ROS_MASTER_URI="${ROS_MASTER_URI:-http://127.0.0.1:11311}"
unset ROS_IP
export ROS_HOSTNAME="${ROS_HOSTNAME:-127.0.0.1}"
export HIK_RTSP_URL="${HIK_RTSP_URL:-rtsp://test:hitzri123@192.168.1.64:554/Streaming/Channels/101}"
export CAMERA_FRAME_ID="${CAMERA_FRAME_ID:-camera_link}"
export LIVOX_PUBLISH_FREQ="${LIVOX_PUBLISH_FREQ:-10.0}"
export LIVOX_XFER_FORMAT="${LIVOX_XFER_FORMAT:-2}"
export LIVOX_MULTI_TOPIC="${LIVOX_MULTI_TOPIC:-0}"
export LIVOX_FRAME_ID="${LIVOX_FRAME_ID:-livox_frame}"

envsubst '${MID360_IP} ${MID360_HOST_IP}' \
  < /opt/camera_ws/src/project_bringup/config/MID360_config.json.template \
  > /tmp/MID360_config.json

roscore >/tmp/roscore.log 2>&1 &
ROSCORE_PID=$!

if ! wait_for_roscore; then
  echo "roscore failed to start" >&2
  cat /tmp/roscore.log >&2 || true
  exit 1
fi

roslaunch project_bringup ipcamera_hik.launch \
  video_url:="${HIK_RTSP_URL}" \
  frame_id:="${CAMERA_FRAME_ID}" \
  >/tmp/ipcamera_driver.log 2>&1 &
IPCAMERA_PID=$!

roslaunch project_bringup livox_mid360.launch \
  config_path:=/tmp/MID360_config.json \
  publish_freq:="${LIVOX_PUBLISH_FREQ}" \
  xfer_format:="${LIVOX_XFER_FORMAT}" \
  multi_topic:="${LIVOX_MULTI_TOPIC}" \
  msg_frame_id:="${LIVOX_FRAME_ID}" \
  >/tmp/livox_ros_driver2.log 2>&1 &
LIVOX_PID=$!

wait -n "${IPCAMERA_PID}" "${LIVOX_PID}" "${ROSCORE_PID}"
