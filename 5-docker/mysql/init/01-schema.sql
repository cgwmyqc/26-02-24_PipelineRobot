CREATE DATABASE IF NOT EXISTS pipeline_robot DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE pipeline_robot;

CREATE TABLE IF NOT EXISTS sys_user (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  username VARCHAR(64) NOT NULL UNIQUE,
  password_hash VARCHAR(255) NOT NULL,
  display_name VARCHAR(128) NOT NULL,
  status VARCHAR(32) NOT NULL,
  role VARCHAR(64) NOT NULL,
  created_at DATETIME NOT NULL,
  INDEX idx_sys_user_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS inspection_record (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  mode VARCHAR(32) NOT NULL,
  environment VARCHAR(64) NOT NULL,
  operator_name VARCHAR(128) NOT NULL,
  result_summary VARCHAR(255) NOT NULL,
  result_dir VARCHAR(255),
  video_path VARCHAR(255),
  fitted_result_path VARCHAR(255),
  inspection_time DATETIME NOT NULL,
  created_at DATETIME NOT NULL,
  INDEX idx_inspection_mode (mode),
  INDEX idx_inspection_environment (environment),
  INDEX idx_inspection_time (inspection_time),
  INDEX idx_inspection_created_at (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS inspection_anomaly_image (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  inspection_id BIGINT NOT NULL,
  image_path VARCHAR(255),
  anomaly_type VARCHAR(64) NOT NULL,
  remark VARCHAR(255),
  captured_at DATETIME NOT NULL,
  CONSTRAINT fk_inspection_record FOREIGN KEY (inspection_id) REFERENCES inspection_record(id) ON DELETE CASCADE,
  INDEX idx_anomaly_inspection_id (inspection_id),
  INDEX idx_anomaly_type (anomaly_type),
  INDEX idx_anomaly_captured_at (captured_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS inspection_point_file (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  inspection_id BIGINT NOT NULL,
  point_path VARCHAR(255) NOT NULL,
  captured_at DATETIME NOT NULL,
  CONSTRAINT fk_point_inspection_record FOREIGN KEY (inspection_id) REFERENCES inspection_record(id) ON DELETE CASCADE,
  INDEX idx_point_inspection_id (inspection_id),
  INDEX idx_point_captured_at (captured_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
