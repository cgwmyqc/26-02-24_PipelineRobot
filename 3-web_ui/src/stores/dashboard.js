import { defineStore } from 'pinia'
import { exportInspectionRecord, fetchInspectionDetail, fetchInspectionHistory } from '../api/history'
import { fetchPipeDatasetFittedResult } from '../api/pointCloud'
import { appConfig } from '../config/app'

const MODE_MAP = {
  0: '手动',
  1: '自动',
  2: '测试'
}

const ENVIRONMENT_MAP = {
  0: '非满水',
  1: '满水'
}

const HISTORY_RESULT_MAP = {
  0: '无异常',
  1: '异常',
  2: '异常'
}

const DETAIL_RESULT_MAP = {
  0: '无异常',
  1: '裂缝预警',
  2: '淤泥预警'
}

const ANOMALY_MAP = {
  PL: '破裂',
  BX: '变形',
  SG: '树根',
  ZAW: '阻碍物',
  RG: '人工捕获'
}

const ANALYSIS_STATUS = Object.freeze({
  OK: 'ok',
  CHECKING: 'checking',
  WARNING: 'warning'
})

const ANALYSIS_STATUS_IMAGE_MAP = Object.freeze({
  [ANALYSIS_STATUS.OK]: 'status_ok',
  [ANALYSIS_STATUS.CHECKING]: 'status_checking',
  [ANALYSIS_STATUS.WARNING]: 'status_warning'
})

const ANALYSIS_MESSAGE_MAP = Object.freeze({
  [ANALYSIS_STATUS.OK]: ['排水管道状态良好，无异常。'],
  [ANALYSIS_STATUS.CHECKING]: ['管道状态检测中…']
})

const ANALYSIS_ANOMALY_MESSAGE_MAP = Object.freeze({
  BX: '管道变形，请及时处理！',
  PL: '管道破裂，请及时处理！',
  ZAW: '管道有阻碍物，请及时处理！',
  SG: '管道有树根，请及时处理！',
  RG: '人工识别缺陷，请及时处理！'
})

export const POINT_CLOUD_DISPLAY_MODES = Object.freeze({
  MOCK: 'mock',
  AUTO_ASSEMBLY: 'auto_assembly',
  TEST_ASSEMBLY: 'test_assembly',
  LIVE_STREAM: 'live_stream'
})

export const TEST_MODE_STATES = Object.freeze({
  IDLE: 'idle',
  WAITING_START: 'waiting_start',
  STARTING_SYSTEM: 'starting_system',
  WAITING_TRIGGER: 'waiting_trigger',
  PROCESSING_CAPTURE: 'processing_capture',
  READY_FINISH: 'ready_finish',
  COOLDOWN: 'cooldown'
})

const AUTO_TOTAL_LENGTH_M = 2.1
const AUTO_FIRST_DETECT_OFFSET_M = 0.9
const AUTO_STEP_LENGTH_M = 0.3
const TEST_CAPTURE_MAX_COUNT = 14
const TEST_CAPTURE_FIRST_Z = -2.0
const TEST_CAPTURE_Z_STEP = 0.3
const TEST_CAPTURE_X_OFFSET_M = 0
const TEST_CAPTURE_Y_OFFSET_M = 0
const DEFAULT_PIPE_POINT_COUNT = 15000
const DEFAULT_PIPE_RADIUS = 1.0
const DEFAULT_PIPE_LENGTH = 4.0
const DEFAULT_RADIAL_JITTER = 0.045
const DEFAULT_AXIAL_JITTER = 0.05
const AUTO_SEGMENT_POINT_COUNT = 1200
const AUTO_SEGMENT_LENGTH = 0.32
const AUTO_SEGMENT_AXIAL_JITTER = 0.02
const TEST_CAPTURE_MAX_POINTS = 1500
const TEST_ASSEMBLY_TOTAL_MAX_POINTS = 30000
const AUTO_SEGMENT_DIAMETER_MIN = 0.95      //label范围
const AUTO_SEGMENT_DIAMETER_MAX = 0.98
const AUTO_RESULT_DIAMETER_MIN = 0.955      //json范围
const AUTO_RESULT_DIAMETER_MAX = 0.975
const TEST_SEGMENT_DIAMETER_MIN = 0.98
const TEST_SEGMENT_DIAMETER_MAX = 1.01

function formatDateOnly(value) {
  if (!value) {
    return '--'
  }
  return String(value).trim().slice(0, 10)
}

function mapCode(value, mapping) {
  if (value === null || value === undefined || value === '') {
    return '--'
  }
  const normalized = String(value).trim()
  return mapping[normalized] ?? normalized
}

