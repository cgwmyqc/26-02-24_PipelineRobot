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
  (1, '1', '0', 'admin', '1', 'results/20260320101100', 'results/20260320101100/videos/inspection_1_20260320101100.mp4', '2026-03-20 10:07:00', '2026-03-20 10:11:00'),
  (2, '0', '1', 'admin', '2', 'results/20260319111200', 'results/20260319111200/videos/inspection_0_20260319111200.mp4', '2026-03-19 11:07:00', '2026-03-19 11:12:00'),
  (3, '2', '1', 'admin', '2', 'results/20260318091300', 'results/20260318091300/videos/inspection_2_20260318091300.mp4', '2026-03-18 09:07:00', '2026-03-18 09:13:00'),
  (4, '1', '1', 'admin', '0', 'results/20260317101400', 'results/20260317101400/videos/inspection_1_20260317101400.mp4', '2026-03-17 10:07:00', '2026-03-17 10:14:00'),
  (5, '0', '0', 'admin', '1', 'results/20260316111500', 'results/20260316111500/videos/inspection_0_20260316111500.mp4', '2026-03-16 11:07:00', '2026-03-16 11:15:00'),
  (6, '2', '0', 'admin', '2', 'results/20260315091600', 'results/20260315091600/videos/inspection_2_20260315091600.mp4', '2026-03-15 09:07:00', '2026-03-15 09:16:00'),
  (7, '1', '1', 'admin', '2', 'results/20260314101700', 'results/20260314101700/videos/inspection_1_20260314101700.mp4', '2026-03-14 10:07:00', '2026-03-14 10:17:00'),
  (8, '0', '1', 'admin', '0', 'results/20260313111800', 'results/20260313111800/videos/inspection_0_20260313111800.mp4', '2026-03-13 11:07:00', '2026-03-13 11:18:00'),
  (9, '2', '0', 'admin', '1', 'results/20260312091900', 'results/20260312091900/videos/inspection_2_20260312091900.mp4', '2026-03-12 09:07:00', '2026-03-12 09:19:00'),
  (10, '1', '1', 'admin', '0', 'results/20260311102000', 'results/20260311102000/videos/inspection_1_20260311102000.mp4', '2026-03-11 10:07:00', '2026-03-11 10:20:00')
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
  (1, 1, 'results/20260320101100/images/stop_0001_PL.png', 'PL', 'stop_0001_PL.png', '2026-03-20 10:09:00'),
  (2, 1, 'results/20260320101100/images/stop_0002_RG.png', 'RG', 'stop_0002_RG.png', '2026-03-20 10:10:00'),
  (3, 2, 'results/20260319111200/images/stop_0001_RG.png', 'RG', 'stop_0001_RG.png', '2026-03-19 11:11:00'),
  (4, 3, 'results/20260318091300/images/stop_0001_BX.png', 'BX', 'stop_0001_BX.png', '2026-03-18 09:12:00'),
  (5, 5, 'results/20260316111500/images/stop_0001_ZAW.png', 'ZAW', 'stop_0001_ZAW.png', '2026-03-16 11:14:00'),
  (6, 6, 'results/20260315091600/images/stop_0001_PL.png', 'PL', 'stop_0001_PL.png', '2026-03-15 09:15:00'),
  (7, 7, 'results/20260314101700/images/stop_0001_RG.png', 'RG', 'stop_0001_RG.png', '2026-03-14 10:16:00'),
  (8, 9, 'results/20260312091900/images/stop_0001_SG.png', 'SG', 'stop_0001_SG.png', '2026-03-12 09:18:00')
ON DUPLICATE KEY UPDATE
  inspection_id = VALUES(inspection_id),
  image_path = VALUES(image_path),
  anomaly_type = VALUES(anomaly_type),
  remark = VALUES(remark),
  captured_at = VALUES(captured_at);

INSERT INTO inspection_point_file (id, inspection_id, point_path, captured_at)
VALUES
  (1, 3, 'results/20260318091300/points/assembled_cloud_01.pcd', '2026-03-18 09:12:15'),
  (2, 6, 'results/20260315091600/points/assembled_cloud_01.pcd', '2026-03-15 09:14:30'),
  (3, 9, 'results/20260312091900/points/assembled_cloud_01.pcd', '2026-03-12 09:16:45')
ON DUPLICATE KEY UPDATE
  inspection_id = VALUES(inspection_id),
  point_path = VALUES(point_path),
  captured_at = VALUES(captured_at);
