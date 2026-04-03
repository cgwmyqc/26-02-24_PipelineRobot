# Web UI Stream Node

这个节点只给 `3-web_ui` 提供稳定的 UI 专用流，不影响算法继续使用原始话题。

输入：

- `/ipcamera/image_raw`
- `/livox/lidar`

输出：

- `/web_ui/video_frame` (`sensor_msgs/msg/CompressedImage`, JPEG)
- `/web_ui/point_cloud` (`sensor_msgs/msg/PointCloud2`)

启动：

```bash
cd 5-docker/web-ui-stream-node
chmod +x start.sh
./start.sh
```

`start.sh` 已经内置 ROS 环境加载，不需要额外手动 `source /opt/ros/humble/setup.bash`，也不需要 `colcon build`。
