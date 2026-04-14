# Pipeline Robot Digital Twin UI

Vue 3 + Vite 前端工程，面向下水道巡检机器人数字孪生页面。

## 技术栈

- Vue 3
- Vite
- Vue Router
- Pinia
- Element Plus
- Three.js
- roslib
- Axios

## 已搭建内容

- 数字孪生主页面布局
- 自动巡检 / 人工巡检切换
- 前进 / 后退控制按钮
- Three.js 管道模型展示
- Three.js 点云区域展示
- ROS2 rosbridge WebSocket 通信封装
- 历史查询表格与 Spring Boot API 占位
- 视频流区域占位，支持 base64 JPEG 帧显示

## 启动

1. 安装依赖

```bash
npm install
```

2. 配置环境变量

```bash
copy .env.example .env
```

3. 启动开发环境

```bash
npm run dev
```

## ROS2 对接说明

当前默认话题配置位于 `src/config/app.js`：

- `/robot/patrol_mode`
- `/robot/move_command`
- `/robot/temperature`
- `/robot/sludge_thickness`
- `/robot/point_cloud_preview`
- `/robot/video_frame_base64`

如果你的 ROS2 消息类型或话题名不同，直接修改配置即可。

## 视频接入说明

实时视频模块不再订阅 ROS2 `/web_ui/video_frame`。

- 前端默认通过 `webrtc-streamer` 播放视频
- `webrtc-streamer` 直接连接 RTSP 源
- 可通过以下环境变量覆盖默认配置：
  - `VITE_WEBRTC_STREAMER_URL`
  - `VITE_WEBRTC_STREAM_NAME`

## 后续建议

- 点云如果是 `sensor_msgs/msg/PointCloud2`，建议增加后端解码或在前端补充 PointCloud2 解析。
- 历史查询详情和导出按钮目前只保留界面，后续接业务接口。

## 测试模式点云积分

- 测试模式触发按钮会继续发送 `trigger_stop_capture.sh`
- 前端会直接订阅 `/livox/lidar`，在默认 `1000ms` 窗口内本地积分
- 积分完成后，前端把这次静态点云结果按测试序号沿 Z 轴叠加到场景
- 普通实时点云预览仍继续使用 `/web_ui/point_cloud`
