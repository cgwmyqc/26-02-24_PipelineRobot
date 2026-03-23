<template>
  <div class="metric-card">
    <div class="metric-icon">{{ icon }}</div>
    <div>
      <div class="metric-value">{{ value }}<span>{{ unit }}</span></div>
      <div class="metric-label">{{ label }}</div>
    </div>
  </div>
</template>

<script setup>
defineProps({
  icon: String,
  label: String,
  value: [String, Number],
  unit: String
})
</script>

<style scoped>
.metric-card {
  display: flex;
  align-items: center;
  gap: 14px;
  min-width: 145px;
}
.metric-icon {
  display: grid;
  place-items: center;
  width: 54px;
  height: 54px;
  border-radius: 50%;
  border: 2px solid rgba(67, 214, 230, 0.36);
  color: var(--brand);
  font-size: 26px;
  box-shadow: inset 0 0 18px rgba(22, 122, 160, 0.2);
}
.metric-value {
  font-size: 28px;
  font-weight: 700;
  color: #8af6cb;
}
.metric-value span {
  margin-left: 4px;
  font-size: 20px;
}
.metric-label {
  color: var(--text-muted);
  font-size: 15px;
}
</style>
