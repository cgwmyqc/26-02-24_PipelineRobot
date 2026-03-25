CREATE TABLE IF NOT EXISTS sys_user (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  username VARCHAR(64) NOT NULL UNIQUE,
  password_hash VARCHAR(255) NOT NULL,
  display_name VARCHAR(128) NOT NULL,
  status VARCHAR(32) NOT NULL,
  role VARCHAR(64) NOT NULL,
  created_at DATETIME NOT NULL
);

CREATE TABLE IF NOT EXISTS inspection_record (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  mode VARCHAR(32) NOT NULL,
  environment VARCHAR(64) NOT NULL,
  operator_name VARCHAR(128) NOT NULL,
  result_summary VARCHAR(255) NOT NULL,
  video_path VARCHAR(255),
  inspection_time DATETIME NOT NULL,
  created_at DATETIME NOT NULL
);

CREATE TABLE IF NOT EXISTS inspection_anomaly_image (
  id BIGINT PRIMARY KEY AUTO_INCREMENT,
  inspection_id BIGINT NOT NULL,
  image_path VARCHAR(255),
  anomaly_type VARCHAR(64) NOT NULL,
  remark VARCHAR(255),
  captured_at DATETIME NOT NULL,
  CONSTRAINT fk_inspection_record FOREIGN KEY (inspection_id) REFERENCES inspection_record(id) ON DELETE CASCADE
);
