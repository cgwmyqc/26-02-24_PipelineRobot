<template>
  <header class="app-header">
    <div class="brand">
      <div class="brand-mark">UNI</div>
      <div class="brand-cut"></div>
    </div>
    <div class="title-wrap">
      <h1>低干扰排水管道缺陷与状态自动检测装置智能监测平台</h1>
    </div>
    <div class="header-actions">
      <div class="status-dot" :class="{ online: connected }"></div>
      <span>{{ connected ? 'ROS2 已连接' : 'ROS2 未连接' }}</span>
      <div class="avatar">U</div>
      <span>user_name_</span>
    </div>
  </header>
</template>

<script setup>
defineProps({
  connected: {
    type: Boolean,
    default: false
  }
})
</script>

<style scoped>
.app-header {
  display: grid;
  grid-template-columns: 110px 1fr auto;
  align-items: center;
  min-height: 72px;
  margin-bottom: 10px;
  border-bottom: 1px solid rgba(103, 212, 255, 0.22);
  background: linear-gradient(180deg, rgba(7, 16, 30, 0.96), rgba(11, 24, 40, 0.78));
}
.brand {
  position: relative;
  height: 72px;
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
.title-wrap h1 {
  margin: 0;
  padding-left: 26px;
  font-size: 24px;
  letter-spacing: 1px;
}
.header-actions {
  display: flex;
  align-items: center;
  gap: 12px;
  padding-right: 22px;
  color: var(--text-muted);
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
</style>
