import { computed, onBeforeUnmount, onMounted } from 'vue'
import { storeToRefs } from 'pinia'
import { useDashboardStore } from '../stores/dashboard'
import { rosService } from '../services/ros'
import { appConfig } from '../config/app'

export function useRosDashboard() {
  const store = useDashboardStore()
  const { patrolMode, pendingPatrolMode } = storeToRefs(store)
  const unsubscribers = []
  const uiPatrolMode = computed(() => pendingPatrolMode.value || patrolMode.value)

  function publishManualMode(isManual) {
    rosService.publish(appConfig.topics.manualModeCommand, {
      data: isManual
    })
  }

  function setPatrolMode(mode) {
    store.setPendingPatrolMode(mode)
    publishManualMode(mode === 'manual')
  }

  function publishMoveCommand(direction, active) {
    const topic = direction === 'forward'
      ? appConfig.topics.manualForwardCommand
      : appConfig.topics.manualReverseCommand

    rosService.publish(topic, { data: active })
  }

  function startAutoInspection() {
    rosService.publish(appConfig.topics.startAuto, { data: true })
  }

  function markDetectDone() {
    rosService.publish(appConfig.topics.detectDone, { data: true })
  }

  onMounted(() => {
    rosService.connect({
      onConnection: () => store.setRosConnected(true),
      onClose: () => store.setRosConnected(false),
      onError: () => store.setRosConnected(false)
    })

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.temperature, (message) => {
        store.updateRosMetric('temperature', message.data)
      })
    )

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.humidity, (message) => {
        store.updateRosMetric('humidity', message.data)
      })
    )

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.waterSensor, (message) => {
        store.updateRosMetric('waterDetected', Boolean(message.data))
      })
    )

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.manualModeState, (message) => {
        store.setPatrolModeByState(Boolean(message.data))
      })
    )

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.motorEnable, (message) => {
        store.updateRosMetric('motorEnabled', Boolean(message.data))
      })
    )

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.motorRunState, (message) => {
        store.updateRosMetric('motorRunState', Number(message.data))
      })
    )

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.encoderCount, (message) => {
        store.updateRosMetric('encoderCount', Number(message.data))
      })
    )

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.travelMeters, (message) => {
        store.updateRosMetric('travelMeters', Number(message.data))
      })
    )

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.motionReached, (message) => {
        store.updateRosMetric('motionReached', Boolean(message.data))
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
    patrolMode: uiPatrolMode,
    setPatrolMode,
    publishMoveCommand,
    startAutoInspection,
    markDetectDone
  }
}
