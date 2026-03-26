<template>
  <section class="panel video-panel">
    <div class="panel-title">视频数据</div>
    <div class="panel-body video-body">
      <img v-if="frameSrc" :src="frameSrc" alt="巡检视频画面" class="video-image" />
      <div v-else class="video-surface"></div>
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
  min-height: 420px;
  height: 100%;
  display: flex;
  flex-direction: column;
}

.video-body {
  flex: 1;
  min-height: 0;
  padding: 0;
}

.video-image,
.video-surface {
  width: 100%;
  height: 100%;
  display: block;
  background:
    linear-gradient(180deg, rgba(31, 74, 110, 0.18), rgba(6, 15, 26, 0.92)),
    radial-gradient(circle at 50% 50%, rgba(120, 207, 255, 0.16), transparent 24%);
}

.video-image {
  object-fit: cover;
}
</style>
