FROM ros:noetic-ros-base-focal

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]

RUN apt-get update && apt-get install -y \
    locales tzdata curl gnupg2 lsb-release ca-certificates \
    build-essential cmake git wget vim \
    python3-pip python3-rosdep python3-argcomplete \
    python3-yaml python3-empy python3-nose python3-setuptools \
    libasio-dev libtinyxml2-dev libtinyxml-dev \
    && rm -rf /var/lib/apt/lists/*

RUN locale-gen en_US en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

RUN rosdep init || true
RUN rosdep update

RUN python3 -m pip install --no-cache-dir -U \
    vcstool \
    colcon-common-extensions

WORKDIR /opt
RUN mkdir -p /opt/ros2_humble_ws/src
WORKDIR /opt/ros2_humble_ws

RUN wget https://raw.githubusercontent.com/ros2/ros2/humble/ros2.repos
RUN vcs import src < ros2.repos

RUN source /opt/ros/noetic/setup.bash && \
    apt-get update && \
    rosdep install --from-paths src --ignore-src -r -y \
      --skip-keys "fastcdr rti-connext-dds-6.0.1 urdfdom_headers" && \
    rm -rf /var/lib/apt/lists/*

RUN source /opt/ros/noetic/setup.bash && \
    colcon build \
      --symlink-install \
      --cmake-args -DCMAKE_BUILD_TYPE=Release

RUN mkdir -p /opt/bridge_ws/src
WORKDIR /opt/bridge_ws/src
RUN git clone https://github.com/ros2/ros1_bridge.git

WORKDIR /opt/bridge_ws

RUN source /opt/ros/noetic/setup.bash && \
    source /opt/ros2_humble_ws/install/setup.bash && \
    rosdep install --from-paths src --ignore-src -r -y && \
    colcon build \
      --symlink-install \
      --packages-select ros1_bridge \
      --cmake-force-configure \
      --cmake-args -DCMAKE_BUILD_TYPE=Release

RUN echo "source /opt/ros/noetic/setup.bash" >> /root/.bashrc && \
    echo "source /opt/ros2_humble_ws/install/setup.bash" >> /root/.bashrc && \
    echo "if [ -f /opt/bridge_ws/install/setup.bash ]; then source /opt/bridge_ws/install/setup.bash; fi" >> /root/.bashrc

COPY scripts/start.sh /usr/local/bin/start-ros1-ros2-bridge.sh
RUN chmod +x /usr/local/bin/start-ros1-ros2-bridge.sh

ENV ROS_MASTER_URI=http://ros1_camera_algo:11311
ENV ROS_DOMAIN_ID=0
ENV RMW_IMPLEMENTATION=rmw_fastrtps_cpp
ENV ROS1_INSTALL_PATH=/opt/ros/noetic
ENV ROS2_INSTALL_PATH=/opt/ros2_humble_ws/install

ENTRYPOINT ["/usr/local/bin/start-ros1-ros2-bridge.sh"]