function normalizeOperator(value) {
  const normalized = String(value || '').trim()
  return normalized || '--'
}

function normalizeHistoryRecord(record) {
  return {
    ...record,
    mode: mapCode(record?.mode, MODE_MAP),
    environment: mapCode(record?.environment, ENVIRONMENT_MAP),
    operator: normalizeOperator(record?.operator),
    result: mapCode(record?.result, HISTORY_RESULT_MAP),
    createdAt: formatDateOnly(record?.createdAt),
    inspectionTime: formatDateOnly(record?.inspectionTime)
  }
}

function normalizeDetailRecord(record) {
  const videos = Array.isArray(record?.videos)
    ? record.videos.map((item) => ({
        ...item,
        fileUrl: normalizeAssetUrl(item?.fileUrl)
      }))
    : []
  const images = Array.isArray(record?.images)
    ? record.images.map((item) => ({
        ...item,
        fileUrl: normalizeAssetUrl(item?.fileUrl)
      }))
    : []
  const points = Array.isArray(record?.points)
    ? record.points.map((item) => ({
        ...item,
        fileUrl: normalizeAssetUrl(item?.fileUrl)
      }))
    : []

  const anomalies = Array.isArray(record?.anomalies)
    ? record.anomalies.map((item) => ({
        ...item,
        anomalyType: mapAnomalyType(item?.anomalyType),
        imageUrl: normalizeAssetUrl(item?.imageUrl)
      }))
    : []

  return {
    ...record,
    mode: mapCode(record?.mode, MODE_MAP),
    environment: mapCode(record?.environment, ENVIRONMENT_MAP),
    operator: normalizeOperator(record?.operator),
    result: mapCode(record?.result, DETAIL_RESULT_MAP),
    createdAt: formatDateOnly(record?.createdAt),
    inspectionTime: formatDateOnly(record?.inspectionTime),
    videoUrl: normalizeAssetUrl(record?.videoUrl),
    fittedResultUrl: normalizeAssetUrl(record?.fittedResultUrl),
    videos,
    images,
    points,
    anomalies
  }
}

function mapAnomalyType(value) {
  if (!value) {
    return '未知'
  }
  const normalized = String(value).trim().toUpperCase()
  if (!normalized) {
    return '未知'
  }

  if (ANOMALY_MAP[normalized]) {
    return ANOMALY_MAP[normalized]
  }

  if (normalized.includes('+')) {
    return normalized.split('+').map((item) => ANOMALY_MAP[item] || item).join('+')
  }

  return ANOMALY_MAP[normalized] || normalized
}

function getDefaultAnalysisState(status = ANALYSIS_STATUS.OK) {
  return {
    status,
    imageKey: ANALYSIS_STATUS_IMAGE_MAP[status],
    messages: [...(ANALYSIS_MESSAGE_MAP[status] || ANALYSIS_MESSAGE_MAP[ANALYSIS_STATUS.OK])]
  }
}

function normalizeAnomalyTokens(value) {
  const normalized = String(value || '').trim()
  if (!normalized) {
    return []
  }

  return normalized
    .split('+')
    .map((item) => String(item || '').trim().toUpperCase())
    .filter(Boolean)
}

function mapAnalysisMessagesFromAnomalies(anomalies) {
  if (!Array.isArray(anomalies) || !anomalies.length) {
    return []
  }

  const uniqueTokens = new Set()
  const messages = []

  anomalies.forEach((item) => {
    const tokens = normalizeAnomalyTokens(item?.anomalyType)
    tokens.forEach((token) => {
      if (uniqueTokens.has(token) || !ANALYSIS_ANOMALY_MESSAGE_MAP[token]) {
        return
      }
      uniqueTokens.add(token)
      messages.push(ANALYSIS_ANOMALY_MESSAGE_MAP[token])
    })
  })

  return messages
}

const mockHistory = [
  {
    id: 1,
    mode: '1',
    environment: '0',
    operator: 'admin',
    result: '1',
    createdAt: '2026-03-20',
    inspectionTime: '2026-03-20'
  },
  {
    id: 2,
    mode: '0',
    environment: '1',
    operator: 'admin',
    result: '2',
    createdAt: '2026-03-18',
    inspectionTime: '2026-03-18'
  }
]

const mockDetail = {
  id: 1,
  mode: '1',
  environment: '0',
  operator: 'admin',
  result: '1',
  inspectionTime: '2026-03-20',
  createdAt: '2026-03-20',
  videoUrl: '',
  fittedResultUrl: '',
  anomalies: [
    {
      id: 1,
      anomalyType: '1',
      remark: '管壁上方出现连续裂缝痕迹。',
      imageUrl: ''
    }
  ]
}

