<template>
  <div class="metric-card">
    <div class="metric-icon" :class="{ 'has-image': iconSrc }">
      <img v-if="iconSrc" :src="iconSrc" :alt="iconAlt || label">
      <span v-else>{{ icon }}</span>
    </div>
    <div class="metric-content">
      <div class="metric-value">{{ value }}<span>{{ unit }}</span></div>
      <div class="metric-label">{{ label }}</div>
    </div>
  </div>
</template>

<script setup>
defineProps({
  icon: String,
  iconSrc: String,
  iconAlt: String,
  label: String,
  value: [String, Number],
  unit: String
})
</script>

<style scoped>
.metric-card {
  display: flex;
  align-items: center;
  width: 160px;
  min-height: 80px;
}

.metric-icon {
  display: grid;
  place-items: center;
  width: 80px;
  height: 80px;
  border-radius: 50%;
  border: 2px solid rgba(67, 214, 230, 0.36);
  color: var(--brand);
  font-size: 24px;
  box-shadow: inset 0 0 18px rgba(22, 122, 160, 0.2);
  overflow: hidden;
  flex: 0 0 80px;
}

.metric-icon.has-image {
  border: 0;
  box-shadow: none;
  background: transparent;
}

.metric-icon img {
  width: 80px;
  height: 80px;
  object-fit: cover;
}

.metric-content {
  margin-left: 7px;
}

.metric-value {
  font-size: 36px;
  font-weight: 700;
  line-height: 1;
  color: #8af6cb;
}

.metric-value span {
  margin-left: 4px;
  font-size: 16px;
}

.metric-label {
  margin-top: 8px;
  color: var(--text-muted);
  font-size: 16px;
  line-height: 1;
}
</style>
