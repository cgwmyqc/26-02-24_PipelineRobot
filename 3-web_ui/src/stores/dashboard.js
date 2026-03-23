import { defineStore } from 'pinia'
import { fetchInspectionHistory } from '../api/history'

const mockHistory = [
  { id: 1, mode: '自动巡检', environment: '满水环境', operator: 'xiaoshuai.lv', result: '裂缝预警', createdAt: '2025-02-01' },
  { id: 2, mode: '自动巡检', environment: '非满水环境', operator: 'xiaoshuai.lv', result: '未发现异常', createdAt: '2024-08-06' },
  { id: 3, mode: '人工巡检', environment: '非满水环境', operator: 'xiaoshuai.lv', result: '未发现异常', createdAt: '2024-05-12' },
  { id: 4, mode: '自动巡检', environment: '半淤积环境', operator: 'xiaoshuai.lv', result: '管道拥堵', createdAt: '2024-02-18' }
]

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

export const useDashboardStore = defineStore('dashboard', {
  state: () => ({
    patrolMode: 'auto',
    temperature: 23,
    sludgeThickness: 15,
    rosConnected: false,
    videoFrame: '',
    historyList: [],
    historyLoading: false,
    pointCloudPoints: buildMockPoints()
  }),
  actions: {
    setPatrolMode(mode) {
      this.patrolMode = mode
    },
    updateMetric(key, value) {
      this[key] = value
    },
    setRosConnected(status) {
      this.rosConnected = status
    },
    setVideoFrame(frame) {
      this.videoFrame = frame
    },
    setPointCloudPoints(points) {
      this.pointCloudPoints = Array.isArray(points) && points.length ? points : buildMockPoints()
    },
    async loadHistory(params = {}) {
      this.historyLoading = true
      try {
        const { data } = await fetchInspectionHistory(params)
        const list = Array.isArray(data?.records) ? data.records : Array.isArray(data) ? data : mockHistory
        this.historyList = list
      } catch (error) {
        this.historyList = mockHistory
      } finally {
        this.historyLoading = false
      }
    }
  }
})
