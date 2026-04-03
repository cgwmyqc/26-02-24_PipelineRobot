# Docker Layout

`5-docker/docker-compose.yml` is the only compose entrypoint for the current Docker runtime stack.

## Services

- `micro_ros_agent`: UDP micro-ROS agent, exposed on host port `8888`
- `web_ros2_bridge`: `rosbridge_server` for the web UI, exposed on host port `9090`
- `mysql`: MySQL 8.0, exposed on host port `3307`, default app user `dev/123456`

## Runtime Notes

- `docker-compose.yml` now manages only the base infrastructure services listed above.
- The `ipcamera` container and image have been removed from `5-docker`; camera-related ROS2 packages must now be started directly on the host machine.
- Running `docker compose up` no longer starts any camera node automatically.
- The old ROS1-related directories are still kept in the repository for reference only:
  - `5-docker/ros1-camera-algo/`
  - `5-docker/ros1-ros2-bridge/`

## Run

```powershell
cd 5-docker
Copy-Item .env.example .env
docker compose up -d --build
```

This command starts only the Docker-managed support services: `micro_ros_agent`, `web_ros2_bridge`, and `mysql`.

Camera-related ROS2 packages should be launched directly in the host ROS environment, not through `docker compose`.

## Notes

- `compose/docker-compose.dev.yml` is still kept as a legacy rollback file, but it is not the main entrypoint.
- MySQL init scripts under `mysql/init/` only run when the MySQL data volume is empty for the first time.
- A backup of the previous main compose file should be kept under `5-docker/bak/` before further migration edits.
