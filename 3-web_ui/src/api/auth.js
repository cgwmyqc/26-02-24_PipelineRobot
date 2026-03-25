import request from '../utils/request'

export function login(payload) {
  return request.post('/auth/login', payload)
}
