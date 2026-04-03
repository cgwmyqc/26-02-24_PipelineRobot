#!/usr/bin/env python3
from __future__ import annotations

import math
from collections import deque
from dataclasses import dataclass
from typing import Deque, Optional

import cv2
import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSHistoryPolicy, QoSProfile, QoSReliabilityPolicy, qos_profile_sensor_data
from rclpy.executors import ExternalShutdownException
from sensor_msgs.msg import CompressedImage, Image, PointCloud2, PointField
from std_msgs.msg import Header


VIDEO_OUTPUT_TOPIC = '/web_ui/video_frame'
POINT_CLOUD_OUTPUT_TOPIC = '/web_ui/point_cloud'
VIDEO_INPUT_TOPIC = '/ipcamera/image_raw'
POINT_CLOUD_INPUT_TOPIC = '/livox/lidar'

VIDEO_DELAY_SEC = 2.0
POINT_CLOUD_DELAY_SEC = 2.0
VIDEO_INACTIVE_SEC = 2.0
POINT_CLOUD_INACTIVE_SEC = 2.0

VIDEO_OUTPUT_FPS = 5.0
POINT_CLOUD_OUTPUT_FPS = 4.0
VIDEO_OUTPUT_SIZE = (640, 640)
VIDEO_JPEG_QUALITY = 72
POINT_CLOUD_TARGET_POINTS = 4000

VIDEO_BUFFER_LEN = 64
POINT_CLOUD_BUFFER_LEN = 24


@dataclass
class VideoFrame:
  received_ns: int
  frame: np.ndarray


@dataclass
class PointCloudFrame:
  received_ns: int
  points: np.ndarray


def make_best_effort_qos(depth: int) -> QoSProfile:
  return QoSProfile(
    reliability=QoSReliabilityPolicy.BEST_EFFORT,
    history=QoSHistoryPolicy.KEEP_LAST,
    depth=depth
  )


