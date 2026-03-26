<template>
  <section class="control-layout">
    <div class="control-stage">
      <div class="scene-layer">
        <PipeThreeScene />
      </div>

      <div class="overlay-layer">
        <div class="mode-switch hud-panel">
          <button
            :class="['mode-btn', { active: patrolMode === 'auto' }]"
            @click="setPatrolMode('auto')"
          >
            自动巡检
          </button>
          <button
            :class="['mode-btn', { active: patrolMode === 'manual' }]"
            @click="setPatrolMode('manual')"
          >
            人工巡检
          </button>
        </div>

        <div class="hud-grid">
          <div class="motion-actions hud-panel hud-block">
            <button
              class="image-action-btn"
              :class="{ inactive: !isManualMode }"
              :disabled="!isManualMode"
              @mousedown="toggleMove('forward', true)"
              @mouseup="toggleMove('forward', false)"
              @mouseleave="toggleMove('forward', false)"
              @blur="toggleMove('forward', false)"
              @touchstart.prevent="toggleMove('forward', true)"
              @touchend.prevent="toggleMove('forward', false)"
              @touchcancel.prevent="toggleMove('forward', false)"
            >
              <img v-if="forwardButtonImage" :src="forwardButtonImage" alt="前进">
              <span v-else>前进</span>
            </button>

            <button
              class="image-action-btn"
              :class="{ inactive: !isManualMode }"
              :disabled="!isManualMode"
              @mousedown="toggleMove('reverse', true)"
              @mouseup="toggleMove('reverse', false)"
              @mouseleave="toggleMove('reverse', false)"
              @blur="toggleMove('reverse', false)"
              @touchstart.prevent="toggleMove('reverse', true)"
              @touchend.prevent="toggleMove('reverse', false)"
              @touchcancel.prevent="toggleMove('reverse', false)"
            >
              <img v-if="reverseButtonImage" :src="reverseButtonImage" alt="后退">
              <span v-else>后退</span>
            </button>

            <button
              class="image-action-btn"
              :class="{ inactive: !isAutoMode }"
              :disabled="!isAutoMode"
              @click="startAutoInspection"
            >
              <img v-if="startButtonImage" :src="startButtonImage" alt="开始自动巡检">
              <span v-else>自动</span>
            </button>
          </div>

          <div class="metrics hud-panel hud-block">
            <MetricCard
              :icon-src="temperatureImage"
              icon-alt="温度"
              label="温度"
              :value="formatValue(temperature, 1)"
              unit="°C"
            />
            <MetricCard
              :icon-src="humidityImage"
              icon-alt="湿度"
              label="湿度"
              :value="formatValue(humidity, 1)"
              unit="%"
            />
            <MetricCard
              :icon-src="mudheightImage"
              icon-alt="淤泥厚度"
              label="淤泥厚度"
              value="--"
              unit=""
            />
          </div>
        </div>

        <div class="state-bar hud-panel">
          <div class="pill">
            <span>电机状态</span>
            <strong>{{ motorEnabled ? '已启动' : '未启动' }}</strong>
          </div>
          <div class="pill">
            <span>运行方向</span>
            <strong>{{ directionLabel }}</strong>
          </div>
          <div class="pill">
            <span>当前位置</span>
            <strong>{{ positionText }}</strong>
          </div>
          <div class="pill">
            <span>湿度</span>
            <strong>{{ humidityText }}</strong>
          </div>
          <div class="pill">
            <span>到位状态</span>
            <strong>{{ motionReached ? '已到位' : '未到位' }}</strong>
          </div>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup>
import { computed } from 'vue'
import MetricCard from './MetricCard.vue'
import PipeThreeScene from './PipeThreeScene.vue'
import { resolveImageAsset } from '../utils/assets'
import { formatValue } from '../utils/format'

const props = defineProps({
  patrolMode: String,
  temperature: Number,
  humidity: Number,
  travelMeters: Number,
  motorEnabled: Boolean,
  motorRunState: Number,
  motionReached: Boolean,
  setPatrolMode: Function,
  publishMoveCommand: Function,
  startAutoInspection: Function
})

const forwardActiveImage = resolveImageAsset('btn_fw_active')
const forwardInactiveImage = resolveImageAsset('btn_fw_disactive')
const reverseActiveImage = resolveImageAsset('btn_bw_active')
const reverseInactiveImage = resolveImageAsset('btn_bw_disactive')
const startActiveImage = resolveImageAsset('btn_start_active')
const startInactiveImage = resolveImageAsset('btn_start_disacitve')
const temperatureImage = resolveImageAsset('temperature')
const humidityImage = resolveImageAsset('hum')
const mudheightImage = resolveImageAsset('mudheight')

const isManualMode = computed(() => props.patrolMode === 'manual')
const isAutoMode = computed(() => props.patrolMode === 'auto')

