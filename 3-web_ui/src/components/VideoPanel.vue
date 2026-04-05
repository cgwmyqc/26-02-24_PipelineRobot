<template>
  <section class="panel video-panel">
    <div class="panel-title">视频数据</div>
    <div class="panel-body video-body">
      <video
        ref="videoRef"
        class="video-player"
        :class="{ visible: status === 'playing' }"
        autoplay
        muted
        playsinline
        @playing="handlePlaying"
        @loadeddata="handleLoadedData"
        @error="handlePlaybackError"
      ></video>
      <div v-if="status !== 'playing'" class="video-surface">
        <div class="video-status-card">
          <div class="status-kicker">实时视频</div>
          <div class="status-title">{{ statusTitle }}</div>
          <div class="status-description">
            {{ statusDescription }}
          </div>
          <button class="play-button" type="button" aria-label="播放提示按钮">
            <span class="play-icon"></span>
          </button>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { appConfig } from '../config/app'

const SCRIPT_LOADERS_KEY = '__webrtcStreamerScriptLoaders__'
const videoRef = ref(null)
const status = ref('connecting')

let player = null
let connectTimeoutId = 0

function normalizeBaseUrl(url) {
  return String(url || '').trim().replace(/\/+$/, '')
}

function loadRemoteScript(src) {
  if (!window[SCRIPT_LOADERS_KEY]) {
    window[SCRIPT_LOADERS_KEY] = new Map()
  }

  const cached = window[SCRIPT_LOADERS_KEY].get(src)
  if (cached) {
    return cached
  }

  const loader = new Promise((resolve, reject) => {
    const existing = document.querySelector(`script[data-webrtc-src="${src}"]`)
    if (existing) {
      if (existing.dataset.loaded === 'true') {
        resolve()
        return
      }

      existing.addEventListener('load', () => resolve(), { once: true })
      existing.addEventListener('error', () => reject(new Error(`load failed: ${src}`)), { once: true })
      return
    }

    const script = document.createElement('script')
    script.src = src
    script.async = true
    script.dataset.webrtcSrc = src
    script.onload = () => {
      script.dataset.loaded = 'true'
      resolve()
    }
    script.onerror = () => reject(new Error(`load failed: ${src}`))
    document.head.appendChild(script)
  })

  window[SCRIPT_LOADERS_KEY].set(src, loader)
  return loader
}

async function ensureWebRtcStreamerScripts(baseUrl) {
  await loadRemoteScript(`${baseUrl}/libs/adapter.min.js`)
  await loadRemoteScript(`${baseUrl}/webrtcstreamer.js`)
}

function clearConnectTimeout() {
  if (connectTimeoutId) {
    window.clearTimeout(connectTimeoutId)
    connectTimeoutId = 0
  }
}

function disconnectPlayer() {
  clearConnectTimeout()

  if (player?.disconnect) {
    player.disconnect()
  }
  player = null

  if (videoRef.value?.srcObject) {
    videoRef.value.srcObject.getTracks().forEach((track) => track.stop())
    videoRef.value.srcObject = null
  }
}

function handleLoadedData() {
  if (status.value !== 'playing') {
    status.value = 'playing'
  }

  videoRef.value?.play?.().catch(() => {})
}

function handlePlaying() {
  clearConnectTimeout()
  status.value = 'playing'
}

function handlePlaybackError() {
  clearConnectTimeout()
  status.value = 'error'
}

async function connectStream() {
  const serverUrl = normalizeBaseUrl(appConfig.webrtcStreamerUrl)
  const streamName = String(appConfig.webrtcStreamName || '').trim()

  if (!serverUrl || !streamName) {
    status.value = 'idle'
    return
  }

  status.value = 'connecting'

  try {
    await ensureWebRtcStreamerScripts(serverUrl)

    if (typeof window.WebRtcStreamer !== 'function') {
      throw new Error('WebRtcStreamer is unavailable')
    }

    player = new window.WebRtcStreamer(videoRef.value, serverUrl)
    player.onError = () => {
      clearConnectTimeout()
      status.value = 'error'
    }
    player.connect(streamName)

    clearConnectTimeout()
    connectTimeoutId = window.setTimeout(() => {
      if (status.value !== 'playing') {
        status.value = 'error'
      }
    }, 12000)
  } catch (_error) {
    status.value = 'error'
  }
}

const statusTitle = computed(() => {
  if (status.value === 'idle') {
    return '未配置视频流'
  }
  if (status.value === 'error') {
    return '视频连接失败'
  }
  if (status.value === 'connecting') {
    return '正在建立视频连接'
  }
  return '实时视频已接入'
})

const statusDescription = computed(() => {
  if (status.value === 'idle') {
    return '请配置 WebRTC 服务地址和固定流名称后，再打开演示页面。'
  }
  if (status.value === 'error') {
    return '请检查 webrtc-streamer 服务、RTSP 地址和浏览器网络连通性。'
  }
  if (status.value === 'connecting') {
    return '页面正在通过 webrtc-streamer 连接巡检相机，请稍候。'
  }
  return '视频流已经建立，正在播放实时巡检画面。'
})

onMounted(() => {
  connectStream()
})

onBeforeUnmount(() => {
  disconnectPlayer()
})
</script>

<style scoped>
.video-panel {
  min-height: 420px;
  height: 100%;
  display: flex;
  flex-direction: column;
}

.video-body {
  flex: 1;
  min-height: 0;
  padding: 0;
  position: relative;
}

.video-player,
.video-surface {
  width: 100%;
  height: 100%;
  display: block;
  background:
    linear-gradient(180deg, rgba(31, 74, 110, 0.18), rgba(6, 15, 26, 0.92)),
    radial-gradient(circle at 50% 50%, rgba(120, 207, 255, 0.16), transparent 24%);
}

.video-player {
  object-fit: cover;
  opacity: 0;
  transition: opacity 220ms ease;
}

.video-player.visible {
  opacity: 1;
}

.video-surface {
  position: absolute;
  inset: 0;
  display: grid;
  place-items: center;
  padding: 28px;
}

.video-status-card {
  width: min(100%, 320px);
  padding: 28px 24px 24px;
  border: 1px solid rgba(117, 240, 194, 0.18);
  background: linear-gradient(180deg, rgba(13, 27, 43, 0.82), rgba(7, 17, 29, 0.9));
  box-shadow: 0 18px 40px rgba(0, 0, 0, 0.28);
  text-align: center;
}

.status-kicker {
  margin-bottom: 12px;
  color: rgba(117, 240, 194, 0.86);
  font-size: 13px;
  letter-spacing: 3px;
  text-transform: uppercase;
}

.status-title {
  margin-bottom: 10px;
  font-size: 24px;
  font-weight: 700;
  color: #ecf7ff;
}

.status-description {
  margin: 0 auto 22px;
  max-width: 240px;
  color: rgba(226, 241, 255, 0.76);
  font-size: 14px;
  line-height: 1.7;
}

.play-button {
  width: 74px;
  height: 74px;
  border: 1px solid rgba(117, 240, 194, 0.28);
  border-radius: 50%;
  background: radial-gradient(circle at 35% 35%, rgba(117, 240, 194, 0.34), rgba(24, 71, 59, 0.82));
  box-shadow: 0 0 0 8px rgba(117, 240, 194, 0.08);
  cursor: default;
}

.play-icon {
  display: inline-block;
  margin-left: 6px;
  width: 0;
  height: 0;
  border-top: 12px solid transparent;
  border-bottom: 12px solid transparent;
  border-left: 20px solid #f4fffb;
}
</style>
