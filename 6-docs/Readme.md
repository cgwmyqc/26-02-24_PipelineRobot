# ROS2 测试命令

开始自动运行：

```
ros2 topic pub /fixed_controller/start_auto std_msgs/msg/Bool "{data: true}" --once
```

检测完成：

```
ros2 topic pub /fixed_controller/detect_done std_msgs/msg/Bool "{data: true}" --once
```

看编码器：

```
ros2 topic echo /fixed_controller/encoder_count
```

看位移：

```
ros2 topic echo /fixed_controller/travel_m
```

看到位事件：

```
ros2 topic echo /fixed_controller/motion_reached
```