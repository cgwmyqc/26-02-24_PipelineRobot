export const appConfig = {
  backendBaseURL: import.meta.env.VITE_API_BASE_URL || 'http://localhost:8080/api',
  rosbridgeUrl: import.meta.env.VITE_ROSBRIDGE_URL || 'ws://localhost:9090',
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
    motionReached: {
      name: '/fixed_controller/motion_reached',
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
    pointCloud: {
      name: '/robot/point_cloud_preview',
      messageType: 'std_msgs/msg/Float32MultiArray'
    },
    videoStream: {
      name: '/robot/video_frame_base64',
      messageType: 'std_msgs/msg/String'
    }
  }
}
