<template>
  <section class="panel analysis-panel">
    <div class="panel-title">监测分析</div>
    <div class="panel-body analysis-body">
      <div class="analysis-visual">
        <img
          :src="statusImage"
          :alt="statusAlt"
          :class="['analysis-image', imageEffectClass]"
        >
      </div>

      <div
        ref="messageViewportRef"
        :class="['analysis-message-viewport', { scrolling: shouldScroll }]"
      >
        <div
          ref="messageTrackRef"
          class="analysis-message-track"
          :style="trackStyle"
        >
          <p
            v-for="(message, index) in renderedMessages"
            :key="`${message}-${index}`"
            class="analysis-message"
          >
            {{ message }}
          </p>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { storeToRefs } from 'pinia'
import { resolveImageAsset } from '../utils/assets'
import { ANALYSIS_STATUS, useDashboardStore } from '../stores/dashboard'

const store = useDashboardStore()
const { analysisStatus, analysisImageKey, analysisMessages } = storeToRefs(store)

const messageViewportRef = ref(null)
const messageTrackRef = ref(null)
const shouldScroll = ref(false)

let resizeObserver = null

const statusImage = computed(() => resolveImageAsset(analysisImageKey.value))
const imageEffectClass = computed(() => {
  if (analysisStatus.value === ANALYSIS_STATUS.CHECKING) {
    return 'is-checking'
  }
  if (analysisStatus.value === ANALYSIS_STATUS.WARNING) {
    return 'is-warning'
  }
  return 'is-ok'
})

const statusAlt = computed(() => {
  if (analysisStatus.value === ANALYSIS_STATUS.CHECKING) {
    return '管道状态检测中'
  }
  if (analysisStatus.value === ANALYSIS_STATUS.WARNING) {
    return '管道状态异常预警'
  }
  return '管道状态正常'
})

const renderedMessages = computed(() => (
  shouldScroll.value
    ? analysisMessages.value.concat(analysisMessages.value)
    : analysisMessages.value
))

const trackStyle = computed(() => ({
  '--analysis-scroll-duration': `${Math.max(8, analysisMessages.value.length * 3)}s`
}))

async function updateScrollState() {
  await nextTick()

  const viewport = messageViewportRef.value
  const track = messageTrackRef.value
  if (!viewport || !track) {
    shouldScroll.value = false
    return
  }

  if (analysisStatus.value !== ANALYSIS_STATUS.WARNING || analysisMessages.value.length <= 1) {
    shouldScroll.value = false
    return
  }

  shouldScroll.value = track.scrollHeight > viewport.clientHeight + 1
}

watch(
  () => [analysisStatus.value, analysisMessages.value.join('|')],
  () => {
    updateScrollState()
  },
  { immediate: true }
)

onMounted(() => {
  if (typeof ResizeObserver !== 'undefined') {
    resizeObserver = new ResizeObserver(() => {
      updateScrollState()
    })

    if (messageViewportRef.value) {
      resizeObserver.observe(messageViewportRef.value)
    }
  }
})

onBeforeUnmount(() => {
  resizeObserver?.disconnect()
})
</script>

<style scoped>
.analysis-panel {
  min-height: 420px;
  height: 100%;
}

.analysis-body {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 18px;
  height: 100%;
  padding: 12px 18px 24px;
}

.analysis-visual {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
}

.analysis-image {
  display: block;
  max-width: min(100%, 390px);
  max-height: 260px;
  object-fit: contain;
  transform-origin: center;
  will-change: transform;
}

.analysis-image.is-ok {
  animation: none;
}

.analysis-image.is-checking {
  animation: analysis-breathe-slow 3.0s cubic-bezier(0.4, 0, 0.2, 1) infinite;
}

.analysis-image.is-warning {
  animation: analysis-breathe-fast 1.2s cubic-bezier(0.4, 0, 0.2, 1) infinite;
}

.analysis-message-viewport {
  position: relative;
  width: min(100%, 320px);
  min-height: 34px;
  max-height: 104px;
  overflow: hidden;
  mask-image: linear-gradient(to bottom, transparent 0, #000 12px, #000 calc(100% - 12px), transparent 100%);
}

.analysis-message-track {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.analysis-message-viewport.scrolling .analysis-message-track {
  animation: scroll-messages var(--analysis-scroll-duration, 10s) linear infinite;
}

.analysis-message {
  margin: 0;
  color: #dbe6f2;
  font-size: 18px;
  line-height: 1.6;
  text-align: center;
}

@keyframes scroll-messages {
  from {
    transform: translateY(0);
  }

  to {
    transform: translateY(calc(-50% - 5px));
  }
}

@keyframes analysis-breathe-slow {
  0%,
  100% {
    transform: scale(1);
  }

  50% {
    transform: scale(1.08);
  }
}

@keyframes analysis-breathe-fast {
  0%,
  100% {
    transform: scale(1);
  }

  50% {
    transform: scale(1.1);
  }
}

@media (max-width: 768px) {
  .analysis-body {
    padding: 20px 14px;
  }

  .analysis-image {
    max-height: 180px;
  }

  .analysis-message {
    font-size: 16px;
  }
}
</style>