function buildPipeShellPoints({
  pointCount,
  radius,
  length,
  zCenter = 0,
  radialJitter = DEFAULT_RADIAL_JITTER,
  axialJitter = DEFAULT_AXIAL_JITTER
}) {
  const points = []
  for (let index = 0; index < pointCount; index += 1) {
    const angle = Math.random() * Math.PI * 2
    const radiusOffset = (Math.random() - 0.5) * radialJitter
    const lengthOffset = (Math.random() - 0.5) * axialJitter
    const surfaceWave = Math.sin((index / pointCount) * Math.PI * 14) * 0.02
    const currentRadius = radius + radiusOffset + surfaceWave
    const x = Math.cos(angle) * currentRadius
    const y = Math.sin(angle) * currentRadius
    const z = zCenter + (Math.random() - 0.5) * length + lengthOffset
    points.push({ x, y, z })
  }
  return points
}

function buildMockPoints() {
  return buildPipeShellPoints({
    pointCount: DEFAULT_PIPE_POINT_COUNT,
    radius: DEFAULT_PIPE_RADIUS,
    length: DEFAULT_PIPE_LENGTH
  })
}

function buildAutoSegmentPoints(zCenter) {
  return buildPipeShellPoints({
    pointCount: AUTO_SEGMENT_POINT_COUNT,
    radius: DEFAULT_PIPE_RADIUS,
    length: AUTO_SEGMENT_LENGTH,
    zCenter,
    axialJitter: AUTO_SEGMENT_AXIAL_JITTER
  })
}

function getAutoDetectPointCount() {
  return 1 + Math.ceil(AUTO_TOTAL_LENGTH_M / AUTO_STEP_LENGTH_M)
}

function buildAutoSegmentCenters() {
  const segmentCount = getAutoDetectPointCount()
  const midpoint = (segmentCount - 1) / 2
  return Array.from({ length: segmentCount }, (_item, index) => (index - midpoint) * AUTO_STEP_LENGTH_M)
}

function classifyInspectionResult(value) {
  return String(value || '').trim() === '0' ? '0' : '1'
}

function normalizeMode(isManual) {
  return isManual ? 'manual' : 'auto'
}

function normalizeAssetUrl(url) {
  if (!url) {
    return ''
  }
  try {
    return new URL(url, appConfig.backendBaseURL).toString()
  } catch (_error) {
    return url
  }
}

function getTestSegmentTargetZ(stopNumber) {
  return TEST_CAPTURE_FIRST_Z + (stopNumber - 1) * TEST_CAPTURE_Z_STEP
}

function reducePoints(points, targetCount) {
  if (!Array.isArray(points) || points.length <= targetCount) {
    return Array.isArray(points) ? points.slice() : []
  }

  const step = Math.max(1, Math.ceil(points.length / targetCount))
  const reduced = []
  for (let index = 0; index < points.length && reduced.length < targetCount; index += step) {
    reduced.push(points[index])
  }
  return reduced
}

function buildDefaultPointCloud() {
  return buildMockPoints()
}

function getRandomValueInRange(min, max) {
  const start = Number(min)
  const end = Number(max)
  if (!Number.isFinite(start) || !Number.isFinite(end)) {
    return 0
  }
  const lower = Math.min(start, end)
  const upper = Math.max(start, end)
  return lower + Math.random() * (upper - lower)
}

function roundToThreeDecimals(value) {
  const normalized = Number(value)
  if (!Number.isFinite(normalized)) {
    return 0
  }
  return Number(normalized.toFixed(3))
}

function createSegmentLabel(segment) {
  const centerZ = Number(segment?.centerZ)
  const diameter = Number(segment?.diameter)
  if (!Number.isFinite(centerZ) || !Number.isFinite(diameter)) {
    return null
  }

  return {
    id: String(segment?.id || `segment-${centerZ}`),
    text: `${diameter.toFixed(3)}m`,
    centerZ,
    diameter
  }
}

function getLatestSegmentLabel(segments) {
  if (!Array.isArray(segments) || !segments.length) {
    return []
  }

  const latestSegment = segments[segments.length - 1]
  const latestLabel = createSegmentLabel(latestSegment)
  return latestLabel ? [latestLabel] : []
}

function buildAutoAssemblySegment(segmentIndex, centerZ) {
  const points = buildAutoSegmentPoints(centerZ)
  return {
    id: `auto-segment-${segmentIndex + 1}`,
    index: segmentIndex + 1,
    centerZ,
    points,
    diameter: roundToThreeDecimals(getRandomValueInRange(AUTO_SEGMENT_DIAMETER_MIN, AUTO_SEGMENT_DIAMETER_MAX))
  }
}

