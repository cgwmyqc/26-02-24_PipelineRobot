import { onBeforeUnmount, onMounted } from 'vue'
import { storeToRefs } from 'pinia'
import { useDashboardStore } from '../stores/dashboard'
import { rosService } from '../services/ros'
import { appConfig } from '../config/app'

export function useRosDashboard() {
  const store = useDashboardStore()
  const { patrolMode } = storeToRefs(store)
  const unsubscribers = []

  function publishPatrolMode(mode) {
    store.setPatrolMode(mode)
    rosService.publish(appConfig.topics.patrolMode, {
      data: mode === 'auto' ? 'AUTO' : 'MANUAL'
    })
  }

  function publishMoveCommand(command) {
    rosService.publish(appConfig.topics.move, {
      data: command
    })
  }

  onMounted(() => {
    rosService.connect({
      onConnection: () => store.setRosConnected(true),
      onClose: () => store.setRosConnected(false),
      onError: () => store.setRosConnected(false)
    })

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.temperature, (message) => {
        store.updateMetric('temperature', message.data)
      })
    )

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.sludgeThickness, (message) => {
        store.updateMetric('sludgeThickness', message.data)
      })
    )

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.videoStream, (message) => {
        store.setVideoFrame(message.data)
      })
    )

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.pointCloud, (message) => {
        const raw = Array.isArray(message.data) ? message.data : []
        const points = []
        for (let i = 0; i < raw.length; i += 3) {
          points.push({
            x: raw[i] ?? 0,
            y: raw[i + 1] ?? 0,
            z: raw[i + 2] ?? 0
          })
        }
        store.setPointCloudPoints(points)
      })
    )

    store.loadHistory()
  })

  onBeforeUnmount(() => {
    unsubscribers.splice(0).forEach((unsubscribe) => unsubscribe())
  })

  return {
    patrolMode,
    publishPatrolMode,
    publishMoveCommand
  }
}
