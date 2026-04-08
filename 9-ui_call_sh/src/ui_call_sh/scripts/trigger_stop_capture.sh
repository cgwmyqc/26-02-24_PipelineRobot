#!/usr/bin/env bash
source /opt/ros/humble/setup.bash
source /home/hit/sewer_ws/install/setup.bash
ros2 service call /trigger_capture std_srvs/srv/Trigger {}
