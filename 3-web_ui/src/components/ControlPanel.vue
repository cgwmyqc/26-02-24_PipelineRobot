<template>
  <section class="control-layout">
    <div class="mode-switch">
      <button :class="['mode-btn', { active: patrolMode === 'auto' }]" @click="publishPatrolMode('auto')">自动巡检</button>
      <button :class="['mode-btn', { active: patrolMode === 'manual' }]" @click="publishPatrolMode('manual')">人工巡检</button>
    </div>

    <div class="main-stage">
      <div class="motion-actions">
        <button class="action-btn" @mousedown="publishMoveCommand('FORWARD')" @click="publishMoveCommand('FORWARD')">
          <span>▶</span>
          <strong>前进</strong>
        </button>
        <button class="action-btn secondary" @mousedown="publishMoveCommand('BACKWARD')" @click="publishMoveCommand('BACKWARD')">
          <span>◀</span>
          <strong>后退</strong>
        </button>
      </div>

      <div class="pipe-scene">
        <PipeThreeScene />
      </div>

      <div class="metrics">
        <MetricCard icon="℃" label="设备温度" :value="formatValue(temperature, 0)" unit="°C" />
        <MetricCard icon="▤" label="淤泥厚度" :value="formatValue(sludgeThickness, 0)" unit="cm" />
      </div>
    </div>
  </section>
</template>

<script setup>
import MetricCard from './MetricCard.vue'
import PipeThreeScene from './PipeThreeScene.vue'
import { formatValue } from '../utils/format'

defineProps({
  patrolMode: String,
  temperature: Number,
  sludgeThickness: Number,
  publishPatrolMode: Function,
  publishMoveCommand: Function
})
</script>

<style scoped>
.control-layout {
  display: flex;
  flex-direction: column;
  gap: 16px;
  min-height: 470px;
}
.mode-switch {
  display: inline-flex;
  width: fit-content;
  padding: 4px;
  border: 1px solid rgba(103, 212, 255, 0.22);
  border-radius: 999px;
  background: rgba(11, 23, 38, 0.7);
}
.mode-btn {
  min-width: 104px;
  height: 34px;
  border: 0;
  border-radius: 999px;
  background: transparent;
  color: var(--text-muted);
  cursor: pointer;
  transition: all 0.2s ease;
}
.mode-btn.active {
  background: linear-gradient(135deg, #88f1ba, #60f2df);
  color: #06211d;
  font-weight: 700;
}
.main-stage {
  display: grid;
  grid-template-columns: 84px 1fr 170px;
  align-items: center;
  gap: 18px;
  min-height: 410px;
}
.motion-actions {
  display: grid;
  gap: 16px;
}
.action-btn {
  display: grid;
  place-items: center;
  gap: 4px;
  width: 70px;
  height: 70px;
  border-radius: 50%;
  border: 1px solid rgba(106, 236, 194, 0.28);
  background: radial-gradient(circle at 30% 30%, rgba(135, 244, 190, 0.34), rgba(9, 25, 38, 0.95));
  color: var(--brand);
  cursor: pointer;
}
.action-btn.secondary {
  color: #9ab7db;
}
.action-btn span {
  font-size: 24px;
}
.action-btn strong {
  font-size: 18px;
}
.pipe-scene {
  height: 360px;
  background: linear-gradient(180deg, rgba(6, 19, 26, 0.12), rgba(41, 139, 103, 0.06));
}
.metrics {
  display: grid;
  gap: 26px;
}
</style>
