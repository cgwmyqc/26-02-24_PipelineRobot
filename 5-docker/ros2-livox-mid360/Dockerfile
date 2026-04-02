FROM ros:humble-ros-base-jammy

SHELL ["/bin/bash", "-c"]

ENV DEBIAN_FRONTEND=noninteractive
ENV ROS_DISTRO=humble
ENV LIVOX_SDK2_REPO=https://github.com/Livox-SDK/Livox-SDK2.git
ENV LIVOX_ROS_DRIVER2_REPO=https://github.com/Livox-SDK/livox_ros_driver2.git
ENV WS_LIVOX=/opt/ws_livox
ENV LD_LIBRARY_PATH=/usr/local/lib:${LD_LIBRARY_PATH}

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    curl \
    python3-pip \
    python3-colcon-common-extensions \
    python3-rosdep \
    python3-vcstool \
    libyaml-cpp-dev \
    libpcl-dev \
    ros-humble-pcl-conversions \
    ros-humble-tf2 \
    ros-humble-tf2-ros \
    ros-humble-sensor-msgs \
    ros-humble-std-msgs \
    ros-humble-geometry-msgs \
    ros-humble-launch-ros \
    && rm -rf /var/lib/apt/lists/*

RUN rosdep init || true && rosdep update

WORKDIR /opt
RUN git clone --depth 1 ${LIVOX_SDK2_REPO} Livox-SDK2 \
    && cd Livox-SDK2 \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make -j"$(nproc)" \
    && make install \
    && ldconfig

RUN mkdir -p ${WS_LIVOX}/src
WORKDIR ${WS_LIVOX}/src
RUN git clone --depth 1 ${LIVOX_ROS_DRIVER2_REPO} livox_ros_driver2

COPY config/MID360_config.json ${WS_LIVOX}/src/livox_ros_driver2/config/MID360_config.json
COPY launch/msg_MID360_launch.py ${WS_LIVOX}/src/livox_ros_driver2/launch_ROS2/msg_MID360_launch.py

WORKDIR ${WS_LIVOX}
RUN source /opt/ros/${ROS_DISTRO}/setup.bash \
    && rosdep install --from-paths src --ignore-src -r -y \
    && cd src/livox_ros_driver2 \
    && ./build.sh humble

COPY scripts/start.sh /usr/local/bin/start-ros2-livox-mid360.sh

RUN sed -i 's/\r$//' /usr/local/bin/start-ros2-livox-mid360.sh \
    && sed -i 's/\r$//' ${WS_LIVOX}/src/livox_ros_driver2/launch_ROS2/msg_MID360_launch.py \
    && chmod +x /usr/local/bin/start-ros2-livox-mid360.sh \
    && echo 'source /opt/ros/humble/setup.bash' >> /root/.bashrc \
    && echo 'source /opt/ws_livox/install/setup.bash' >> /root/.bashrc \
    && echo 'export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH' >> /root/.bashrc

ENTRYPOINT ["/usr/local/bin/start-ros2-livox-mid360.sh"]
