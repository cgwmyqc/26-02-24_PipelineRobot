import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { storeToRefs } from 'pinia'
import { ElMessage } from 'element-plus'
import { abortInspectionSession, captureInspectionSession, finishInspectionSession, startInspectionSession } from '../api/inspectionSession'
import { TEST_MODE_STATES, useDashboardStore } from '../stores/dashboard'
import { rosService } from '../services/ros'
import { appConfig } from '../config/app'

const POINT_CLOUD_TIMEOUT_MS = 2500
const MAX_POINT_CLOUD_POINTS = 5000
const MAX_TEST_CAPTURE_FRAME_POINTS = 1800
const TEST_CAPTURE_FILTER_X_LIMIT_M = 0.7
const TEST_CAPTURE_FILTER_Y_LIMIT_M = 0.7
const TEST_CAPTURE_FILTER_Z_LIMIT_M = 0.5
const AUTO_DETECT_DONE_DELAY_MS = 3000
const MOTION_REACHED_EVENT_DEDUP_MS = 400
const TEST_START_DELAY_MS = 7000
const TEST_CAPTURE_INTEGRATION_MS = 1000
const TEST_FINISH_COOLDOWN_MS = 150000
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

function showRequestError(defaultMessage, error) {
  const status = getAxiosStatus(error)
  const message = getAxiosMessage(error)
  if (status === 401 || status === 403) {
    ElMessage.error(message || '登录态失效或接口权限被拒绝')
    return
  }
  ElMessage.error(message || defaultMessage)
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

function decodePointCloud(message, maxPoints = MAX_POINT_CLOUD_POINTS) {
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
  const step = Math.max(1, Math.ceil(totalPoints / Math.max(1, maxPoints)))
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

function filterTestCapturePoints(points) {
  if (!Array.isArray(points) || !points.length) {
    return []
  }

  return points.filter((point) => (
    Math.abs(point.x) <= TEST_CAPTURE_FILTER_X_LIMIT_M
    && Math.abs(point.y) <= TEST_CAPTURE_FILTER_Y_LIMIT_M
    && Math.abs(point.z) <= TEST_CAPTURE_FILTER_Z_LIMIT_M
  ))
}

export function useRosDashboard() {
  const store = useDashboardStore()
  const { patrolMode, pendingPatrolMode } = storeToRefs(store)

  const unsubscribers = []
  const uiPatrolMode = computed(() => pendingPatrolMode.value || patrolMode.value)
  const manualRecordingActive = ref(false)

  let streamWatchdogId = 0
  let hasLoggedPointCloudFrame = false
  let lastMotionReachedEventAt = 0
  let testStartTimerId = 0
  let testProcessTimerId = 0
  let testCooldownTimerId = 0
  let autoDetectDoneTimerId = 0
  let autoSessionId = ''
  let testSessionId = ''
  let manualSessionId = ''
  let autoFinishing = false
  let autoAwaitingReturnHome = false
  let autoCompletionArmed = false
  let currentAutoReturnHomeDoneValue = false
  let autoReturnHomeDoneConsumed = false
  let pendingTestCapture = null

  function getEnvironmentCode() {
    return store.waterDetected ? '1' : '0'
  }

  async function createSession(mode) {
    const { data } = await startInspectionSession({
      mode,
      environment: getEnvironmentCode()
    })
    return String(data?.sessionId || '').trim()
  }

  async function abortSession(sessionId) {
    if (!sessionId) {
      return
    }
    try {
      await abortInspectionSession(sessionId)
    } catch (error) {
      debugLog('abort inspection session failed', getAxiosMessage(error))
    }
  }

  function clearTestTimers() {
    window.clearTimeout(testStartTimerId)
    window.clearTimeout(testProcessTimerId)
    window.clearTimeout(testCooldownTimerId)
    testStartTimerId = 0
    testProcessTimerId = 0
    testCooldownTimerId = 0
  }

  function resetPendingTestCapture() {
    window.clearTimeout(testProcessTimerId)
    testProcessTimerId = 0
    pendingTestCapture = null
  }

  function clearAutoDetectDoneTimer() {
    window.clearTimeout(autoDetectDoneTimerId)
    autoDetectDoneTimerId = 0
  }

  function resetAutoFlowState() {
    clearAutoDetectDoneTimer()
    autoAwaitingReturnHome = false
    autoCompletionArmed = false
    lastMotionReachedEventAt = 0
    currentAutoReturnHomeDoneValue = false
    autoReturnHomeDoneConsumed = false
  }

  function buildAsciiPcd(points) {
    const normalized = Array.isArray(points)
      ? points.filter((point) => Number.isFinite(point?.x) && Number.isFinite(point?.y) && Number.isFinite(point?.z))
      : []

    if (!normalized.length) {
      return ''
    }

    const header = [
      '# .PCD v0.7 - Point Cloud Data file format',
      'VERSION 0.7',
      'FIELDS x y z',
      'SIZE 4 4 4',
      'TYPE F F F',
      'COUNT 1 1 1',
      `WIDTH ${normalized.length}`,
      'HEIGHT 1',
      'VIEWPOINT 0 0 0 1 0 0 0',
      `POINTS ${normalized.length}`,
      'DATA ascii'
    ]

    const body = normalized.map((point) => `${point.x} ${point.y} ${point.z}`)
    return `${header.join('\n')}\n${body.join('\n')}\n`
  }

  function getCurrentPointCloudPayload(mode) {
    const points = mode === '2'
      ? store.testAssemblyPoints
      : mode === '1'
        ? store.autoAssemblyPoints
        : []
    const pointCloudPcdContent = buildAsciiPcd(points)
    if (!pointCloudPcdContent) {
      return {}
    }
    return {
      pointCloudFileName: 'pointclouds.pcd',
      pointCloudPcdContent
    }
  }

  function publishManualMode(isManual) {
    rosService.publish(appConfig.topics.manualModeCommand, {
      data: isManual
    })
  }

  async function setPatrolMode(mode) {
    if (mode !== 'auto' && autoSessionId) {
      await abortSession(autoSessionId)
      autoSessionId = ''
      resetAutoFlowState()
      store.clearCompletedAutoAssembly()
      store.resetAutoAssembly()
      store.revertAnalysisStatus()
    }
    if (mode !== 'auto' && !autoSessionId) {
      store.clearCompletedAutoAssembly()
      store.resetAutoAssembly()
    }

    if (mode !== 'manual' && manualSessionId) {
      await abortSession(manualSessionId)
      manualSessionId = ''
      manualRecordingActive.value = false
      store.revertAnalysisStatus()
    }

    store.setPendingPatrolMode(mode)
    publishManualMode(mode === 'manual')
  }

  function publishMoveCommand(direction, active) {
    const topic = direction === 'forward'
      ? appConfig.topics.manualForwardCommand
      : appConfig.topics.manualReverseCommand

    rosService.publish(topic, { data: active })
  }

  async function startAutoInspection() {
    if (autoSessionId || !store.startAutoAssembly()) {
      return
    }

    try {
      autoSessionId = await createSession('1')
      if (!autoSessionId) {
        throw new Error('empty session id')
      }
      resetAutoFlowState()
      store.setAnalysisStatusChecking()
      rosService.publish(appConfig.topics.startAuto, { data: true })
    } catch (error) {
      store.resetAutoAssembly()
      resetAutoFlowState()
      debugLog('start auto inspection recording failed', getAxiosMessage(error))
      showRequestError('自动巡检录像启动失败', error)
    }
  }

  async function completeAutoInspection() {
    if (!autoSessionId || autoFinishing) {
      return
    }

    autoFinishing = true
    const sessionId = autoSessionId
    autoSessionId = ''
    resetAutoFlowState()

    try {
      const { data } = await finishInspectionSession(sessionId, {
        copyDefectImages: false,
        ...getCurrentPointCloudPayload('1')
      })
      store.completeAutoAssemblyDisplay()
      try {
        await store.syncAnalysisStatusFromInspection(data?.inspectionId)
      } catch (error) {
        debugLog('sync auto analysis status failed', getAxiosMessage(error))
      }
      await store.loadHistory({ page: 1 })
    } catch (error) {
      debugLog('finish auto inspection recording failed', getAxiosMessage(error))
      showRequestError('自动巡检录像结束失败', error)
      await abortSession(sessionId)
      store.resetAutoAssembly()
      store.revertAnalysisStatus()
    } finally {
      autoFinishing = false
    }
  }

  async function toggleManualRecording() {
    if (store.patrolMode !== 'manual') {
      return
    }

    if (manualRecordingActive.value && manualSessionId) {
      const sessionId = manualSessionId
      manualSessionId = ''
      manualRecordingActive.value = false
      try {
        const { data } = await finishInspectionSession(sessionId, { copyDefectImages: false })
        try {
          await store.syncAnalysisStatusFromInspection(data?.inspectionId)
        } catch (syncError) {
          debugLog('sync manual analysis status failed', getAxiosMessage(syncError))
        }
        await store.loadHistory({ page: 1 })
      } catch (error) {
        debugLog('finish manual recording failed', getAxiosMessage(error))
        showRequestError('人工巡检录像保存失败', error)
        await abortSession(sessionId)
        store.revertAnalysisStatus()
      }
      return
    }

    try {
      const sessionId = await createSession('0')
      if (!sessionId) {
        throw new Error('empty session id')
      }
      manualSessionId = sessionId
      manualRecordingActive.value = true
      store.setAnalysisStatusChecking()
    } catch (error) {
      manualSessionId = ''
      manualRecordingActive.value = false
      debugLog('start manual recording failed', getAxiosMessage(error))
      showRequestError('人工巡检录像启动失败', error)
    }
  }

  async function captureManualSnapshot() {
    if (!manualRecordingActive.value || !manualSessionId) {
      return
    }

    try {
      await captureInspectionSession(manualSessionId)
    } catch (error) {
      debugLog('manual capture failed', getAxiosMessage(error))
      showRequestError('人工巡检拍照失败', error)
    }
  }

  function markDetectDone() {
    rosService.publish(appConfig.topics.detectDone, { data: true })
  }

  function scheduleAutoDetectDone() {
    if (!autoSessionId || !store.autoAssemblyActive) {
      return
    }

    const isLastDetectPoint = store.autoAssemblySegmentCount >= store.autoAssemblySegmentCenters.length
    clearAutoDetectDoneTimer()
    autoDetectDoneTimerId = window.setTimeout(() => {
      autoDetectDoneTimerId = 0
      markDetectDone()
      if (isLastDetectPoint) {
        autoAwaitingReturnHome = true
        if (currentAutoReturnHomeDoneValue && !autoReturnHomeDoneConsumed) {
          handleAutoReturnHomeDoneEvent()
        }
      }
    }, AUTO_DETECT_DONE_DELAY_MS)
  }

  function handleAutoMotionReachedEvent() {
    if (
      !store.autoAssemblyActive ||
      !autoSessionId ||
      currentAutoReturnHomeDoneValue ||
      autoReturnHomeDoneConsumed
    ) {
      return
    }

    store.appendNextAutoAssemblySegment()
    autoCompletionArmed = true
    scheduleAutoDetectDone()
  }

  function handleAutoReturnHomeDoneEvent() {
    if (!autoSessionId || autoFinishing || autoReturnHomeDoneConsumed || !autoCompletionArmed) {
      return
    }

    autoReturnHomeDoneConsumed = true
    autoAwaitingReturnHome = false
    completeAutoInspection()
  }

  function publishUiScriptCommand(scriptName) {
    const normalized = String(scriptName || '').trim()
    if (!normalized) {
      return
    }

    rosService.publish(appConfig.topics.uiCallScriptCmd, { data: normalized })
  }

  async function setTestModeEnabled(enabled) {
    clearTestTimers()
    resetPendingTestCapture()

    if (!enabled) {
      if (testSessionId) {
        await abortSession(testSessionId)
        testSessionId = ''
        store.revertAnalysisStatus()
      }
      store.setTestModeEnabled(false)
      publishUiScriptCommand('stop_pipe_system.sh')
      return
    }

    store.setTestModeEnabled(true)
  }

  async function startTestSequence() {
    if (!store.testModeEnabled || store.testState !== TEST_MODE_STATES.WAITING_START) {
      return
    }

    if (!testSessionId) {
      try {
        testSessionId = await createSession('2')
        if (!testSessionId) {
          throw new Error('empty session id')
        }
      } catch (error) {
        testSessionId = ''
        debugLog('start test recording failed', getAxiosMessage(error))
        showRequestError('测试模式录像启动失败', error)
        return
      }
    }

    const token = store.advancePcdLoadSequenceToken()
    if (!store.beginTestSystemStart()) {
      return
    }

    store.setAnalysisStatusChecking()
    publishUiScriptCommand('start_pipe_system.sh')
    testStartTimerId = window.setTimeout(() => {
      if (!store.testModeEnabled || token !== store.pcdLoadSequenceToken) {
        return
      }
      store.setTestWaitingTrigger()
    }, TEST_START_DELAY_MS)
  }

  function handleTestPointCloudCaptureFrame(message) {
    if (!pendingTestCapture) {
      return
    }

    const { token, pointGroups } = pendingTestCapture
    if (!store.testModeEnabled || token !== store.pcdLoadSequenceToken) {
      resetPendingTestCapture()
      return
    }

    try {
      const points = decodePointCloud(message, MAX_TEST_CAPTURE_FRAME_POINTS)
      const filteredPoints = filterTestCapturePoints(points)
      if (filteredPoints.length) {
        pointGroups.push(filteredPoints)
      }
    } catch (error) {
      debugLog('test integration point cloud decode failed', {
        token,
        error
      })
    }
  }

  function completeTestCapture(stopNumber, token) {
    if (!pendingTestCapture || pendingTestCapture.stopNumber !== stopNumber || pendingTestCapture.token !== token) {
      return
    }

    const mergedPoints = pendingTestCapture.pointGroups.flat()
    resetPendingTestCapture()

    if (!store.testModeEnabled || token !== store.pcdLoadSequenceToken) {
      return
    }

    if (!mergedPoints.length) {
      debugLog('test capture integration finished without valid livox frames', {
        stopNumber,
        token
      })
      store.setTestWaitingTrigger()
      ElMessage.error('测试点云积分失败，1000ms 内未收到 /livox/lidar 有效点云')
      return
    }

    try {
      store.appendTestCapturePoints(stopNumber, mergedPoints)
    } catch (error) {
      debugLog('test capture integration append failed', {
        stopNumber,
        token,
        error
      })
      store.setTestWaitingTrigger()
      ElMessage.error(error?.message || '测试点云积分结果处理失败')
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

    pendingTestCapture = {
      stopNumber,
      token,
      pointGroups: []
    }
    publishUiScriptCommand('trigger_stop_capture.sh')
    testProcessTimerId = window.setTimeout(() => {
      if (!store.testModeEnabled || token !== store.pcdLoadSequenceToken) {
        return
      }
      completeTestCapture(stopNumber, token)
    }, TEST_CAPTURE_INTEGRATION_MS)
  }

  function finishTestSequence() {
    if (!store.testModeEnabled || store.testState !== TEST_MODE_STATES.READY_FINISH || !testSessionId) {
      return
    }

    const token = store.advancePcdLoadSequenceToken()
    const cooldownUntil = Date.now() + TEST_FINISH_COOLDOWN_MS
    const sessionId = testSessionId

    store.beginTestCooldown(cooldownUntil)
    publishUiScriptCommand('end_pipe_postprocess.sh')

    testCooldownTimerId = window.setTimeout(async () => {
      if (!store.testModeEnabled || token !== store.pcdLoadSequenceToken) {
        return
      }

      try {
        const { data } = await finishInspectionSession(sessionId, {
          copyDefectImages: true,
          copyFittedResult: true,
          ...getCurrentPointCloudPayload('2')
        })
        try {
          await store.syncAnalysisStatusFromInspection(data?.inspectionId)
        } catch (syncError) {
          debugLog('sync test analysis status failed', getAxiosMessage(syncError))
        }
        await store.loadHistory({ page: 1 })
      } catch (error) {
        debugLog('finish test recording failed', getAxiosMessage(error))
        showRequestError('测试模式录像保存失败', error)
        await abortSession(sessionId)
        store.revertAnalysisStatus()
      } finally {
        if (testSessionId === sessionId) {
          testSessionId = ''
        }
        if (store.testModeEnabled && token === store.pcdLoadSequenceToken) {
          store.finishTestCooldown()
        }
      }
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
        if (value) {
          const now = Date.now()
          if (now - lastMotionReachedEventAt >= MOTION_REACHED_EVENT_DEDUP_MS) {
            lastMotionReachedEventAt = now
            handleAutoMotionReachedEvent()
          }
        }
      })
    )

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.autoReturnHomeDone, (message) => {
        const value = Boolean(message.data)
        currentAutoReturnHomeDoneValue = value
        if (value && !autoReturnHomeDoneConsumed && autoCompletionArmed) {
          handleAutoReturnHomeDoneEvent()
        }
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

    unsubscribers.push(
      rosService.subscribe(appConfig.topics.testPointCloudSource, (message) => {
        handleTestPointCloudCaptureFrame(message)
      })
    )

    store.loadHistory()
  })

  onBeforeUnmount(() => {
    window.clearInterval(streamWatchdogId)
    clearTestTimers()
    clearAutoDetectDoneTimer()
    resetPendingTestCapture()

    if (autoSessionId) {
      abortSession(autoSessionId)
      autoSessionId = ''
    }
    if (testSessionId) {
      abortSession(testSessionId)
      testSessionId = ''
    }
    if (manualSessionId) {
      abortSession(manualSessionId)
      manualSessionId = ''
    }

    manualRecordingActive.value = false

    unsubscribers.splice(0).forEach((unsubscribe) => unsubscribe())
  })

  return {
    patrolMode: uiPatrolMode,
    manualRecordingActive,
    setPatrolMode,
    publishMoveCommand,
    startAutoInspection,
    toggleManualRecording,
    captureManualSnapshot,
    markDetectDone,
    publishUiScriptCommand,
    setTestModeEnabled,
    startTestSequence,
    triggerTestCapture,
    finishTestSequence
  }
}
