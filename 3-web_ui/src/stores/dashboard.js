import { defineStore } from 'pinia'
import { PCDLoader } from 'three/examples/jsm/loaders/PCDLoader.js'
import { exportInspectionRecord, fetchInspectionDetail, fetchInspectionHistory } from '../api/history'
import { fetchPipeDatasetFittedResult, fetchPipeDatasetPointCloud } from '../api/pointCloud'
import { appConfig } from '../config/app'

const MODE_MAP = {
  0: '手动',
  1: '自动'
}

const ENVIRONMENT_MAP = {
  0: '非满水',
  1: '满水'
}

const RESULT_MAP = {
  0: '无异常',
  1: '裂缝预警',
  2: '淤泥预警'
}

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

const AUTO_SEGMENT_CENTERS = Object.freeze([-0.6, -0.3, 0.0, 0.3, 0.6])
const TEST_CAPTURE_MAX_COUNT = 14
const TEST_CAPTURE_FIRST_Z = -2.0
const TEST_CAPTURE_Z_STEP = 0.3
const DEFAULT_PIPE_POINT_COUNT = 15000
const DEFAULT_PIPE_RADIUS = 1.0
const DEFAULT_PIPE_LENGTH = 4.0
const DEFAULT_RADIAL_JITTER = 0.045
const DEFAULT_AXIAL_JITTER = 0.05
const AUTO_SEGMENT_POINT_COUNT = 1200
const AUTO_SEGMENT_LENGTH = 0.32
const AUTO_SEGMENT_AXIAL_JITTER = 0.02
const MAX_IMPORTED_PCD_POINTS = 5000

const pcdLoader = new PCDLoader()

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
    result: mapCode(record?.result, RESULT_MAP),
    createdAt: formatDateOnly(record?.createdAt),
    inspectionTime: formatDateOnly(record?.inspectionTime)
  }
}

