export const appConfig = {
  backendBaseURL: import.meta.env.VITE_API_BASE_URL || 'http://localhost:8080/api',
  rosbridgeUrl: import.meta.env.VITE_ROSBRIDGE_URL || 'ws://localhost:9090',
  topics: {
    patrolMode: {
      name: '/robot/patrol_mode',
      messageType: 'std_msgs/msg/String'
    },
    move: {
      name: '/robot/move_command',
      messageType: 'std_msgs/msg/String'
    },
    temperature: {
      name: '/robot/temperature',
      messageType: 'std_msgs/msg/Float32'
    },
    sludgeThickness: {
      name: '/robot/sludge_thickness',
      messageType: 'std_msgs/msg/Float32'
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
