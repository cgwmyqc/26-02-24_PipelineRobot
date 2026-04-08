import { computed, onBeforeUnmount, onMounted } from 'vue'
import { storeToRefs } from 'pinia'
import { TEST_MODE_STATES, useDashboardStore } from '../stores/dashboard'
import { rosService } from '../services/ros'
import { appConfig } from '../config/app'

const POINT_CLOUD_TIMEOUT_MS = 2500
const MAX_POINT_CLOUD_POINTS = 5000
const TEST_START_DELAY_MS = 0
const TEST_CAPTURE_LOAD_DELAY_MS = 0
const TEST_FINISH_COOLDOWN_MS = 0
const IS_DEV = import.meta.env.DEV

function debugLog(...args) {
  if (IS_DEV) {
    console.debug('[ros-dashboard]', ...args)
  }
}

function getAxiosStatus(error) {
  return Number(error?.response?.status || 0)
}

function getAxiosMessage(error) {
  return error?.response?.data?.message || error?.message || 'unknown error'
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
  let lastMotionReachedValue = false
  let testStartTimerId = 0
  let testProcessTimerId = 0
  let testCooldownTimerId = 0

  function clearTestTimers() {
    window.clearTimeout(testStartTimerId)
    window.clearTimeout(testProcessTimerId)
    window.clearTimeout(testCooldownTimerId)
    testStartTimerId = 0
    testProcessTimerId = 0
    testCooldownTimerId = 0
  }

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
    if (!store.startAutoAssembly()) {
      return
    }
    lastMotionReachedValue = false
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

  function setTestModeEnabled(enabled) {
    clearTestTimers()
    if (!enabled) {
      store.setTestModeEnabled(false)
      publishUiScriptCommand('stop_pipe_system.sh')
      return
    }

    store.setTestModeEnabled(true)
  }

  function startTestSequence() {
    if (!store.testModeEnabled || store.testState !== TEST_MODE_STATES.WAITING_START) {
      return
    }

    const token = store.advancePcdLoadSequenceToken()
    if (!store.beginTestSystemStart()) {
      return
    }

    publishUiScriptCommand('start_pipe_system.sh')
    testStartTimerId = window.setTimeout(() => {
      if (!store.testModeEnabled || token !== store.pcdLoadSequenceToken) {
        return
      }
      store.setTestWaitingTrigger()
    }, TEST_START_DELAY_MS)
  }

  async function completeTestCapture(stopNumber, token) {
    try {
      await store.loadAndAppendTestCapture(stopNumber)
    } catch (error) {
      const status = getAxiosStatus(error)
      if (status === 404) {
        debugLog(`test capture PCD missing for stop_${String(stopNumber).padStart(4, '0')}`, {
          status,
          message: getAxiosMessage(error)
        })
      } else if (error instanceof Error && /contains no valid points/i.test(error.message)) {
        debugLog(`test capture PCD parse failed for stop_${String(stopNumber).padStart(4, '0')}`, {
          message: error.message
        })
      } else {
        debugLog('test capture load failed', {
          status,
          message: getAxiosMessage(error),
          error
        })
      }
      if (store.testModeEnabled && token === store.pcdLoadSequenceToken) {
        store.setTestWaitingTrigger()
      }
    }
  }

  function triggerTestCapture() {
    if (!store.testModeEnabled || store.testState !== TEST_MODE_STATES.WAITING_TRIGGER || store.triggerCount >= 14) {
      return
    }

    if (store.triggerCount === 0) {
      store.resetTestAssemblyForNextRun()
    }

    const stopNumber = store.triggerCount + 1
    const token = store.advancePcdLoadSequenceToken()
    if (!store.beginTestCaptureProcessing()) {
      return
    }

    publishUiScriptCommand('trigger_stop_capture.sh')
    testProcessTimerId = window.setTimeout(() => {
      if (!store.testModeEnabled || token !== store.pcdLoadSequenceToken) {
        return
      }
      completeTestCapture(stopNumber, token)
    }, TEST_CAPTURE_LOAD_DELAY_MS)
  }

  function finishTestSequence() {
    if (!store.testModeEnabled || store.testState !== TEST_MODE_STATES.READY_FINISH) {
      return
    }

    const token = store.advancePcdLoadSequenceToken()
    const cooldownUntil = Date.now() + TEST_FINISH_COOLDOWN_MS
    store.beginTestCooldown(cooldownUntil)
    publishUiScriptCommand('end_pipe_postprocess.sh')

    testCooldownTimerId = window.setTimeout(() => {
      if (!store.testModeEnabled || token !== store.pcdLoadSequenceToken) {
        return
      }
      store.finishTestCooldown()
    }, TEST_FINISH_COOLDOWN_MS)
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
      rosService.subscribe(appConfig.topics.motionReached, (message) => {
        const value = Boolean(message.data)
        if (value && !lastMotionReachedValue) {
          store.appendNextAutoAssemblySegment()
        }
        lastMotionReachedValue = value
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
    clearTestTimers()
    unsubscribers.splice(0).forEach((unsubscribe) => unsubscribe())
  })

  return {
    patrolMode: uiPatrolMode,
    setPatrolMode,
    publishMoveCommand,
    startAutoInspection,
    markDetectDone,
    publishUiScriptCommand,
    setTestModeEnabled,
    startTestSequence,
    triggerTestCapture,
    finishTestSequence
  }
}
