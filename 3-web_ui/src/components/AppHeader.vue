<template>
  <header class="app-header">
    <div class="header-image" :style="headerStyle"></div>
    <div class="header-actions">
      <div class="connection-chip">
        <div class="status-dot" :class="{ online: connected }"></div>
        <span>{{ connected ? 'ROS2 已连接' : 'ROS2 未连接' }}</span>
      </div>
      <el-button class="ota-trigger-button" type="primary" @click="$emit('open-ota')">
        固件刷写
      </el-button>
      <div class="user-chip">
        <div class="avatar">{{ userInitial }}</div>
        <span>{{ userName }}</span>
      </div>
      <el-button text type="success" @click="$emit('logout')">退出</el-button>
    </div>
  </header>
</template>

<script setup>
import { computed } from 'vue'
import { resolveImageAsset } from '../utils/assets'

const props = defineProps({
  connected: {
    type: Boolean,
    default: false
  },
  userName: {
    type: String,
    default: 'admin'
  }
})

defineEmits(['logout', 'open-ota'])

const userInitial = computed(() => props.userName?.slice(0, 1)?.toUpperCase() || 'U')
const headerImage = resolveImageAsset('header')
const headerStyle = computed(() => (
  headerImage
    ? { backgroundImage: `url(${headerImage})` }
    : {}
))
</script>

<style scoped>
.app-header {
  position: relative;
  height: clamp(96px, 10vw, 122px);
  min-height: 96px;
  padding-top: 10px;
  padding-bottom: 10px;
  overflow: hidden;
}

.header-image {
  position: absolute;
  inset: 10px 0 10px;
  background:
    linear-gradient(90deg, rgba(4, 13, 24, 0.22), rgba(7, 18, 31, 0.08)),
    linear-gradient(180deg, rgba(7, 16, 30, 0.96), rgba(11, 24, 40, 0.78));
  background-repeat: no-repeat;
  background-size: cover;
  background-position: center;
}

.header-actions {
  position: relative;
  z-index: 1;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: 10px;
  padding: 0 18px 0 18px;
  min-height: 0;
}

.connection-chip,
.user-chip {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  min-height: 32px;
  padding: 0 10px;
  border: 1px solid rgba(103, 212, 255, 0.12);
  background: rgba(9, 18, 30, 0.56);
  backdrop-filter: blur(6px);
  font-size: 13px;
}

.status-dot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: #8a9098;
  box-shadow: 0 0 10px rgba(138, 144, 152, 0.4);
}

.status-dot.online {
  background: var(--brand);
  box-shadow: 0 0 12px rgba(117, 240, 194, 0.8);
}

.avatar {
  display: grid;
  place-items: center;
  width: 24px;
  height: 24px;
  border-radius: 50%;
  background: linear-gradient(135deg, #ffecc5, #bfbfbf);
  color: #0b1220;
  font-size: 13px;
  font-weight: 700;
}

.ota-trigger-button {
  --el-button-bg-color: rgba(17, 59, 78, 0.92);
  --el-button-border-color: rgba(103, 212, 255, 0.28);
  --el-button-hover-bg-color: rgba(28, 84, 111, 0.96);
  --el-button-hover-border-color: rgba(117, 240, 194, 0.42);
  --el-button-text-color: var(--text);
  min-height: 34px;
  padding: 0 14px;
  box-shadow: inset 0 0 0 1px rgba(117, 240, 194, 0.05);
}

@media (max-width: 1024px) {
  .app-header {
    height: auto;
    min-height: 108px;
  }

  .header-actions {
    flex-wrap: wrap;
    justify-content: flex-start;
    align-content: center;
    padding: 8px 14px 0;
  }
}
</style>
