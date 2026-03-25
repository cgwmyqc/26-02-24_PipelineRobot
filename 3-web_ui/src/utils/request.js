import axios from 'axios'
import { appConfig } from '../config/app'

const request = axios.create({
  baseURL: appConfig.backendBaseURL,
  timeout: 15000
})

request.interceptors.request.use((config) => {
  const raw = localStorage.getItem(appConfig.authStorageKey)
  if (raw) {
    try {
      const auth = JSON.parse(raw)
      if (auth?.token) {
        config.headers.Authorization = `Bearer ${auth.token}`
      }
    } catch (_error) {
      localStorage.removeItem(appConfig.authStorageKey)
    }
  }
  return config
})

request.interceptors.response.use(
  (response) => response,
  (error) => {
    if (error?.response?.status === 401) {
      localStorage.removeItem(appConfig.authStorageKey)
      if (window.location.pathname !== '/login') {
        window.location.href = '/login'
      }
    }
    return Promise.reject(error)
  }
)

export default request
