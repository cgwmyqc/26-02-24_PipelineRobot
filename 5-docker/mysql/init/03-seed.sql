USE pipeline_robot;

INSERT INTO sys_user (id, username, password_hash, display_name, status, role, created_at)
VALUES
  (1, 'admin', '$2a$10$LZZ/.1nCFD99/jsxWPqNwO.tkn0NS5ahG1mGe6ZcIKJYdECsglAGK', 'System Admin', 'ACTIVE', 'ADMIN', '2026-03-20 08:00:00')
ON DUPLICATE KEY UPDATE
  password_hash = VALUES(password_hash),
  display_name = VALUES(display_name),
  status = VALUES(status),
  role = VALUES(role);

INSERT INTO inspection_record (id, mode, environment, operator_name, result_summary, result_dir, video_path, inspection_time, created_at)
VALUES
  (1, '1', '0', 'admin', '1', 'results/20260320091501', 'results/20260320091501/videos/inspection_1_20260320091501.mp4', '2026-03-20 09:12:00', '2026-03-20 09:15:00'),
  (2, '0', '1', 'admin', '2', 'results/20260319141002', 'results/20260319141002/videos/inspection_0_20260319141002.mp4', '2026-03-19 14:05:00', '2026-03-19 14:10:00'),
  (3, '2', '1', 'admin', '2', 'results/20260318102403', 'results/20260318102403/videos/inspection_2_20260318102403.mp4', '2026-03-18 10:20:00', '2026-03-18 10:24:00'),
  (4, '1', '1', 'admin', '0', 'results/20260317161404', 'results/20260317161404/videos/inspection_1_20260317161404.mp4', '2026-03-17 16:10:00', '2026-03-17 16:14:00'),
  (5, '0', '0', 'admin', '1', 'results/20260316113605', 'results/20260316113605/videos/inspection_0_20260316113605.mp4', '2026-03-16 11:32:00', '2026-03-16 11:36:00'),
  (6, '2', '0', 'admin', '2', 'results/20260315131206', 'results/20260315131206/videos/inspection_2_20260315131206.mp4', '2026-03-15 13:08:00', '2026-03-15 13:12:00'),
  (7, '1', '1', 'admin', '2', 'results/20260314095307', 'results/20260314095307/videos/inspection_1_20260314095307.mp4', '2026-03-14 09:48:00', '2026-03-14 09:53:00'),
  (8, '0', '1', 'admin', '0', 'results/20260313154308', 'results/20260313154308/videos/inspection_0_20260313154308.mp4', '2026-03-13 15:40:00', '2026-03-13 15:43:00'),
  (9, '2', '0', 'admin', '1', 'results/20260312105909', 'results/20260312105909/videos/inspection_2_20260312105909.mp4', '2026-03-12 10:55:00', '2026-03-12 10:59:00'),
  (10, '1', '1', 'admin', '0', 'results/20260311172710', 'results/20260311172710/videos/inspection_1_20260311172710.mp4', '2026-03-11 17:22:00', '2026-03-11 17:27:00')
ON DUPLICATE KEY UPDATE
  mode = VALUES(mode),
  environment = VALUES(environment),
  operator_name = VALUES(operator_name),
  result_summary = VALUES(result_summary),
  result_dir = VALUES(result_dir),
  video_path = VALUES(video_path),
  inspection_time = VALUES(inspection_time),
  created_at = VALUES(created_at);

INSERT INTO inspection_anomaly_image (id, inspection_id, image_path, anomaly_type, remark, captured_at)
VALUES
  (1, 1, 'results/20260320091501/images/stop_0001_PL.jpg', 'PL', 'stop_0001_PL.jpg', '2026-03-20 09:13:00'),
  (2, 1, 'results/20260320091501/images/stop_0002_PL.jpg', 'PL', 'stop_0002_PL.jpg', '2026-03-20 09:13:30'),
  (3, 2, 'results/20260319141002/images/stop_0001_RG.jpg', 'RG', 'stop_0001_RG.jpg', '2026-03-19 14:06:00'),
  (4, 3, 'results/20260318102403/images/stop_0001_BX_ZAW.jpg', 'BX+ZAW', 'stop_0001_BX_ZAW.jpg', '2026-03-18 10:21:00'),
  (5, 5, 'results/20260316113605/images/stop_0001_PL_RG.jpg', 'PL+RG', 'stop_0001_PL_RG.jpg', '2026-03-16 11:34:00'),
  (6, 6, 'results/20260315131206/images/stop_0001_SG.jpg', 'SG', 'stop_0001_SG.jpg', '2026-03-15 13:10:00'),
  (7, 7, 'results/20260314095307/images/stop_0001_ZAW.jpg', 'ZAW', 'stop_0001_ZAW.jpg', '2026-03-14 09:50:00'),
  (8, 9, 'results/20260312105909/images/stop_0001_BX.jpg', 'BX', 'stop_0001_BX.jpg', '2026-03-12 10:57:00')
ON DUPLICATE KEY UPDATE
  inspection_id = VALUES(inspection_id),
  image_path = VALUES(image_path),
  anomaly_type = VALUES(anomaly_type),
  remark = VALUES(remark),
  captured_at = VALUES(captured_at);

INSERT INTO inspection_point_file (id, inspection_id, point_path, captured_at)
VALUES
  (1, 3, 'results/20260318102403/points/assembled_cloud_01.pcd', '2026-03-18 10:23:00'),
  (2, 6, 'results/20260315131206/points/assembled_cloud_02.pcd', '2026-03-15 13:11:00')
ON DUPLICATE KEY UPDATE
  inspection_id = VALUES(inspection_id),
  point_path = VALUES(point_path),
  captured_at = VALUES(captured_at);
