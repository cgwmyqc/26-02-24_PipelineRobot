FROM ros:noetic-ros-base-focal

SHELL ["/bin/bash", "-c"]

COPY scripts/start.sh /usr/local/bin/start-ros1-camera-algo.sh

RUN chmod +x /usr/local/bin/start-ros1-camera-algo.sh

ENTRYPOINT ["/usr/local/bin/start-ros1-camera-algo.sh"]
