<template>
  <section class="panel video-panel">
    <div class="panel-title">视频数据</div>
    <div class="panel-body video-body">
      <img v-if="showLiveFrame" :src="frame" alt="巡检视频画面" class="video-image" />
      <div v-else class="video-surface">
        <div class="video-status-card">
          <div class="status-kicker">实时视频</div>
          <div class="status-title">{{ connected ? '等待视频流接入' : 'ROS2 服务未连接' }}</div>
          <div class="status-description">
            {{ connected ? '检测到 /ipcamera/image_raw 后会自动切换到实时画面。' : '请先确认 rosbridge 和相机节点已正常启动。' }}
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
import { computed } from 'vue'

const props = defineProps({
  frame: {
    type: String,
    default: ''
  },
  active: {
    type: Boolean,
    default: false
  },
  connected: {
    type: Boolean,
    default: false
  }
})

const showLiveFrame = computed(() => props.active && Boolean(props.frame))
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

.video-surface {
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
