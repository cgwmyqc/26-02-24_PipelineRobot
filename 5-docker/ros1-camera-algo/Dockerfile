FROM ros:noetic-ros-base-focal

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ffmpeg \
    gstreamer1.0-libav \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-tools \
    gettext-base \
    git \
    libapr1-dev \
    libaprutil1-dev \
    libopencv-dev \
    python3-catkin-tools \
    python3-rosdep \
    ros-noetic-camera-info-manager \
    ros-noetic-catkin \
    ros-noetic-cv-bridge \
    ros-noetic-dynamic-reconfigure \
    ros-noetic-image-transport \
    ros-noetic-pcl-conversions \
    ros-noetic-pcl-ros \
    ros-noetic-rosbag \
    ros-noetic-rosbash \
    ros-noetic-roscpp \
    ros-noetic-rosgraph \
    ros-noetic-roslaunch \
    ros-noetic-roslint \
    ros-noetic-rospy \
    ros-noetic-rviz \
    ros-noetic-sensor-msgs \
    ros-noetic-std-msgs \
    ros-noetic-std-srvs \
    && rm -rf /var/lib/apt/lists/*

RUN rosdep init || true && rosdep update || true

WORKDIR /opt
RUN git clone --depth 1 https://github.com/Livox-SDK/Livox-SDK2.git /opt/Livox-SDK2 && \
    cd /opt/Livox-SDK2 && \
    mkdir -p build && \
    cd build && \
    cmake .. && \
    make -j"$(nproc)" && \
    make install && \
    ldconfig

WORKDIR /opt/camera_ws/src
COPY volumes/ipcamera_driver /opt/camera_ws/src/ipcamera_driver
COPY volumes/livox_ros_driver2 /opt/camera_ws/src/livox_ros_driver2
COPY volumes/ros1_camera_ws/src/project_bringup /opt/camera_ws/src/project_bringup

RUN cp /opt/camera_ws/src/livox_ros_driver2/package_ROS1.xml /opt/camera_ws/src/livox_ros_driver2/package.xml && \
    source /opt/ros/noetic/setup.bash && \
    cd /opt/camera_ws && \
    catkin_init_workspace src && \
    rosdep install --from-paths src --ignore-src -r -y --rosdistro=noetic || true && \
    catkin_make -DROS_EDITION=ROS1

COPY scripts/start.sh /usr/local/bin/start-ros1-camera-algo.sh
RUN chmod +x /usr/local/bin/start-ros1-camera-algo.sh

ENTRYPOINT ["/usr/local/bin/start-ros1-camera-algo.sh"]
