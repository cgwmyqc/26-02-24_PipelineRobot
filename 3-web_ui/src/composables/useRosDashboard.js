import { computed, onBeforeUnmount, onMounted } from 'vue'
import { storeToRefs } from 'pinia'
import { useDashboardStore } from '../stores/dashboard'
import { rosService } from '../services/ros'
import { appConfig } from '../config/app'

const POINT_CLOUD_TIMEOUT_MS = 2500
const MAX_POINT_CLOUD_POINTS = 5000
const IS_DEV = import.meta.env.DEV

function debugLog(...args) {
  if (IS_DEV) {
    console.debug('[ros-dashboard]', ...args)
  }
}

function decodeBase64ToBytes(base64Value) {
  const sanitized = String(base64Value || '').trim()
  if (!sanitized) {
    return null
  }

  const binary = atob(sanitized)
  const bytes = new Uint8Array(binary.length)
  for (let index = 0; index < binary.length; index += 1) {
    bytes[index] = binary.charCodeAt(index)
  }
  return bytes
}

function normalizeByteArray(data) {
  if (!data) {
    return null
  }
  if (data instanceof Uint8Array) {
    return data
  }
  if (Array.isArray(data)) {
    return Uint8Array.from(data)
  }
  if (typeof data === 'string') {
    try {
      return decodeBase64ToBytes(data)
    } catch (_error) {
      return null
    }
  }
  if (typeof data === 'object' && typeof data.length === 'number') {
    return Uint8Array.from(data)
  }
  return null
}

function getFieldOffset(fields, fieldName) {
  return Array.isArray(fields)
    ? fields.find((field) => field?.name === fieldName)?.offset
    : undefined
}

function decodePointCloud(message) {
  const bytes = normalizeByteArray(message?.data)
  const pointStep = Number(message?.point_step || 0)
  const width = Number(message?.width || 0)
  const height = Number(message?.height || 0)
  const totalPoints = width * Math.max(height, 1)
  const xOffset = getFieldOffset(message?.fields, 'x')
  const yOffset = getFieldOffset(message?.fields, 'y')
  const zOffset = getFieldOffset(message?.fields, 'z')

  if (!bytes || !pointStep || totalPoints <= 0) {
    return []
  }
  if ([xOffset, yOffset, zOffset].some((offset) => offset === undefined)) {
    return []
  }

  const littleEndian = !message?.is_bigendian
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
  const step = Math.max(1, Math.ceil(totalPoints / MAX_POINT_CLOUD_POINTS))
  const points = []

  for (let index = 0; index < totalPoints; index += step) {
    const base = index * pointStep
    if (base + pointStep > view.byteLength) {
      break
    }

    const x = view.getFloat32(base + xOffset, littleEndian)
    const y = view.getFloat32(base + yOffset, littleEndian)
    const z = view.getFloat32(base + zOffset, littleEndian)

    if (Number.isFinite(x) && Number.isFinite(y) && Number.isFinite(z)) {
      points.push({ x, y, z })
    }
  }

  return points
}

export function useRosDashboard() {
  const store = useDashboardStore()
  const { patrolMode, pendingPatrolMode } = storeToRefs(store)
  const unsubscribers = []
  const uiPatrolMode = computed(() => pendingPatrolMode.value || patrolMode.value)
  let streamWatchdogId = 0
  let hasLoggedPointCloudFrame = false

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

  function publishUiScriptCommand(scriptName) {
    const normalized = String(scriptName || '').trim()
    if (!normalized) {
      return
    }

    rosService.publish(appConfig.topics.uiCallScriptCmd, { data: normalized })
  }

  function refreshStreamStates() {
    const now = Date.now()

    if (store.pointCloudLastMessageAt && now - store.pointCloudLastMessageAt > POINT_CLOUD_TIMEOUT_MS) {
      debugLog('point cloud stream timed out')
      store.setPointCloudStreamActive(false)
    }
  }

  onMounted(() => {
    rosService.connect({
      onConnection: () => {
        debugLog('rosbridge connected', appConfig.rosbridgeUrl)
        store.setRosConnected(true)
      },
      onClose: () => {
        debugLog('rosbridge closed')
        store.setRosConnected(false)
        store.resetRealtimeStreams()
      },
      onError: (error) => {
        debugLog('rosbridge error', error)
        store.setRosConnected(false)
      }
    })

    streamWatchdogId = window.setInterval(() => {
      refreshStreamStates()
    }, 500)

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
      rosService.subscribe(appConfig.topics.pointCloud, (message) => {
        try {
          const points = decodePointCloud(message)
          if (!points.length) {
            return
          }

          store.markPointCloudMessageReceived()
          store.setPointCloudPoints(points)

          if (!hasLoggedPointCloudFrame) {
            debugLog('ui point cloud frame received', {
              points: points.length,
              width: message?.width,
              pointStep: message?.point_step
            })
            hasLoggedPointCloudFrame = true
          }
        } catch (_error) {
          debugLog('point cloud decode failed', _error)
        }
      })
    )

    store.loadHistory()
  })

  onBeforeUnmount(() => {
    window.clearInterval(streamWatchdogId)
    unsubscribers.splice(0).forEach((unsubscribe) => unsubscribe())
  })

  return {
    patrolMode: uiPatrolMode,
    setPatrolMode,
    publishMoveCommand,
    startAutoInspection,
    markDetectDone,
    publishUiScriptCommand
  }
}
