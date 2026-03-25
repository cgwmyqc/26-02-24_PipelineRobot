import { defineStore } from 'pinia'
import { appConfig } from '../config/app'
import { login } from '../api/auth'

function loadPersistedAuth() {
  const raw = localStorage.getItem(appConfig.authStorageKey)
  if (!raw) {
    return { token: '', user: null }
  }

  try {
    const parsed = JSON.parse(raw)
    return {
      token: parsed?.token || '',
      user: parsed?.user || null
    }
  } catch (_error) {
    localStorage.removeItem(appConfig.authStorageKey)
    return { token: '', user: null }
  }
}

export const useAuthStore = defineStore('auth', {
  state: () => ({
    ...loadPersistedAuth(),
    loading: false
  }),
  getters: {
    isAuthenticated: (state) => Boolean(state.token)
  },
  actions: {
    persistAuth() {
      localStorage.setItem(
        appConfig.authStorageKey,
        JSON.stringify({
          token: this.token,
          user: this.user
        })
      )
    },
    setAuth(payload) {
      this.token = payload?.token || ''
      this.user = payload?.user || null
      if (this.token) {
        this.persistAuth()
      } else {
        localStorage.removeItem(appConfig.authStorageKey)
      }
    },
    async login(credentials) {
      this.loading = true
      try {
        const { data } = await login(credentials)
        this.setAuth(data)
        return data
      } finally {
        this.loading = false
      }
    },
    logout() {
      this.setAuth({ token: '', user: null })
    }
  }
})
