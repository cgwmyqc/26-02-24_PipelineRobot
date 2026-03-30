# Docker Layout

`5-docker/docker-compose.yml` is the only compose entrypoint for the project runtime stack in this repo.

## Services

- `micro_ros_agent`: UDP micro-ROS agent on host port `8888`
- `web_ros2_bridge`: `rosbridge_server` for the web UI on host port `9090`
- `mysql`: MySQL 8.0 on host port `3307`, default app user `dev/123456`
- `ros1_camera_algo`: Ubuntu 20.04 + ROS1 Noetic camera algorithm base image with `/opt/camera_ws` mount
- `ros1_ros2_bridge`: Ubuntu 20.04 bridge image that builds ROS2 Humble and `ros1_bridge` from source on Focal

## Run

```powershell
cd 5-docker
Copy-Item .env.example .env
docker compose up -d --build
```

## Notes

- The legacy file `compose/docker-compose.dev.yml` is kept for rollback, but `docker-compose.yml` is now the primary entrypoint.
- MySQL init scripts under `mysql/init/` only run when the MySQL data volume is empty for the first time.
- The `ros1_ros2_bridge` image is intentionally Linux-deployment-oriented. It builds ROS2 Humble first and then builds `ros1_bridge` after sourcing both ROS 1 and ROS 2 environments, following the official bridge workflow for Focal.
- The default bridge command is `ros2 run ros1_bridge dynamic_bridge`. If you need ROS 1 CLI tools like `rostopic list` to see ROS 2 topics during debugging, override it with `--bridge-all-2to1-topics`.
- Put the algorithm workspace directly under `ros1-camera-algo/volumes/ros1_camera_ws/`. There is no git requirement in this layout.
