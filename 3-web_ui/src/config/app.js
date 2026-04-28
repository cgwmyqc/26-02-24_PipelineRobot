const browserHost = window.location.hostname || 'localhost'

export const appConfig = {
  backendBaseURL: import.meta.env.VITE_API_BASE_URL || `http://${browserHost}:8080/api`,
  rosbridgeUrl: import.meta.env.VITE_ROSBRIDGE_URL || `ws://${browserHost}:9090`,
  webrtcStreamerUrl: import.meta.env.VITE_WEBRTC_STREAMER_URL || `http://${browserHost}:8000`,
  webrtcStreamName: import.meta.env.VITE_WEBRTC_STREAM_NAME || 'inspection_camera',
  authStorageKey: 'pipeline-robot-auth',
  topics: {
    temperature: {
      name: '/env_sensor/temperature',
      messageType: 'std_msgs/msg/Float32'
    },
    humidity: {
      name: '/env_sensor/humidity',
      messageType: 'std_msgs/msg/Float32'
    },
    waterSensor: {
      name: '/water_sensor',
      messageType: 'std_msgs/msg/Bool'
    },
    manualModeState: {
      name: '/fixed_controller/manual_mode',
      messageType: 'std_msgs/msg/Bool'
    },
    motorEnable: {
      name: '/fixed_controller/motor_enable',
      messageType: 'std_msgs/msg/Bool'
    },
    motorRunState: {
      name: '/fixed_controller/motor_run_state',
      messageType: 'std_msgs/msg/Int8'
    },
    encoderCount: {
      name: '/fixed_controller/encoder_count',
      messageType: 'std_msgs/msg/Int32'
    },
    travelMeters: {
      name: '/fixed_controller/travel_m',
      messageType: 'std_msgs/msg/Float32'
    },
    laserDistances: {
      name: '/fixed_controller/laser_distances_mm',
      messageType: 'std_msgs/msg/Int32MultiArray'
    },
    motionReached: {
      name: '/fixed_controller/motion_reached',
      messageType: 'std_msgs/msg/Bool'
    },
    autoReturnHomeDone: {
      name: '/fixed_controller/auto_return_home_done',
      messageType: 'std_msgs/msg/Bool'
    },
    manualModeCommand: {
      name: '/fixed_controller/set_manual_mode',
      messageType: 'std_msgs/msg/Bool'
    },
    manualForwardCommand: {
      name: '/fixed_controller/manual_forward_cmd',
      messageType: 'std_msgs/msg/Bool'
    },
    manualReverseCommand: {
      name: '/fixed_controller/manual_reverse_cmd',
      messageType: 'std_msgs/msg/Bool'
    },
    startAuto: {
      name: '/fixed_controller/start_auto',
      messageType: 'std_msgs/msg/Bool'
    },
    detectDone: {
      name: '/fixed_controller/detect_done',
      messageType: 'std_msgs/msg/Bool'
    },
    uiCallScriptCmd: {
      name: '/ui_call_script_cmd',
      messageType: 'std_msgs/msg/String'
    },
    mobileOtaCommand: {
      name: import.meta.env.VITE_MOBILE_OTA_COMMAND_TOPIC || '/mobile_part/ota/command',
      messageType: 'std_msgs/msg/String'
    },
    mobileOtaStatus: {
      name: import.meta.env.VITE_MOBILE_OTA_STATUS_TOPIC || '/mobile_part/ota/status',
      messageType: 'std_msgs/msg/String'
    },
    mobileOtaProgress: {
      name: import.meta.env.VITE_MOBILE_OTA_PROGRESS_TOPIC || '/mobile_part/ota/progress',
      messageType: 'std_msgs/msg/Int32'
    },
    fixedOtaCommand: {
      name: import.meta.env.VITE_FIXED_OTA_COMMAND_TOPIC || '/fixed_part/ota/command',
      messageType: 'std_msgs/msg/String'
    },
    fixedOtaStatus: {
      name: import.meta.env.VITE_FIXED_OTA_STATUS_TOPIC || '/fixed_part/ota/status',
      messageType: 'std_msgs/msg/String'
    },
    fixedOtaProgress: {
      name: import.meta.env.VITE_FIXED_OTA_PROGRESS_TOPIC || '/fixed_part/ota/progress',
      messageType: 'std_msgs/msg/Int32'
    },
    pointCloud: {
      name: '/web_ui/point_cloud',
      messageType: 'sensor_msgs/msg/PointCloud2'
    },
    testPointCloudSource: {
      name: '/livox/lidar',
      messageType: 'sensor_msgs/msg/PointCloud2'
    }
  }
}
