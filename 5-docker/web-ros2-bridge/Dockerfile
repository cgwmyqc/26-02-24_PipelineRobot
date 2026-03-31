FROM ros:humble-ros-base-jammy

SHELL ["/bin/bash", "-c"]

RUN apt-get update \
    && apt-get install -y --no-install-recommends ros-humble-rosbridge-server \
    && rm -rf /var/lib/apt/lists/*

COPY scripts/start.sh /usr/local/bin/start-web-ros2-bridge.sh

RUN sed -i 's/\r$//' /usr/local/bin/start-web-ros2-bridge.sh \
    && chmod +x /usr/local/bin/start-web-ros2-bridge.sh

ENTRYPOINT ["/usr/local/bin/start-web-ros2-bridge.sh"]
