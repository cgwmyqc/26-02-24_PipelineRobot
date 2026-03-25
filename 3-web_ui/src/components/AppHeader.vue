<template>
  <header class="app-header">
    <div class="brand">
      <div class="brand-mark">UNI</div>
      <div class="brand-cut"></div>
    </div>

    <div class="title-wrap">
      <p class="eyebrow">Digital Twin Platform</p>
      <h1>下水道巡检机器人智能监测平台</h1>
    </div>

    <div class="header-actions">
      <div class="connection-chip">
        <div class="status-dot" :class="{ online: connected }"></div>
        <span>{{ connected ? 'ROS2 已连接' : 'ROS2 未连接' }}</span>
      </div>
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

defineEmits(['logout'])

const userInitial = computed(() => props.userName?.slice(0, 1)?.toUpperCase() || 'U')
</script>

<style scoped>
.app-header {
  display: grid;
  grid-template-columns: 110px minmax(0, 1fr) auto;
  align-items: center;
  gap: 18px;
  min-height: 82px;
  padding-right: 18px;
  border-bottom: 1px solid rgba(103, 212, 255, 0.16);
  background: linear-gradient(180deg, rgba(7, 16, 30, 0.96), rgba(11, 24, 40, 0.78));
}

.brand {
  position: relative;
  height: 82px;
  background: linear-gradient(135deg, rgba(33, 126, 168, 0.35), rgba(18, 49, 77, 0.72));
  border-right: 1px solid rgba(103, 212, 255, 0.22);
}

.brand-mark {
  display: grid;
  place-items: center;
  height: 100%;
  font-size: 30px;
  font-weight: 800;
  color: #59e9f5;
  letter-spacing: 2px;
}

.brand-cut {
  position: absolute;
  right: -10px;
  bottom: -1px;
  width: 20px;
  height: 12px;
  background: linear-gradient(135deg, transparent 50%, rgba(33, 126, 168, 0.58) 50%);
}

.eyebrow {
  margin: 0 0 6px;
  color: var(--text-dim);
  font-size: 12px;
  letter-spacing: 2px;
  text-transform: uppercase;
}

.title-wrap h1 {
  margin: 0;
  font-size: 24px;
  letter-spacing: 1px;
}

.header-actions {
  display: flex;
  align-items: center;
  gap: 12px;
  color: var(--text-dim);
}

.connection-chip,
.user-chip {
  display: inline-flex;
  align-items: center;
  gap: 10px;
  padding: 8px 12px;
  border: 1px solid rgba(103, 212, 255, 0.12);
  background: rgba(9, 18, 30, 0.52);
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
  width: 30px;
  height: 30px;
  border-radius: 50%;
  background: linear-gradient(135deg, #ffecc5, #bfbfbf);
  color: #0b1220;
  font-weight: 700;
}

@media (max-width: 1024px) {
  .app-header {
    grid-template-columns: 88px 1fr;
    padding-right: 12px;
  }

  .header-actions {
    grid-column: 1 / -1;
    padding: 0 12px 12px;
    flex-wrap: wrap;
  }
}
</style>
