# Docker Layout

`5-docker/docker-compose.yml` is the only compose entrypoint for the current runtime stack.

## Services

- `micro_ros_agent`: UDP micro-ROS agent on host port `8888`
- `web_ros2_bridge`: `rosbridge_server` for the web UI on host port `9090`
- `ros2_livox_mid360`: ROS2 Humble Livox MID360 driver, publishing `sensor_msgs/msg/PointCloud2` in Livox `PointXYZRTLT` layout
- `mysql`: MySQL 8.0 on host port `3307`, default app user `dev/123456`

## Runtime Notes

- The ROS1 path has been removed from compose. `ros1_camera_algo` and `ros1_bridge` are no longer started by `docker-compose.yml`.
- Algorithm-side ROS2 development is now expected to run directly on Jetson. This repository no longer maintains a Docker-based ROS2 algorithm container.
- `ros2_livox_mid360` uses `network_mode: host`. Docker Desktop host networking must be enabled before starting the stack.
- The current MID360 config assumes:
  - LiDAR IP: `192.168.1.20`
  - Driver bind IP in container: `0.0.0.0`
  - Physical host NIC on the LiDAR subnet: `192.168.1.132`
  - Point cloud transfer format: `xfer_format=0` (`PointCloud2 / PointXYZRTLT`)
- The only files you should edit for MID360 are:
  - `ros2-livox-mid360/config/MID360_config.json`
  - `ros2-livox-mid360/launch/msg_MID360_launch.py`
- These two files are copied into `/opt/ws_livox/src/livox_ros_driver2/...` during image build, then `./build.sh humble` updates `/opt/ws_livox/install/...`.
- Legacy ROS1-related directories are retained in the repo for reference only:
  - `5-docker/ros1-camera-algo/`
  - `5-docker/ros1-ros2-bridge/`

## Run

```powershell
cd 5-docker
Copy-Item .env.example .env
docker compose up -d --build
```

To start only the MID360 driver:

```powershell
docker compose up -d --build ros2_livox_mid360
```

To inspect the driver logs:

```powershell
docker compose logs -f ros2_livox_mid360
```

To verify the source/install files inside the container:

```powershell
docker exec -it ros2_livox_mid360 bash
cat /opt/ws_livox/src/livox_ros_driver2/config/MID360_config.json
cat /opt/ws_livox/install/livox_ros_driver2/share/livox_ros_driver2/config/MID360_config.json
cat /opt/ws_livox/src/livox_ros_driver2/launch_ROS2/msg_MID360_launch.py
cat /opt/ws_livox/install/livox_ros_driver2/share/livox_ros_driver2/launch_ROS2/msg_MID360_launch.py
```

## Notes

- `compose/docker-compose.dev.yml` is still kept as a legacy rollback file, but it is not the main entrypoint.
- MySQL init scripts under `mysql/init/` only run when the MySQL data volume is empty for the first time.
- A backup of the previous main compose file should be kept under `5-docker/bak/` before further migration edits.
- On Docker Desktop host networking, binding directly to the Windows NIC IP may fail inside the Linux container. The current config binds UDP sockets to `0.0.0.0` and relies on the host routing table to reach the MID360 subnet.
