import { defineStore } from 'pinia'
import { exportInspectionRecord, fetchInspectionDetail, fetchInspectionHistory } from '../api/history'
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

function buildMockPoints() {
  const points = []
  for (let i = 0; i < 1800; i += 1) {
    const angle = Math.random() * Math.PI * 2
    const radius = 28 + (Math.random() - 0.5) * 2.8
    const x = Math.cos(angle) * radius
    const y = Math.sin(angle) * radius
    const z = (Math.random() - 0.5) * 8
    points.push({ x, y, z })
  }
  return points
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

export const useDashboardStore = defineStore('dashboard', {
  state: () => ({
    patrolMode: 'auto',
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
    videoFrame: '',
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
    pointCloudPoints: buildMockPoints()
  }),
  actions: {
    setRosConnected(status) {
      this.rosConnected = status
    },
    setVideoFrame(frame) {
      this.videoFrame = frame
    },
    setPointCloudPoints(points) {
      this.pointCloudPoints = Array.isArray(points) && points.length ? points : buildMockPoints()
    },
    setPatrolModeByState(isManual) {
      this.patrolMode = normalizeMode(isManual)
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
