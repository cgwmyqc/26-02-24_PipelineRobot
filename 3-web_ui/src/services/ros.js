import ROSLIB from 'roslib'
import { appConfig } from '../config/app'

class RosService {
  constructor() {
    this.ros = null
    this.topicCache = new Map()
    this.handlersBound = false
  }

  connect({ onConnection, onClose, onError } = {}) {
    if (!this.ros) {
      this.ros = new ROSLIB.Ros({
        url: appConfig.rosbridgeUrl
      })
    }

    if (!this.handlersBound) {
      this.ros.on('connection', () => onConnection?.())
      this.ros.on('close', () => onClose?.())
      this.ros.on('error', (error) => onError?.(error))
      this.handlersBound = true
    }

    return this.ros
  }

  ensureConnection() {
    if (!this.ros) {
      this.connect()
    }
  }

  getTopic(config) {
    this.ensureConnection()
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
    this.getTopic(config).publish(new ROSLIB.Message(data))
  }

  subscribe(config, handler) {
    const topic = this.getTopic(config)
    topic.subscribe(handler)
    return () => topic.unsubscribe(handler)
  }
}

export const rosService = new RosService()
