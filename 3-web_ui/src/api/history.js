import request from '../utils/request'

export function fetchInspectionHistory(params) {
  return request.get('/inspection/history', { params })
}
