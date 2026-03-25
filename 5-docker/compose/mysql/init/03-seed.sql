USE pipeline_robot;

INSERT INTO sys_user (id, username, password_hash, display_name, status, role, created_at)
VALUES
  (1, 'admin', '$2a$10$LZZ/.1nCFD99/jsxWPqNwO.tkn0NS5ahG1mGe6ZcIKJYdECsglAGK', 'System Admin', 'ACTIVE', 'ADMIN', '2026-03-20 08:00:00')
ON DUPLICATE KEY UPDATE
  password_hash = VALUES(password_hash),
  display_name = VALUES(display_name),
  status = VALUES(status),
  role = VALUES(role);

INSERT INTO inspection_record (id, mode, environment, operator_name, result_summary, video_path, inspection_time, created_at)
VALUES
  (1, 'AUTO', 'dry', 'admin', 'Routine scan completed with joint crack found', 'videos/inspection-2026-03-20-01.mp4', '2026-03-20 09:12:00', '2026-03-20 09:15:00'),
  (2, 'MANUAL', 'humid', 'admin', 'Manual follow-up completed with wall seepage observed', 'videos/inspection-2026-03-19-02.mp4', '2026-03-19 14:05:00', '2026-03-19 14:10:00'),
  (3, 'AUTO', 'waterlogged', 'admin', 'Auto patrol completed and sediment warning recorded', 'videos/inspection-2026-03-18-03.mp4', '2026-03-18 10:20:00', '2026-03-18 10:24:00'),
  (4, 'MANUAL', 'dry', 'admin', 'Drive test completed without visible anomaly', 'videos/inspection-2026-03-17-04.mp4', '2026-03-17 16:10:00', '2026-03-17 16:14:00'),
  (5, 'AUTO', 'humid', 'admin', 'Routine scan completed with corrosion marks', 'videos/inspection-2026-03-16-05.mp4', '2026-03-16 11:32:00', '2026-03-16 11:36:00'),
  (6, 'MANUAL', 'dry', 'admin', 'Manual inspection completed and cable tension normal', 'videos/inspection-2026-03-15-06.mp4', '2026-03-15 13:08:00', '2026-03-15 13:12:00'),
  (7, 'AUTO', 'waterlogged', 'admin', 'Auto patrol completed with standing water noted', 'videos/inspection-2026-03-14-07.mp4', '2026-03-14 09:48:00', '2026-03-14 09:53:00'),
  (8, 'MANUAL', 'humid', 'admin', 'Manual inspection completed without abnormal image capture', 'videos/inspection-2026-03-13-08.mp4', '2026-03-13 15:40:00', '2026-03-13 15:43:00'),
  (9, 'AUTO', 'dry', 'admin', 'Routine scan completed with minor offset at hanger section', 'videos/inspection-2026-03-12-09.mp4', '2026-03-12 10:55:00', '2026-03-12 10:59:00'),
  (10, 'MANUAL', 'waterlogged', 'admin', 'Manual verification completed and no secondary defect found', 'videos/inspection-2026-03-11-10.mp4', '2026-03-11 17:22:00', '2026-03-11 17:27:00')
ON DUPLICATE KEY UPDATE
  mode = VALUES(mode),
  environment = VALUES(environment),
  operator_name = VALUES(operator_name),
  result_summary = VALUES(result_summary),
  video_path = VALUES(video_path),
  inspection_time = VALUES(inspection_time),
  created_at = VALUES(created_at);

INSERT INTO inspection_anomaly_image (id, inspection_id, image_path, anomaly_type, remark, captured_at)
VALUES
  (1, 1, 'images/anomaly-202603-01.jpg', 'joint_crack', 'Crack detected at the upper joint section.', '2026-03-20 09:13:00'),
  (2, 1, 'images/anomaly-202603-02.jpg', 'joint_crack', 'Secondary close-up confirming edge fracture.', '2026-03-20 09:13:30'),
  (3, 2, 'images/anomaly-202603-03.jpg', 'wall_seepage', 'Moisture streak visible along the right wall.', '2026-03-19 14:06:00'),
  (4, 3, 'images/anomaly-202603-04.jpg', 'sediment', 'Sediment buildup observed near the trench bottom.', '2026-03-18 10:21:00'),
  (5, 5, 'images/anomaly-202603-05.jpg', 'corrosion', 'Corrosion spot detected on metal support.', '2026-03-16 11:33:00'),
  (6, 9, 'images/anomaly-202603-06.jpg', 'hanger_offset', 'Hanger alignment offset requires review.', '2026-03-12 10:56:00')
ON DUPLICATE KEY UPDATE
  inspection_id = VALUES(inspection_id),
  image_path = VALUES(image_path),
  anomaly_type = VALUES(anomaly_type),
  remark = VALUES(remark),
  captured_at = VALUES(captured_at);
