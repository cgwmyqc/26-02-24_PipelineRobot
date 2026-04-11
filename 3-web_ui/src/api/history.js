import request from '../utils/request'

export function fetchInspectionHistory(params) {
  return request.get('/inspection/history', { params })
}

export function fetchInspectionDetail(id) {
  return request.get(`/inspection/${id}`)
}

export function exportInspectionRecord(id) {
  return request.get(`/inspection/${id}/export`, {
    responseType: 'blob'
  })
}
