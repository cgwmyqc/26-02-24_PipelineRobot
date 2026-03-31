# Docker Layout

`5-docker/docker-compose.yml` is the only compose entrypoint for the current runtime stack.

## Services

- `micro_ros_agent`: UDP micro-ROS agent on host port `8888`
- `web_ros2_bridge`: `rosbridge_server` for the web UI on host port `9090`
- `mysql`: MySQL 8.0 on host port `3307`, default app user `dev/123456`

## Runtime Notes

- The ROS1 path has been removed from compose. `ros1_camera_algo` and `ros1_bridge` are no longer started by `docker-compose.yml`.
- Algorithm-side ROS2 development is now expected to run directly on Jetson. This repository no longer maintains a Docker-based ROS2 algorithm container.
- Legacy ROS1-related directories are retained in the repo for reference only:
  - `5-docker/ros1-camera-algo/`
  - `5-docker/ros1-ros2-bridge/`

## Run

```powershell
cd 5-docker
Copy-Item .env.example .env
docker compose up -d --build
```

## Notes

- `compose/docker-compose.dev.yml` is still kept as a legacy rollback file, but it is not the main entrypoint.
- MySQL init scripts under `mysql/init/` only run when the MySQL data volume is empty for the first time.
- A backup of the previous main compose file should be kept under `5-docker/bak/` before further migration edits.
