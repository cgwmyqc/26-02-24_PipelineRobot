<template>
  <section class="panel video-panel">
    <div class="panel-title">视频数据</div>
    <div class="panel-body video-body">
      <img v-if="frameSrc" :src="frameSrc" alt="实时视频流" class="video-image" />
      <div v-else class="video-placeholder">
        <div class="play-button">▶</div>
        <span>等待 ROS 视频流接入</span>
      </div>
      <div class="video-status">{{ connected ? '实时流' : '离线演示' }}</div>
    </div>
  </section>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  frame: {
    type: String,
    default: ''
  },
  connected: {
    type: Boolean,
    default: false
  }
})

const frameSrc = computed(() => {
  if (!props.frame) {
    return ''
  }
  if (props.frame.startsWith('data:image')) {
    return props.frame
  }
  return `data:image/jpeg;base64,${props.frame}`
})
</script>

<style scoped>
.video-panel {
  min-height: 400px;
}

.video-body {
  position: relative;
  padding: 12px;
}

.video-image,
.video-placeholder {
  width: 100%;
  min-height: 340px;
  border: 1px solid rgba(103, 212, 255, 0.12);
}

.video-image {
  object-fit: cover;
  background: #091522;
}

.video-placeholder {
  display: grid;
  place-items: center;
  gap: 16px;
  background:
    linear-gradient(180deg, rgba(31, 74, 110, 0.2), rgba(6, 15, 26, 0.85)),
    radial-gradient(circle at 50% 50%, rgba(120, 207, 255, 0.18), transparent 24%);
  color: var(--text-dim);
}

.play-button {
  display: grid;
  place-items: center;
  width: 88px;
  height: 88px;
  border-radius: 50%;
  background: radial-gradient(circle, rgba(117, 240, 194, 0.9), rgba(82, 210, 193, 0.7));
  color: #082118;
  font-size: 34px;
  box-shadow: 0 0 30px rgba(117, 240, 194, 0.3);
}

.video-status {
  position: absolute;
  right: 22px;
  bottom: 20px;
  padding: 4px 10px;
  border-radius: 999px;
  background: rgba(8, 18, 30, 0.75);
  color: #8ef3cf;
  font-size: 13px;
}
</style>
