import { defineStore } from 'pinia'
import { exportInspectionRecord, fetchInspectionDetail, fetchInspectionHistory } from '../api/history'
import { appConfig } from '../config/app'

function formatDateOnly(value) {
  if (!value) {
    return '--'
  }
  return String(value).trim().slice(0, 10)
}

function normalizeOperator(value) {
  const normalized = String(value || '').trim()
  if (!normalized) {
    return '--'
  }
  return normalized === 'operator.admin' ? 'admin' : normalized
}

function normalizeHistoryRecord(record) {
  return {
    ...record,
    operator: normalizeOperator(record?.operator),
    createdAt: formatDateOnly(record?.createdAt),
    inspectionTime: formatDateOnly(record?.inspectionTime)
  }
}

const mockHistory = [
  {
    id: 1,
    mode: '自动巡检',
    environment: '满水环境',
    operator: 'admin',
    result: '裂缝预警',
    createdAt: '2026-03-20',
    inspectionTime: '2026-03-20'
  },
  {
    id: 2,
    mode: '人工巡检',
    environment: '非满水环境',
    operator: 'admin',
    result: '未发现异常',
    createdAt: '2026-03-18',
    inspectionTime: '2026-03-18'
  }
]

const mockDetail = {
  id: 1,
  mode: '自动巡检',
  environment: '满水环境',
  operator: 'admin',
  result: '裂缝预警',
  inspectionTime: '2026-03-20',
  createdAt: '2026-03-20',
  videoUrl: '',
  anomalies: [
    {
      id: 1,
      anomalyType: '裂缝',
      remark: '管壁中段发现疑似裂缝',
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
      pageSize: 10,
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
      this.historyFilters = {
        ...this.historyFilters,
        ...params
      }

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
    async openDetail(recordId) {
      this.detailVisible = true
      this.detailLoading = true
      try {
        const { data } = await fetchInspectionDetail(recordId)
        this.detailRecord = {
          ...data,
          operator: normalizeOperator(data?.operator),
          createdAt: formatDateOnly(data?.createdAt),
          inspectionTime: formatDateOnly(data?.inspectionTime),
          videoUrl: normalizeAssetUrl(data?.videoUrl),
          anomalies: Array.isArray(data?.anomalies)
            ? data.anomalies.map((item) => ({
                ...item,
                imageUrl: normalizeAssetUrl(item?.imageUrl)
              }))
            : []
        }
      } catch (_error) {
        this.detailRecord = {
          ...mockDetail,
          id: recordId
        }
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
