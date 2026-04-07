# `9-ui_call_sh` Workspace

This directory is a standalone ROS2 workspace for the `ui_call_sh` package.

## Layout

- `src/ui_call_sh/`: package source
- `build/`: colcon build artifacts
- `install/`: colcon install artifacts
- `log/`: colcon logs

## Build

```bash
cd 9-ui_call_sh
colcon build
```

## Run

```bash
cd 9-ui_call_sh
source install/setup.bash
ros2 run ui_call_sh ui_call_sh_node
```

## Launch

```bash
cd 9-ui_call_sh
source install/setup.bash
ros2 launch ui_call_sh ui_call_sh.launch.py
```