class UiStreamBridge(Node):
  def __init__(self) -> None:
    super().__init__('web_ui_stream_bridge')

    self.video_buffer: Deque[VideoFrame] = deque(maxlen=VIDEO_BUFFER_LEN)
    self.point_cloud_buffer: Deque[PointCloudFrame] = deque(maxlen=POINT_CLOUD_BUFFER_LEN)

    self.last_video_input_ns = 0
    self.last_point_cloud_input_ns = 0
    self.last_video_publish_msg: Optional[CompressedImage] = None
    self.last_point_cloud_publish_msg: Optional[PointCloud2] = None

    self.video_received_count = 0
    self.point_cloud_received_count = 0
    self.video_published_count = 0
    self.point_cloud_published_count = 0

    ui_qos = make_best_effort_qos(1)

    self.create_subscription(
      Image,
      VIDEO_INPUT_TOPIC,
      self.handle_video_frame,
      qos_profile_sensor_data
    )
    self.create_subscription(
      PointCloud2,
      POINT_CLOUD_INPUT_TOPIC,
      self.handle_point_cloud,
      qos_profile_sensor_data
    )
    self.video_pub = self.create_publisher(CompressedImage, VIDEO_OUTPUT_TOPIC, ui_qos)
    self.point_cloud_pub = self.create_publisher(PointCloud2, POINT_CLOUD_OUTPUT_TOPIC, ui_qos)

    self.create_timer(1.0 / VIDEO_OUTPUT_FPS, self.publish_video_frame)
    self.create_timer(1.0 / POINT_CLOUD_OUTPUT_FPS, self.publish_point_cloud)
    self.create_timer(5.0, self.log_stats)

    self.get_logger().info(
      'web_ui_stream_bridge started: '
      f'{VIDEO_INPUT_TOPIC} -> {VIDEO_OUTPUT_TOPIC}, '
      f'{POINT_CLOUD_INPUT_TOPIC} -> {POINT_CLOUD_OUTPUT_TOPIC}'
    )

  def now_ns(self) -> int:
    return self.get_clock().now().nanoseconds

  def make_header(self, frame_id: str) -> Header:
    return Header(stamp=self.get_clock().now().to_msg(), frame_id=frame_id)

  def pick_buffered_frame(self, buffer, target_delay_sec: float):
    if not buffer:
      return None

    target_ns = self.now_ns() - int(target_delay_sec * 1_000_000_000)
    selected = None
    for item in buffer:
      if item.received_ns <= target_ns:
        selected = item
      else:
        break
    return selected if selected is not None else buffer[0]

  def handle_video_frame(self, msg: Image) -> None:
    if msg.encoding.lower() != 'bgr8':
      return

    try:
      frame = self.image_message_to_bgr(msg)
      resized = cv2.resize(frame, VIDEO_OUTPUT_SIZE, interpolation=cv2.INTER_AREA)
    except Exception as error:
      self.get_logger().warning(f'video preprocess failed: {error}')
      return

    now_ns = self.now_ns()
    self.video_buffer.append(VideoFrame(received_ns=now_ns, frame=resized))
    self.last_video_input_ns = now_ns
    self.video_received_count += 1

  def image_message_to_bgr(self, msg: Image) -> np.ndarray:
    width = int(msg.width)
    height = int(msg.height)
    step = int(msg.step)
    raw = np.frombuffer(msg.data, dtype=np.uint8)
    rows = raw.reshape((height, step))
    return rows[:, :width * 3].reshape((height, width, 3))

  def publish_video_frame(self) -> None:
    selected = self.pick_buffered_frame(self.video_buffer, VIDEO_DELAY_SEC)
    now_ns = self.now_ns()

    if selected is not None:
      success, encoded = cv2.imencode(
        '.jpg',
        selected.frame,
        [int(cv2.IMWRITE_JPEG_QUALITY), VIDEO_JPEG_QUALITY]
      )
      if not success:
        return

      msg = CompressedImage()
      msg.header = self.make_header('web_ui_video')
      msg.format = 'jpeg'
      msg.data = encoded.tobytes()
      self.last_video_publish_msg = msg
      self.video_pub.publish(msg)
      self.video_published_count += 1
      return

    if self.last_video_publish_msg and now_ns - self.last_video_input_ns <= int(VIDEO_INACTIVE_SEC * 1_000_000_000):
      replay = CompressedImage()
      replay.header = self.make_header(self.last_video_publish_msg.header.frame_id)
      replay.format = self.last_video_publish_msg.format
      replay.data = self.last_video_publish_msg.data
      self.video_pub.publish(replay)
      self.video_published_count += 1

  def handle_point_cloud(self, msg: PointCloud2) -> None:
    try:
      points = self.point_cloud_to_xyz(msg)
    except Exception as error:
      self.get_logger().warning(f'point cloud preprocess failed: {error}')
      return

    if points.size == 0:
      return

    now_ns = self.now_ns()
    reduced = self.reduce_point_cloud(points, POINT_CLOUD_TARGET_POINTS)
    self.point_cloud_buffer.append(PointCloudFrame(received_ns=now_ns, points=reduced))
    self.last_point_cloud_input_ns = now_ns
    self.point_cloud_received_count += 1

  def point_cloud_to_xyz(self, msg: PointCloud2) -> np.ndarray:
    point_count = int(msg.width) * max(1, int(msg.height))
    if point_count <= 0 or msg.point_step <= 0:
      return np.empty((0, 3), dtype=np.float32)

    field_offsets = {field.name: field.offset for field in msg.fields}
    if not {'x', 'y', 'z'}.issubset(field_offsets):
      return np.empty((0, 3), dtype=np.float32)

    endian = '>' if msg.is_bigendian else '<'
    dtype = np.dtype({
      'names': ['x', 'y', 'z'],
      'formats': [f'{endian}f4', f'{endian}f4', f'{endian}f4'],
      'offsets': [field_offsets['x'], field_offsets['y'], field_offsets['z']],
      'itemsize': msg.point_step
    })
    structured = np.frombuffer(msg.data, dtype=dtype, count=point_count)
    points = np.stack((structured['x'], structured['y'], structured['z']), axis=1)
    valid_mask = np.isfinite(points).all(axis=1)
    return points[valid_mask].astype(np.float32, copy=False)

  def reduce_point_cloud(self, points: np.ndarray, target_points: int) -> np.ndarray:
    if len(points) <= target_points:
      return points.copy()

    step = max(1, math.ceil(len(points) / target_points))
    sampled = points[::step]
    if len(sampled) > target_points:
      sampled = sampled[:target_points]
    return sampled.copy()

  def build_point_cloud_message(self, points: np.ndarray) -> PointCloud2:
    cloud = PointCloud2()
    cloud.header = self.make_header('web_ui_point_cloud')
    cloud.height = 1
    cloud.width = int(len(points))
    cloud.fields = [
      PointField(name='x', offset=0, datatype=PointField.FLOAT32, count=1),
      PointField(name='y', offset=4, datatype=PointField.FLOAT32, count=1),
      PointField(name='z', offset=8, datatype=PointField.FLOAT32, count=1)
    ]
    cloud.is_bigendian = False
    cloud.point_step = 12
    cloud.row_step = cloud.point_step * cloud.width
    cloud.is_dense = True
    cloud.data = np.ascontiguousarray(points, dtype=np.float32).tobytes()
    return cloud

  def publish_point_cloud(self) -> None:
    selected = self.pick_buffered_frame(self.point_cloud_buffer, POINT_CLOUD_DELAY_SEC)
    now_ns = self.now_ns()

    if selected is not None:
      msg = self.build_point_cloud_message(selected.points)
      self.last_point_cloud_publish_msg = msg
      self.point_cloud_pub.publish(msg)
      self.point_cloud_published_count += 1
      return

    if self.last_point_cloud_publish_msg and now_ns - self.last_point_cloud_input_ns <= int(POINT_CLOUD_INACTIVE_SEC * 1_000_000_000):
      replay = PointCloud2()
      replay.header = self.make_header(self.last_point_cloud_publish_msg.header.frame_id)
      replay.height = self.last_point_cloud_publish_msg.height
      replay.width = self.last_point_cloud_publish_msg.width
      replay.fields = self.last_point_cloud_publish_msg.fields
      replay.is_bigendian = self.last_point_cloud_publish_msg.is_bigendian
      replay.point_step = self.last_point_cloud_publish_msg.point_step
      replay.row_step = self.last_point_cloud_publish_msg.row_step
      replay.is_dense = self.last_point_cloud_publish_msg.is_dense
      replay.data = self.last_point_cloud_publish_msg.data
      self.point_cloud_pub.publish(replay)
      self.point_cloud_published_count += 1

  def log_stats(self) -> None:
    self.get_logger().info(
      'raw video fps=%.2f, ui video fps=%.2f, raw point cloud fps=%.2f, ui point cloud fps=%.2f, '
      'video buffer=%d, point cloud buffer=%d'
      % (
        self.video_received_count / 5.0,
        self.video_published_count / 5.0,
        self.point_cloud_received_count / 5.0,
        self.point_cloud_published_count / 5.0,
        len(self.video_buffer),
        len(self.point_cloud_buffer)
      )
    )
    self.video_received_count = 0
    self.video_published_count = 0
    self.point_cloud_received_count = 0
    self.point_cloud_published_count = 0


def main() -> None:
  rclpy.init()
  node = UiStreamBridge()
  try:
    rclpy.spin(node)
  except (KeyboardInterrupt, ExternalShutdownException):
    pass
  finally:
    node.destroy_node()
    if rclpy.ok():
      rclpy.shutdown()


if __name__ == '__main__':
  main()