function normalizeDetailRecord(record) {
  return {
    ...record,
    mode: mapCode(record?.mode, MODE_MAP),
    environment: mapCode(record?.environment, ENVIRONMENT_MAP),
    operator: normalizeOperator(record?.operator),
    result: mapCode(record?.result, RESULT_MAP),
    createdAt: formatDateOnly(record?.createdAt),
    inspectionTime: formatDateOnly(record?.inspectionTime),
    videoUrl: normalizeAssetUrl(record?.videoUrl),
    anomalies: Array.isArray(record?.anomalies)
      ? record.anomalies.map((item) => ({
          ...item,
          anomalyType: mapCode(item?.anomalyType, RESULT_MAP),
          imageUrl: normalizeAssetUrl(item?.imageUrl)
        }))
      : []
  }
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

function formatStopId(stopNumber) {
  return `stop_${String(stopNumber).padStart(4, '0')}`
}

function getTestSegmentTargetZ(stopNumber) {
  return TEST_CAPTURE_FIRST_Z + (stopNumber - 1) * TEST_CAPTURE_Z_STEP
}

function centerPointsAtOrigin(points) {
  if (!Array.isArray(points) || !points.length) {
    return []
  }

  let minX = Infinity
  let maxX = -Infinity
  let minY = Infinity
  let maxY = -Infinity
  let minZ = Infinity
  let maxZ = -Infinity

  points.forEach((point) => {
    minX = Math.min(minX, point.x)
    maxX = Math.max(maxX, point.x)
    minY = Math.min(minY, point.y)
    maxY = Math.max(maxY, point.y)
    minZ = Math.min(minZ, point.z)
    maxZ = Math.max(maxZ, point.z)
  })

  const centerX = (minX + maxX) / 2
  const centerY = (minY + maxY) / 2
  const centerZ = (minZ + maxZ) / 2

  return points.map((point) => ({
    x: point.x - centerX,
    y: point.y - centerY,
    z: point.z - centerZ
  }))
}

function parsePcdArrayBuffer(arrayBuffer) {
  const parsed = pcdLoader.parse(arrayBuffer, '')
  const positionAttribute = parsed?.geometry?.getAttribute('position')
  if (!positionAttribute?.array?.length) {
    return []
  }

  const step = Math.max(1, Math.ceil(positionAttribute.count / MAX_IMPORTED_PCD_POINTS))
  const points = []

  for (let index = 0; index < positionAttribute.count; index += step) {
    const offset = index * 3
    const x = positionAttribute.array[offset]
    const y = positionAttribute.array[offset + 1]
    const z = positionAttribute.array[offset + 2]

    if (Number.isFinite(x) && Number.isFinite(y) && Number.isFinite(z)) {
      points.push({ x, y, z })
    }
  }

  return points
}

function buildDefaultPointCloud() {
  return buildMockPoints()
}

const INITIAL_DEFAULT_MOCK_POINT_CLOUD = buildDefaultPointCloud()

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
    autoAssemblySegmentCenters: [...AUTO_SEGMENT_CENTERS],
    testModeEnabled: false,
    testState: TEST_MODE_STATES.IDLE,
    triggerCount: 0,
    cooldownUntil: 0,
    testAssemblyPoints: [],
    pcdLoadSequenceToken: 0,
    fitViewEnabled: false,
    fitViewAvailable: false,
    fittedPipeData: null,
    fittedPipeLoading: false,
    fittedPipeLoadError: ''
  }),
  actions: {
    setRosConnected(status) {
      this.rosConnected = status
    },
    restoreDefaultPointCloud() {
      if (this.testModeEnabled) {
        this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.TEST_ASSEMBLY
        this.pointCloudPoints = this.testAssemblyPoints.slice()
        return
      }

      if (this.autoAssemblyActive) {
        this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.AUTO_ASSEMBLY
        this.pointCloudPoints = this.autoAssemblyPoints.slice()
        return
      }

      if (this.pointCloudStreamActive && this.latestLivePointCloudPoints.length) {
        this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.LIVE_STREAM
        this.pointCloudPoints = this.latestLivePointCloudPoints.slice()
        return
      }

      this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.MOCK
      this.pointCloudPoints = this.defaultMockPointCloud
    },
    regenerateDefaultMockPointCloud() {
      this.defaultMockPointCloud = buildDefaultPointCloud()
      if (this.pointCloudDisplayMode === POINT_CLOUD_DISPLAY_MODES.MOCK) {
        this.pointCloudPoints = this.defaultMockPointCloud
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
      this.restoreDefaultPointCloud()
    },
    startAutoAssembly() {
      if (this.testModeEnabled) {
        return false
      }

      this.autoAssemblyActive = true
      this.autoAssemblySegmentCount = 0
      this.autoAssemblyPoints = []
      this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.AUTO_ASSEMBLY
      this.pointCloudPoints = []
      return true
    },
    appendNextAutoAssemblySegment() {
      if (!this.autoAssemblyActive || this.autoAssemblySegmentCount >= this.autoAssemblySegmentCenters.length) {
        return false
      }

      const centerZ = this.autoAssemblySegmentCenters[this.autoAssemblySegmentCount]
      const segmentPoints = buildAutoSegmentPoints(centerZ)
      this.autoAssemblyPoints = this.autoAssemblyPoints.concat(segmentPoints)
      this.autoAssemblySegmentCount += 1
      this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.AUTO_ASSEMBLY
      this.pointCloudPoints = this.autoAssemblyPoints.slice()
      return true
    },
    resetAutoAssembly(restoreDisplay = true) {
      this.autoAssemblyActive = false
      this.autoAssemblySegmentCount = 0
      this.autoAssemblyPoints = []
      if (restoreDisplay) {
        this.restoreDefaultPointCloud()
      }
    },
    setTestModeEnabled(enabled) {
      const nextValue = Boolean(enabled)
      this.pcdLoadSequenceToken += 1
      this.cooldownUntil = 0
      this.resetFitViewState(true)

      if (nextValue) {
        this.resetAutoAssembly(false)
        this.testModeEnabled = true
        this.testState = TEST_MODE_STATES.WAITING_START
        this.triggerCount = 0
        this.testAssemblyPoints = []
        this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.TEST_ASSEMBLY
        this.pointCloudPoints = []
        return
      }

      this.testModeEnabled = false
      this.testState = TEST_MODE_STATES.IDLE
      this.triggerCount = 0
      this.testAssemblyPoints = []
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
      this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.TEST_ASSEMBLY
      this.pointCloudPoints = []
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
      this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.TEST_ASSEMBLY
      this.pointCloudPoints = []
      this.resetFitViewState(true)
    },
    async loadAndAppendTestCapture(stopNumber) {
      const stopId = formatStopId(stopNumber)
      const arrayBuffer = await fetchPipeDatasetPointCloud(stopId)
      const parsedPoints = parsePcdArrayBuffer(arrayBuffer)
      if (!parsedPoints.length) {
        throw new Error(`PCD file ${stopId} contains no valid points`)
      }

      const centeredPoints = centerPointsAtOrigin(parsedPoints)
      const targetZ = getTestSegmentTargetZ(stopNumber)
      const translatedPoints = centeredPoints.map((point) => ({
        x: point.x,
        y: point.y,
        z: point.z + targetZ
      }))

      this.testAssemblyPoints = this.testAssemblyPoints.concat(translatedPoints)
      this.triggerCount = stopNumber
      this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.TEST_ASSEMBLY
      this.pointCloudPoints = this.testAssemblyPoints.slice()
      this.testState = stopNumber >= TEST_CAPTURE_MAX_COUNT
        ? TEST_MODE_STATES.READY_FINISH
        : TEST_MODE_STATES.WAITING_TRIGGER
      return translatedPoints.length
    },
    beginTestCooldown(untilTimestamp) {
      this.testState = TEST_MODE_STATES.COOLDOWN
      this.cooldownUntil = Number(untilTimestamp || 0)
    },
    finishTestCooldown() {
      if (!this.testModeEnabled) {
        return
      }

      this.triggerCount = 0
      this.cooldownUntil = 0
      this.pointCloudDisplayMode = POINT_CLOUD_DISPLAY_MODES.TEST_ASSEMBLY
      this.pointCloudPoints = this.testAssemblyPoints.slice()
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