function buildTestAssemblySegment(stopNumber, points, targetZ) {
  return {
    id: `test-segment-${stopNumber}`,
    index: stopNumber,
    centerZ: targetZ,
    points,
    diameter: roundToThreeDecimals(getRandomValueInRange(TEST_SEGMENT_DIAMETER_MIN, TEST_SEGMENT_DIAMETER_MAX))
  }
}

function buildAutoFittedResult(diameter) {
  const normalizedDiameter = roundToThreeDecimals(diameter)
  const zStart = 0
  const zEnd = roundToThreeDecimals(AUTO_TOTAL_LENGTH_M)

  return {
    R_global_m: roundToThreeDecimals(normalizedDiameter / 2),
    z_range_m: [zStart, zEnd],
    pipe_length_m: roundToThreeDecimals(AUTO_TOTAL_LENGTH_M),
    defects: []
  }
}

function validateAutoFittedResult(result) {
  if (!result || typeof result !== 'object') {
    throw new Error('Auto fitted result is invalid')
  }

  const radius = Number(result.R_global_m)
  const pipeLength = Number(result.pipe_length_m)
  const zRange = Array.isArray(result.z_range_m) ? result.z_range_m : []
  const zStart = Number(zRange[0])
  const zEnd = Number(zRange[1])
  const defects = Array.isArray(result.defects) ? result.defects : null

  if (!Number.isFinite(radius)) {
    throw new Error('Auto fitted result radius is invalid')
  }
  if (!Number.isFinite(pipeLength)) {
    throw new Error('Auto fitted result pipe length is invalid')
  }
  if (zRange.length !== 2 || !Number.isFinite(zStart) || !Number.isFinite(zEnd)) {
    throw new Error('Auto fitted result z_range_m is invalid')
  }
  if (zStart !== 0 || zEnd !== pipeLength) {
    throw new Error('Auto fitted result z_range_m does not match pipe length')
  }
  if (!defects || defects.length) {
    throw new Error('Auto fitted result defects must be empty')
  }

  return {
    R_global_m: radius,
    z_range_m: [zStart, zEnd],
    pipe_length_m: pipeLength,
    defects: []
  }
}

const INITIAL_DEFAULT_MOCK_POINT_CLOUD = buildDefaultPointCloud()
const INITIAL_AUTO_SEGMENT_CENTERS = Object.freeze(buildAutoSegmentCenters())

