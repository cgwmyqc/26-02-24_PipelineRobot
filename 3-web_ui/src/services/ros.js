import ROSLIB from 'roslib'
import { appConfig } from '../config/app'

class RosService {
  constructor() {
    this.ros = null
    this.topicCache = new Map()
  }

  connect({ onConnection, onClose, onError } = {}) {
    if (this.ros) {
      return this.ros
    }

    this.ros = new ROSLIB.Ros({
      url: appConfig.rosbridgeUrl
    })

    this.ros.on('connection', () => onConnection?.())
    this.ros.on('close', () => onClose?.())
    this.ros.on('error', (error) => onError?.(error))

    return this.ros
  }

  getTopic(config) {
    const key = `${config.name}:${config.messageType}`
    if (!this.topicCache.has(key)) {
      this.topicCache.set(
        key,
        new ROSLIB.Topic({
          ros: this.ros,
          name: config.name,
          messageType: config.messageType
        })
      )
    }
    return this.topicCache.get(key)
  }

  publish(config, data) {
    if (!this.ros) {
      return
    }
    this.getTopic(config).publish(new ROSLIB.Message(data))
  }

  subscribe(config, handler) {
    if (!this.ros) {
      return () => {}
    }
    const topic = this.getTopic(config)
    topic.subscribe(handler)
    return () => topic.unsubscribe(handler)
  }
}

export const rosService = new RosService()