const directionLabel = computed(() => {
  if (props.motorRunState === 1) {
    return '前进'
  }
  if (props.motorRunState === -1) {
    return '后退'
  }
  return '停止'
})

const positionText = computed(() => `${formatValue(props.travelMeters, 2)} m`)
const humidityText = computed(() => `${formatValue(props.humidity, 1)} %`)
const forwardButtonImage = computed(() => (isManualMode.value ? forwardActiveImage : forwardInactiveImage))
const reverseButtonImage = computed(() => (isManualMode.value ? reverseActiveImage : reverseInactiveImage))
const startButtonImage = computed(() => (isAutoMode.value ? startActiveImage : startInactiveImage))

function toggleMove(direction, active) {
  if (!isManualMode.value) {
    return
  }
  props.publishMoveCommand?.(direction, active)
}
</script>

<style scoped>
.control-layout {
  min-height: 100%;
}

.control-stage {
  position: relative;
  min-height: 430px;
  border-radius: 18px;
  overflow: hidden;
  background: linear-gradient(180deg, rgba(5, 15, 26, 0.55), rgba(5, 12, 23, 0.84));
}

.scene-layer {
  position: absolute;
  inset: 0;
}

.overlay-layer {
  position: relative;
  z-index: 1;
  display: grid;
  grid-template-rows: auto 1fr auto;
  gap: 16px;
  min-height: 430px;
  padding: 16px;
  pointer-events: none;
}

.hud-grid {
  display: grid;
  grid-template-columns: 78px minmax(0, 1fr);
  align-items: center;
  gap: 16px;
}

.hud-panel {
  background: rgba(8, 19, 31, 0.48);
  backdrop-filter: blur(8px);
  border: 1px solid rgba(103, 212, 255, 0.12);
  box-shadow: 0 10px 24px rgba(3, 8, 15, 0.24);
}

.hud-block,
.mode-switch,
.state-bar,
.image-action-btn {
  pointer-events: auto;
}

.mode-switch {
  display: inline-flex;
  width: fit-content;
  border-radius: 999px;
  overflow: hidden;
}

.mode-btn {
  width: 111px;
  height: 40px;
  border: 0;
  background: transparent;
  color: var(--text-dim);
  cursor: pointer;
  font-size: 18px;
  line-height: 1;
  transition: all 0.2s ease;
}

.mode-btn.active {
  background: linear-gradient(135deg, #88f1ba, #60f2df);
  color: #06211d;
  font-weight: 700;
}

.motion-actions {
  display: grid;
  align-content: center;
  justify-items: center;
  gap: 16px;
  min-height: 240px;
  padding: 16px 10px;
  border-radius: 18px;
}

.image-action-btn {
  display: grid;
  place-items: center;
  width: 53px;
  height: 53px;
  padding: 0;
  border: 0;
  background: transparent;
  cursor: pointer;
}

.image-action-btn img {
  width: 53px;
  height: 53px;
  display: block;
  object-fit: contain;
}

.image-action-btn span {
  display: grid;
  place-items: center;
  width: 53px;
  height: 53px;
  border-radius: 14px;
  background: rgba(13, 27, 41, 0.92);
  border: 1px solid rgba(103, 212, 255, 0.24);
  color: var(--brand);
  font-size: 12px;
}

.image-action-btn.inactive,
.image-action-btn:disabled {
  cursor: not-allowed;
}

.metrics {
  display: grid;
  justify-content: end;
  align-content: center;
  gap: 16px;
  min-height: 240px;
  padding: 16px;
  border-radius: 18px;
}

.state-bar {
  display: grid;
  grid-template-columns: repeat(5, minmax(0, 1fr));
  gap: 12px;
  padding: 12px;
  border-radius: 18px;
}

.pill {
  min-height: 72px;
  display: grid;
  align-content: center;
  justify-items: center;
  gap: 10px;
  padding: 5px 12px;
  background: rgba(9, 22, 36, 0.7);
  border: 1px solid rgba(103, 212, 255, 0.08);
  text-align: center;
}

.pill span {
  color: var(--text-dim);
  font-size: 16px;
  line-height: 1;
}

.pill strong {
  font-size: 20px;
  line-height: 1.1;
}

@media (max-width: 1440px) {
  .hud-grid {
    grid-template-columns: 78px 1fr;
  }

  .state-bar {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (max-width: 1024px) {
  .overlay-layer {
    padding: 12px;
  }

  .mode-switch {
    width: 100%;
  }

  .mode-btn {
    flex: 1;
    width: auto;
  }

  .hud-grid,
  .state-bar {
    grid-template-columns: 1fr;
  }

  .motion-actions {
    grid-auto-flow: column;
    grid-template-columns: repeat(3, 53px);
    min-height: auto;
  }

  .metrics {
    justify-content: start;
  }
}
</style>
