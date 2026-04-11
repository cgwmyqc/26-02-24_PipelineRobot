import request from '../utils/request'

export function startInspectionSession(payload) {
  return request.post('/inspection/session/start', payload)
}

export function captureInspectionSession(sessionId) {
  return request.post(`/inspection/session/${sessionId}/capture`)
}

export function finishInspectionSession(sessionId, payload = {}) {
  return request.post(`/inspection/session/${sessionId}/finish`, payload)
}

export function abortInspectionSession(sessionId) {
  return request.post(`/inspection/session/${sessionId}/abort`)
}