export const useDashboardStore = defineStore('dashboard', {
  state: () => ({
    patrolMode: 'auto',
    pendingPatrolMode: '',
    temperature: null,
    humidity: null,
    sludgeThickness: null,
    waterDetected: false,
    rosConnected: false,
    motorEnabled: false,
    motorRunState: 0,
    encoderCount: 0,
    travelMeters: 0,
    laserDistance1Mm: -1,
    laserDistance2Mm: -1,
    laserDistance3Mm: -1,
    motionReached: false,
    historyList: [],
    historyLoading: false,
    historyFilters: {
      mode: '',
      environment: '',
      startDate: '',
      endDate: ''
    },
    historyPagination: {
      page: 1,
      pageSize: 5,
      total: 0
    },
    detailVisible: false,
    detailLoading: false,
    detailRecord: null,
    exportingId: null,
    defaultMockPointCloud: INITIAL_DEFAULT_MOCK_POINT_CLOUD,
    pointCloudPoints: INITIAL_DEFAULT_MOCK_POINT_CLOUD,
    latestLivePointCloudPoints: [],
    pointCloudDisplayMode: POINT_CLOUD_DISPLAY_MODES.MOCK,
    pointCloudStreamActive: false,
    pointCloudLastMessageAt: 0,
    autoAssemblyActive: false,
    autoAssemblySegmentCount: 0,
    autoAssemblyPoints: [],
    autoAssemblySegments: [],
    completedAutoAssemblyPoints: [],
    completedAutoAssemblySegments: [],
    autoAssemblySegmentCenters: [...INITIAL_AUTO_SEGMENT_CENTERS],
    testModeEnabled: false,
    testState: TEST_MODE_STATES.IDLE,
    triggerCount: 0,
    cooldownUntil: 0,
    testAssemblyPoints: [],
    testAssemblySegments: [],
    pointCloudSegmentLabels: [],
    pcdLoadSequenceToken: 0,
    fitViewEnabled: false,
    fitViewAvailable: false,
    fittedPipeData: null,
    autoGeneratedFittedPipeData: null,
    fittedPipeLoading: false,
    fittedPipeLoadError: '',
    analysisStatus: ANALYSIS_STATUS.OK,
    analysisImageKey: ANALYSIS_STATUS_IMAGE_MAP[ANALYSIS_STATUS.OK],
    analysisMessages: [...ANALYSIS_MESSAGE_MAP[ANALYSIS_STATUS.OK]],
    lastSettledAnalysisStatus: ANALYSIS_STATUS.OK,
    lastSettledAnalysisImageKey: ANALYSIS_STATUS_IMAGE_MAP[ANALYSIS_STATUS.OK],
    lastSettledAnalysisMessages: [...ANALYSIS_MESSAGE_MAP[ANALYSIS_STATUS.OK]]
  }),
  actions: {
    setRosConnected(status) {
      this.rosConnected = status
    },
    applyAnalysisState(state, { settle = false } = {}) {
      const normalizedState = {
        ...getDefaultAnalysisState(state?.status || ANALYSIS_STATUS.OK),
        ...state,
        messages: Array.isArray(state?.messages) && state.messages.length
          ? state.messages.slice()
          : getDefaultAnalysisState(state?.status || ANALYSIS_STATUS.OK).messages
      }

      this.analysisStatus = normalizedState.status
      this.analysisImageKey = normalizedState.imageKey
      this.analysisMessages = normalizedState.messages

      if (settle) {
        this.lastSettledAnalysisStatus = normalizedState.status
        this.lastSettledAnalysisImageKey = normalizedState.imageKey
        this.lastSettledAnalysisMessages = normalizedState.messages.slice()
      }
    },
    resetAnalysisStatusToOk() {
      this.applyAnalysisState(getDefaultAnalysisState(ANALYSIS_STATUS.OK), { settle: true })
    },
    setAnalysisStatusChecking() {
      this.applyAnalysisState(getDefaultAnalysisState(ANALYSIS_STATUS.CHECKING))
    },
    revertAnalysisStatus() {
      this.applyAnalysisState({
        status: this.lastSettledAnalysisStatus,
        imageKey: this.lastSettledAnalysisImageKey,
        messages: this.lastSettledAnalysisMessages
      })
    },
    setAnalysisStatusFromInspectionDetail(detailRecord) {
      if (!Array.isArray(detailRecord?.anomalies)) {
        return false
      }

      const messages = mapAnalysisMessagesFromAnomalies(detailRecord?.anomalies)
      if (!messages.length) {
        this.resetAnalysisStatusToOk()
        return true
      }

      this.applyAnalysisState({
        status: ANALYSIS_STATUS.WARNING,
        imageKey: ANALYSIS_STATUS_IMAGE_MAP[ANALYSIS_STATUS.WARNING],
        messages
      }, { settle: true })
      return true
    },
    async syncAnalysisStatusFromInspection(inspectionId) {
      const normalizedId = Number(inspectionId)
      if (!normalizedId) {
        return false
      }

      const { data } = await fetchInspectionDetail(normalizedId)
      if (!data || typeof data !== 'object') {
        return false
      }

      return this.setAnalysisStatusFromInspectionDetail(data)
    },
    updatePointCloudSegmentLabels(labels = []) {
      this.pointCloudSegmentLabels = Array.isArray(labels)
        ? labels.filter(Boolean)
        : []
    },
    applyCurrentPointCloudDisplay() {
      if (this.testModeEnabled) {
        this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.TEST_ASSEMBLY
        this.pointCloudPoints = this.testAssemblyPoints.slice()
        this.updatePointCloudSegmentLabels(getLatestSegmentLabel(this.testAssemblySegments))
        return
      }

      if (this.autoAssemblyActive) {
        this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.AUTO_ASSEMBLY
        this.pointCloudPoints = this.autoAssemblyPoints.slice()
        this.updatePointCloudSegmentLabels(getLatestSegmentLabel(this.autoAssemblySegments))
        return
      }

      if (this.patrolMode === 'auto' && this.completedAutoAssemblyPoints.length) {
        this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.AUTO_ASSEMBLY
        this.pointCloudPoints = this.completedAutoAssemblyPoints.slice()
        this.updatePointCloudSegmentLabels(getLatestSegmentLabel(this.completedAutoAssemblySegments))
        return
      }

      if (this.pointCloudStreamActive && this.latestLivePointCloudPoints.length) {
        this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.LIVE_STREAM
        this.pointCloudPoints = this.latestLivePointCloudPoints.slice()
        this.updatePointCloudSegmentLabels([])
        return
      }

      this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.MOCK
      this.pointCloudPoints = this.defaultMockPointCloud
      this.updatePointCloudSegmentLabels([])
    },
    restoreDefaultPointCloud() {
      this.applyCurrentPointCloudDisplay()
    },
    regenerateDefaultMockPointCloud() {
      this.defaultMockPointCloud = buildDefaultPointCloud()
      if (this.pointCloudDisplayMode === POINT_CLOUD_DISPLAY_MODES.MOCK) {
        this.pointCloudPoints = this.defaultMockPointCloud
        this.updatePointCloudSegmentLabels([])
      }
    },
    setPointCloudPoints(points) {
      const normalized = Array.isArray(points) && points.length ? points : []
      if (!normalized.length) {
        return
      }

      this.latestLivePointCloudPoints = normalized
      if (
        this.pointCloudDisplayMode === POINT_CLOUD_DISPLAY_MODES.MOCK
        || this.pointCloudDisplayMode === POINT_CLOUD_DISPLAY_MODES.LIVE_STREAM
      ) {
        this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.LIVE_STREAM
        this.pointCloudPoints = normalized
        this.updatePointCloudSegmentLabels([])
      }
    },
    setPointCloudStreamActive(active) {
      this.pointCloudStreamActive = Boolean(active)
      if (!active) {
        this.latestLivePointCloudPoints = []
        this.pointCloudLastMessageAt = 0
        if (
          this.pointCloudDisplayMode === POINT_CLOUD_DISPLAY_MODES.LIVE_STREAM
          || this.pointCloudDisplayMode === POINT_CLOUD_DISPLAY_MODES.MOCK
        ) {
          this.restoreDefaultPointCloud()
        }
      }
    },
    markPointCloudMessageReceived(timestamp = Date.now()) {
      this.pointCloudLastMessageAt = timestamp
      this.pointCloudStreamActive = true
    },
    resetRealtimeStreams() {
      this.pointCloudStreamActive = false
      this.pointCloudLastMessageAt = 0
      this.latestLivePointCloudPoints = []
      this.laserDistance1Mm = -1
      this.laserDistance2Mm = -1
      this.laserDistance3Mm = -1
      this.restoreDefaultPointCloud()
    },
    startAutoAssembly() {
      if (this.testModeEnabled) {
        return false
      }

      this.completedAutoAssemblyPoints = []
      this.completedAutoAssemblySegments = []
      this.autoAssemblyActive = true
      this.autoAssemblySegmentCount = 0
      this.autoAssemblyPoints = []
      this.autoAssemblySegments = []
      this.autoAssemblySegmentCenters = buildAutoSegmentCenters()
      this.autoGeneratedFittedPipeData = null
      this.resetFitViewState(true)
      this.applyCurrentPointCloudDisplay()
      return true
    },
    appendNextAutoAssemblySegment() {
      if (!this.autoAssemblyActive || this.autoAssemblySegmentCount >= this.autoAssemblySegmentCenters.length) {
        return false
      }

      const centerZ = this.autoAssemblySegmentCenters[this.autoAssemblySegmentCount]
      const segment = buildAutoAssemblySegment(this.autoAssemblySegmentCount, centerZ)
      this.autoAssemblyPoints = this.autoAssemblyPoints.concat(segment.points)
      this.autoAssemblySegments = this.autoAssemblySegments.concat(segment)
      this.autoAssemblySegmentCount += 1
      this.applyCurrentPointCloudDisplay()
      return true
    },
    completeAutoAssemblyDisplay(fittedResultData = null) {
      this.completedAutoAssemblyPoints = this.autoAssemblyPoints.slice()
      this.completedAutoAssemblySegments = this.autoAssemblySegments.slice()
      this.autoAssemblyActive = false
      this.autoAssemblySegmentCount = 0
      this.autoAssemblyPoints = []
      this.autoAssemblySegments = []
      this.autoAssemblySegmentCenters = buildAutoSegmentCenters()
      this.autoGeneratedFittedPipeData = fittedResultData && typeof fittedResultData === 'object'
        ? JSON.parse(JSON.stringify(fittedResultData))
        : null
      this.fitViewAvailable = Boolean(this.autoGeneratedFittedPipeData)
      this.fittedPipeData = this.autoGeneratedFittedPipeData
        ? JSON.parse(JSON.stringify(this.autoGeneratedFittedPipeData))
        : null
      this.applyCurrentPointCloudDisplay()
    },
    clearCompletedAutoAssembly() {
      this.completedAutoAssemblyPoints = []
      this.completedAutoAssemblySegments = []
      this.autoGeneratedFittedPipeData = null
    },
    resetAutoAssembly(restoreDisplay = true) {
      this.autoAssemblyActive = false
      this.autoAssemblySegmentCount = 0
      this.autoAssemblyPoints = []
      this.autoAssemblySegments = []
      this.autoAssemblySegmentCenters = buildAutoSegmentCenters()
      this.autoGeneratedFittedPipeData = null
      this.resetFitViewState(true)
      if (restoreDisplay) {
        this.restoreDefaultPointCloud()
      }
    },
    buildAutoInspectionFittedResult() {
      const diameter = roundToThreeDecimals(getRandomValueInRange(AUTO_RESULT_DIAMETER_MIN, AUTO_RESULT_DIAMETER_MAX))
      return validateAutoFittedResult(buildAutoFittedResult(diameter))
    },
    setTestModeEnabled(enabled) {
      const nextValue = Boolean(enabled)
      this.pcdLoadSequenceToken += 1
      this.cooldownUntil = 0
      this.resetFitViewState(true)

      if (nextValue) {
        this.resetAutoAssembly(false)
        this.clearCompletedAutoAssembly()
        this.testModeEnabled = true
        this.testState = TEST_MODE_STATES.WAITING_START
        this.triggerCount = 0
        this.testAssemblyPoints = []
        this.testAssemblySegments = []
        this.applyCurrentPointCloudDisplay()
        return
      }

      this.testModeEnabled = false
      this.testState = TEST_MODE_STATES.IDLE
      this.triggerCount = 0
      this.testAssemblyPoints = []
      this.testAssemblySegments = []
      this.restoreDefaultPointCloud()
    },
    advancePcdLoadSequenceToken() {
      this.pcdLoadSequenceToken += 1
      return this.pcdLoadSequenceToken
    },
    beginTestSystemStart() {
      if (!this.testModeEnabled) {
        return false
      }
      this.testState = TEST_MODE_STATES.STARTING_SYSTEM
      this.applyCurrentPointCloudDisplay()
      return true
    },
    setTestWaitingTrigger() {
      if (this.testModeEnabled) {
        this.testState = TEST_MODE_STATES.WAITING_TRIGGER
      }
    },
    beginTestCaptureProcessing() {
      if (!this.testModeEnabled) {
        return false
      }
      this.testState = TEST_MODE_STATES.PROCESSING_CAPTURE
      return true
    },
    resetTestAssemblyForNextRun() {
      this.testAssemblyPoints = []
      this.testAssemblySegments = []
      this.applyCurrentPointCloudDisplay()
      this.resetFitViewState(true)
    },
    appendTestCapturePoints(stopNumber, points) {
      const normalizedPoints = Array.isArray(points)
        ? points.filter((point) => Number.isFinite(point?.x) && Number.isFinite(point?.y) && Number.isFinite(point?.z))
        : []

      if (!normalizedPoints.length) {
        throw new Error('Integrated point cloud contains no valid points')
      }

      const reducedPoints = reducePoints(normalizedPoints, TEST_CAPTURE_MAX_POINTS)
      const targetZ = getTestSegmentTargetZ(stopNumber)
      const translatedPoints = reducedPoints.map((point) => ({
        x: point.x + TEST_CAPTURE_X_OFFSET_M,
        y: point.y + TEST_CAPTURE_Y_OFFSET_M,
        z: point.z + targetZ
      }))
      const segment = buildTestAssemblySegment(stopNumber, translatedPoints, targetZ)

      this.testAssemblyPoints = reducePoints(
        this.testAssemblyPoints.concat(translatedPoints),
        TEST_ASSEMBLY_TOTAL_MAX_POINTS
      )
      this.testAssemblySegments = this.testAssemblySegments
        .filter((item) => item.index !== stopNumber)
        .concat(segment)
        .sort((left, right) => left.index - right.index)
      this.triggerCount = stopNumber
      this.applyCurrentPointCloudDisplay()
      this.testState = stopNumber >= TEST_CAPTURE_MAX_COUNT
        ? TEST_MODE_STATES.READY_FINISH
        : TEST_MODE_STATES.WAITING_TRIGGER
      return translatedPoints.length
    },
    beginTestCooldown(untilTimestamp) {
      this.testState = TEST_MODE_STATES.COOLDOWN
      this.cooldownUntil = Number(untilTimestamp || 0)
      this.fitViewAvailable = true
    },
    finishTestCooldown() {
      if (!this.testModeEnabled) {
        return
      }

      this.triggerCount = 0
      this.cooldownUntil = 0
      this.applyCurrentPointCloudDisplay()
      this.testState = TEST_MODE_STATES.WAITING_TRIGGER
      this.fitViewAvailable = true
      this.fitViewEnabled = false
    },
    resetFitViewState(clearData = false) {
      this.fitViewEnabled = false
      this.fitViewAvailable = false
      this.fittedPipeLoading = false
      this.fittedPipeLoadError = ''
      if (clearData) {
        this.fittedPipeData = null
      }
    },
    async setFitViewEnabled(enabled) {
      const nextValue = Boolean(enabled)

      if (!nextValue) {
        this.fitViewEnabled = false
        this.fittedPipeLoading = false
        this.fittedPipeLoadError = ''
        return true
      }

      if (!this.fitViewAvailable) {
        return false
      }

      this.fitViewEnabled = true
      this.fittedPipeLoadError = ''
      if (this.fittedPipeData || this.fittedPipeLoading) {
        return true
      }

      if (!this.testModeEnabled && this.autoGeneratedFittedPipeData) {
        this.fittedPipeData = JSON.parse(JSON.stringify(this.autoGeneratedFittedPipeData))
        return true
      }

      this.fittedPipeLoading = true
      try {
        this.fittedPipeData = await fetchPipeDatasetFittedResult()
        return true
      } catch (error) {
        this.fittedPipeLoadError = error?.response?.data?.message || error?.message || '拟合结果加载失败'
        return false
      } finally {
        this.fittedPipeLoading = false
      }
    },
    setPatrolModeByState(isManual) {
      const nextMode = normalizeMode(isManual)
      const previousMode = this.patrolMode
      this.patrolMode = nextMode
      this.pendingPatrolMode = ''
      if (isManual && previousMode !== 'manual') {
        this.resetAutoAssembly(false)
        this.clearCompletedAutoAssembly()
        if (!this.testModeEnabled) {
          this.restoreDefaultPointCloud()
        }
      }
    },
    setPendingPatrolMode(mode) {
      this.pendingPatrolMode = mode
    },
    updateRosMetric(key, value) {
      this[key] = value
    },
    async loadHistory(params = {}) {
      this.historyLoading = true
      const shouldResetPage = Object.prototype.hasOwnProperty.call(params, 'mode')
        || Object.prototype.hasOwnProperty.call(params, 'environment')
        || Object.prototype.hasOwnProperty.call(params, 'startDate')
        || Object.prototype.hasOwnProperty.call(params, 'endDate')

      this.historyFilters = {
        ...this.historyFilters,
        mode: params.mode ?? this.historyFilters.mode,
        environment: params.environment ?? this.historyFilters.environment,
        startDate: params.startDate ?? this.historyFilters.startDate,
        endDate: params.endDate ?? this.historyFilters.endDate
      }
      this.historyPagination.page = shouldResetPage
        ? 1
        : Number(params.page ?? this.historyPagination.page)
      this.historyPagination.pageSize = 5

      try {
        const requestParams = {
          ...this.historyFilters,
          page: this.historyPagination.page,
          pageSize: this.historyPagination.pageSize
        }
        const { data } = await fetchInspectionHistory(requestParams)
        const records = Array.isArray(data?.records)
          ? data.records
          : Array.isArray(data?.items)
            ? data.items
            : Array.isArray(data)
              ? data
              : mockHistory

        this.historyList = records.map(normalizeHistoryRecord)
        this.historyPagination.total = Number(data?.total ?? records.length)
      } catch (_error) {
        this.historyList = mockHistory.map(normalizeHistoryRecord)
        this.historyPagination.total = mockHistory.length
      } finally {
        this.historyLoading = false
      }
    },
    changeHistoryPage(page) {
      this.loadHistory({ page })
    },
    async openDetail(recordId) {
      this.detailVisible = true
      this.detailLoading = true
      try {
        const { data } = await fetchInspectionDetail(recordId)
        this.detailRecord = normalizeDetailRecord(data)
      } catch (_error) {
        this.detailRecord = normalizeDetailRecord({
          ...mockDetail,
          id: recordId
        })
      } finally {
        this.detailLoading = false
      }
    },
    closeDetail() {
      this.detailVisible = false
      this.detailRecord = null
    },
    async exportRecord(recordId) {
      this.exportingId = recordId
      try {
        const response = await exportInspectionRecord(recordId)
        const contentDisposition = response.headers['content-disposition'] || ''
        const match = contentDisposition.match(/filename="?([^"]+)"?/)
        const filename = match?.[1] || `inspection-${recordId}.zip`
        const blob = new Blob([response.data], { type: 'application/zip' })
        const url = window.URL.createObjectURL(blob)
        const link = document.createElement('a')
        link.href = url
        link.download = filename
        document.body.appendChild(link)
        link.click()
        document.body.removeChild(link)
        window.URL.revokeObjectURL(url)
      } finally {
        this.exportingId = null
      }
    }
  }
})

export {
  AUTO_TOTAL_LENGTH_M,
  AUTO_FIRST_DETECT_OFFSET_M,
  AUTO_STEP_LENGTH_M,
  ANALYSIS_STATUS,
  getAutoDetectPointCount,
  classifyInspectionResult
}
