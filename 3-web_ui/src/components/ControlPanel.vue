<template>
  <section class="control-layout">
    <div class="mode-switch">
      <button :class="['mode-btn', { active: patrolMode === 'auto' }]" @click="setPatrolMode('auto')">自动巡检</button>
      <button :class="['mode-btn', { active: patrolMode === 'manual' }]" @click="setPatrolMode('manual')">人工巡检</button>
    </div>

    <div class="main-stage">
      <div class="motion-actions">
        <button
          class="action-btn"
          :disabled="patrolMode !== 'manual'"
          @mousedown="toggleMove('forward', true)"
          @mouseup="toggleMove('forward', false)"
          @mouseleave="toggleMove('forward', false)"
          @touchstart.prevent="toggleMove('forward', true)"
          @touchend.prevent="toggleMove('forward', false)"
        >
          <span>▲</span>
          <strong>前进</strong>
        </button>

        <button
          class="action-btn secondary"
          :disabled="patrolMode !== 'manual'"
          @mousedown="toggleMove('reverse', true)"
          @mouseup="toggleMove('reverse', false)"
          @mouseleave="toggleMove('reverse', false)"
          @touchstart.prevent="toggleMove('reverse', true)"
          @touchend.prevent="toggleMove('reverse', false)"
        >
          <span>▼</span>
          <strong>后退</strong>
        </button>

        <button class="action-btn launch-btn" @click="startAutoInspection">
          <span>◎</span>
          <strong>开始自动</strong>
        </button>
      </div>

      <div class="pipe-scene">
        <PipeThreeScene />
      </div>

      <div class="metrics">
        <MetricCard icon="℃" label="设备温度" :value="formatValue(temperature, 1)" unit="°C" />
        <MetricCard icon="m" label="当前位置" :value="formatValue(travelMeters, 2)" unit="m" />
        <MetricCard icon="--" label="淤泥厚度" value="--" unit="待接入" />
      </div>
    </div>

    <div class="state-bar">
      <div class="pill">
        <span>电机</span>
        <strong>{{ motorEnabled ? '运行中' : '已停止' }}</strong>
      </div>
      <div class="pill">
        <span>方向</span>
        <strong>{{ directionLabel }}</strong>
      </div>
      <div class="pill">
        <span>编码器</span>
        <strong>{{ encoderCount }}</strong>
      </div>
      <div class="pill">
        <span>进水状态</span>
        <strong>{{ waterDetected ? '已入水' : '未入水' }}</strong>
      </div>
      <div class="pill">
        <span>检测状态</span>
        <strong>{{ motionReached ? '已完成' : '待完成' }}</strong>
      </div>
    </div>
  </section>
</template>

<script setup>
import { computed } from 'vue'
import MetricCard from './MetricCard.vue'
import PipeThreeScene from './PipeThreeScene.vue'
import { formatValue } from '../utils/format'

const props = defineProps({
  patrolMode: String,
  temperature: Number,
  travelMeters: Number,
  motorEnabled: Boolean,
  motorRunState: Number,
  encoderCount: Number,
  waterDetected: Boolean,
  motionReached: Boolean,
  setPatrolMode: Function,
  publishMoveCommand: Function,
  startAutoInspection: Function,
  markDetectDone: Function
})

const directionLabel = computed(() => {
  if (props.motorRunState === 1) {
    return '前进'
  }
  if (props.motorRunState === -1) {
    return '后退'
  }
  return '停止'
})

function toggleMove(direction, active) {
  props.publishMoveCommand?.(direction, active)
}
</script>

<style scoped>
.control-layout {
  display: grid;
  gap: 16px;
  min-height: 100%;
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
  min-width: 112px;
  height: 36px;
  border: 0;
  border-radius: 999px;
  background: transparent;
  color: var(--text-dim);
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
  grid-template-columns: 92px minmax(0, 1fr) 200px;
  gap: 18px;
  align-items: stretch;
  min-height: 420px;
}

.motion-actions {
  display: grid;
  align-content: center;
  gap: 16px;
}

.action-btn {
  display: grid;
  place-items: center;
  gap: 4px;
  width: 78px;
  height: 78px;
  border-radius: 50%;
  border: 1px solid rgba(106, 236, 194, 0.28);
  background: radial-gradient(circle at 30% 30%, rgba(135, 244, 190, 0.34), rgba(9, 25, 38, 0.95));
  color: var(--brand);
  cursor: pointer;
}

.action-btn:disabled {
  opacity: 0.35;
  cursor: not-allowed;
}

.action-btn.secondary {
  color: #9ab7db;
}

.launch-btn {
  font-size: 12px;
  line-height: 1.15;
}

.action-btn span {
  font-size: 24px;
}

.action-btn strong {
  font-size: 16px;
  text-align: center;
}

.pipe-scene {
  min-height: 360px;
  background: linear-gradient(180deg, rgba(6, 19, 26, 0.12), rgba(41, 139, 103, 0.06));
}

.metrics {
  display: grid;
  gap: 22px;
  align-content: center;
}

.state-bar {
  display: grid;
  grid-template-columns: repeat(5, minmax(0, 1fr));
  gap: 12px;
}

.pill {
  min-height: 84px;
  display: grid;
  align-content: center;
  gap: 6px;
  padding: 12px 14px;
  background: rgba(9, 22, 36, 0.7);
  border: 1px solid rgba(103, 212, 255, 0.08);
}

.pill span {
  color: var(--text-dim);
  font-size: 13px;
}

.pill strong {
  font-size: 18px;
}

@media (max-width: 1440px) {
  .main-stage {
    grid-template-columns: 92px minmax(0, 1fr);
  }

  .metrics {
    grid-column: 1 / -1;
    grid-template-columns: repeat(3, minmax(0, 1fr));
  }

  .state-bar {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (max-width: 1024px) {
  .main-stage,
  .state-bar,
  .metrics {
    grid-template-columns: 1fr;
  }

  .motion-actions {
    grid-auto-flow: column;
    justify-content: start;
  }
}
</style>
