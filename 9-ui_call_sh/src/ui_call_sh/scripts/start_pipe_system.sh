#!/usr/bin/env bash
set -e

SESSION=pipe_system

tmux has-session -t $SESSION 2>/dev/null && {
  echo "tmux session '$SESSION' already exists."
  echo "Attach with: tmux attach -t $SESSION"
  exit 0
}

tmux new-session -d -s $SESSION

# 窗格 1: livox
tmux send-keys -t $SESSION:0.0 'cd ~/ws_livox' C-m
tmux send-keys -t $SESSION:0.0 'source /opt/ros/humble/setup.bash' C-m
tmux send-keys -t $SESSION:0.0 'source install/setup.bash' C-m
tmux send-keys -t $SESSION:0.0 'ros2 launch livox_ros_driver2 rviz_MID360_launch.py' C-m

# 分屏
tmux split-window -h -t $SESSION:0
tmux split-window -v -t $SESSION:0.1
tmux split-window -v -t $SESSION:0.0
tmux split-window -v -t $SESSION:0.2

# 窗格 2: ipcamera
tmux send-keys -t $SESSION:0.1 'source /opt/ros/humble/setup.bash' C-m
tmux send-keys -t $SESSION:0.1 'source ~/ros2_cam_ws/install/setup.bash' C-m
tmux send-keys -t $SESSION:0.1 'export OPENCV_FFMPEG_CAPTURE_OPTIONS="rtsp_transport;tcp|stimeout;5000000"' C-m
tmux send-keys -t $SESSION:0.1 'ros2 launch ros2_ipcamera ipcamera.launch.py' C-m

# 窗格 3: rqt_image_view
tmux send-keys -t $SESSION:0.2 'source /opt/ros/humble/setup.bash' C-m
tmux send-keys -t $SESSION:0.2 'ros2 run rqt_image_view rqt_image_view' C-m

# 窗格 4: topic list
tmux send-keys -t $SESSION:0.3 'source /opt/ros/humble/setup.bash' C-m
tmux send-keys -t $SESSION:0.3 'watch -n 1 ros2 topic list' C-m

# 窗格 5: stop_capture
tmux send-keys -t $SESSION:0.4 'source /opt/ros/humble/setup.bash' C-m
tmux send-keys -t $SESSION:0.4 'source /home/hit/sewer_ws/install/setup.bash' C-m
tmux send-keys -t $SESSION:0.4 "ros2 run pipe_stop_capture stop_capture --ros-args -p cloud_topic:=/livox/lidar -p image_topic:=/ipcamera/image_raw -p imu_topic:=/livox/imu -p data_root:=/home/hit/pipe_dataset -p min_clouds:=1 -p min_images:=2 -p flush_sec:=0.8 -p capture_sec:=1.5 -p gyro_thresh_dps:=0.0" C-m

tmux select-layout -t $SESSION tiled
tmux attach -t $SESSION
