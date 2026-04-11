<template>
  <section class="control-layout">
    <div class="control-stage">
      <div class="scene-layer">
        <PipeThreeScene />
      </div>

      <div class="overlay-layer">
        <div class="control-settings">
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

          <label class="test-switch hud-panel">
            <span class="test-switch-label">测试模式</span>
            <input v-model="testModeModel" type="checkbox" class="test-switch-input">
            <span :class="['test-switch-track', { active: testModeEnabled }]">
              <span class="test-switch-thumb" />
            </span>
          </label>
        </div>

        <div class="hud-grid">
          <div class="action-groups">
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

            <div class="manual-actions hud-panel hud-block">
              <button
                class="image-action-btn"
                :class="{ inactive: !isManualMode }"
                :disabled="!isManualMode"
                @click="toggleManualRecording"
              >
                <img :src="recordButtonImage" alt="录制按钮">
              </button>

              <button
                class="image-action-btn"
                :class="{ inactive: !isManualMode || !manualRecordingActive }"
                :disabled="!isManualMode || !manualRecordingActive"
                @click="captureManualSnapshot"
              >
                <img :src="captureButtonImage" alt="拍照按钮">
              </button>
            </div>

            <div v-if="testModeEnabled" class="test-actions hud-panel hud-block">
              <div class="test-meta">
                <span>状态：{{ testStateLabel }}</span>
                <span>已触发：{{ triggerCount }}/14</span>
              </div>

              <button
                class="test-action-btn"
                :class="{ disabled: isStartTestDisabled }"
                :disabled="isStartTestDisabled"
                @click="startTestSequence"
              >
                开始测试
              </button>
              <button
                class="test-action-btn"
                :class="{ disabled: isTriggerDisabled }"
                :disabled="isTriggerDisabled"
                @click="triggerTestCapture"
              >
                触发
              </button>
              <button
                class="test-action-btn"
                :class="{ disabled: isFinishDisabled }"
                :disabled="isFinishDisabled"
                @click="finishTestSequence"
              >
                测试完成
              </button>
            </div>
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
              value="0.0"
              unit="cm"
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
import { TEST_MODE_STATES } from '../stores/dashboard'

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
  startAutoInspection: Function,
  manualRecordingActive: Boolean,
  toggleManualRecording: Function,
  captureManualSnapshot: Function,
  publishUiScriptCommand: Function,
  testModeEnabled: Boolean,
  testState: String,
  triggerCount: Number,
  cooldownUntil: Number,
  setTestModeEnabled: Function,
  startTestSequence: Function,
  triggerTestCapture: Function,
  finishTestSequence: Function
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
const recordStopImage = resolveImageAsset('record_stop')
const recordIngImage = resolveImageAsset('record_ing')
const captureButtonImage = resolveImageAsset('capture')

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
const recordButtonImage = computed(() => (props.manualRecordingActive ? recordIngImage : recordStopImage))

const testModeModel = computed({
  get: () => Boolean(props.testModeEnabled),
  set: (value) => props.setTestModeEnabled?.(value)
})

const testStateLabel = computed(() => {
  const mapping = {
    [TEST_MODE_STATES.WAITING_START]: '等待开始',
    [TEST_MODE_STATES.STARTING_SYSTEM]: '系统启动中',
    [TEST_MODE_STATES.WAITING_TRIGGER]: '等待触发',
    [TEST_MODE_STATES.PROCESSING_CAPTURE]: '数据处理中',
    [TEST_MODE_STATES.READY_FINISH]: '等待完成',
    [TEST_MODE_STATES.COOLDOWN]: '冷却中',
    [TEST_MODE_STATES.IDLE]: '未开启'
  }
  return mapping[props.testState] || '未开启'
})

const isStartTestDisabled = computed(() => props.testState !== TEST_MODE_STATES.WAITING_START)
const isTriggerDisabled = computed(() => props.testState !== TEST_MODE_STATES.WAITING_TRIGGER)
const isFinishDisabled = computed(() => props.testState !== TEST_MODE_STATES.READY_FINISH)

function toggleMove(direction, active) {
  if (!isManualMode.value) {
    return
  }
  props.publishMoveCommand?.(direction, active)
}
</script>

<style scoped>
.control-layout {
  flex: 1;
  min-height: 0;
  display: flex;
}

.control-stage {
  position: relative;
  flex: 1;
  height: auto;
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
  position: absolute;
  inset: 0;
  z-index: 1;
  display: grid;
  grid-template-rows: auto 1fr auto;
  gap: 16px;
  min-height: 0;
  padding: 16px;
  pointer-events: none;
}

.control-settings {
  display: grid;
  justify-items: start;
  gap: 12px;
}

.hud-grid {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  min-height: 0;
}

.hud-panel {
  background: rgba(8, 19, 31, 0.48);
  backdrop-filter: blur(8px);
  border: 1px solid rgba(103, 212, 255, 0.12);
  box-shadow: 0 10px 24px rgba(3, 8, 15, 0.24);
}

.hud-block,
.mode-switch,
.test-switch,
.state-bar,
.image-action-btn,
.test-action-btn {
  pointer-events: auto;
}

.mode-switch {
  display: inline-flex;
  width: fit-content;
  border-radius: 999px;
  overflow: hidden;
}

.test-switch {
  display: inline-flex;
  align-items: center;
  gap: 12px;
  min-height: 40px;
  padding: 8px 14px;
  border-radius: 999px;
  cursor: pointer;
}

.test-switch-label {
  color: var(--text-primary);
  font-size: 14px;
  line-height: 1;
}

.test-switch-input {
  position: absolute;
  opacity: 0;
  pointer-events: none;
}

.test-switch-track {
  position: relative;
  width: 46px;
  height: 24px;
  border-radius: 999px;
  background: rgba(140, 159, 178, 0.35);
  transition: background 0.2s ease;
}

.test-switch-track.active {
  background: linear-gradient(135deg, #88f1ba, #60f2df);
}

.test-switch-thumb {
  position: absolute;
  top: 3px;
  left: 3px;
  width: 18px;
  height: 18px;
  border-radius: 50%;
  background: #f7fffb;
  box-shadow: 0 4px 12px rgba(3, 8, 15, 0.28);
  transition: transform 0.2s ease;
}

.test-switch-track.active .test-switch-thumb {
  transform: translateX(22px);
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

.action-groups {
  display: flex;
  align-items: center;
  gap: 16px;
  min-height: 0;
}

.motion-actions {
  display: grid;
  flex: 0 0 78px;
  align-content: center;
  justify-items: center;
  gap: 16px;
  min-height: 240px;
  padding: 16px 10px;
  border-radius: 18px;
}

.test-actions {
  display: grid;
  align-content: center;
  gap: 12px;
  min-height: 240px;
  min-width: 144px;
  padding: 16px 12px;
  border-radius: 18px;
}

.manual-actions {
  display: grid;
  align-content: center;
  justify-items: center;
  gap: 16px;
  min-height: 240px;
  padding: 16px 10px;
  border-radius: 18px;
}

.test-meta {
  display: grid;
  gap: 6px;
  color: var(--text-dim);
  font-size: 12px;
  line-height: 1.2;
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

.test-action-btn {
  min-width: 112px;
  min-height: 44px;
  padding: 10px 14px;
  border: 1px solid rgba(103, 212, 255, 0.24);
  border-radius: 14px;
  background: rgba(13, 27, 41, 0.92);
  color: var(--brand);
  font-size: 14px;
  line-height: 1.2;
  cursor: pointer;
  transition: transform 0.2s ease, border-color 0.2s ease, background 0.2s ease;
}

.test-action-btn:hover {
  transform: translateY(-1px);
  border-color: rgba(103, 212, 255, 0.42);
  background: rgba(17, 36, 54, 0.96);
}

.test-action-btn.disabled,
.test-action-btn:disabled {
  cursor: not-allowed;
  opacity: 0.46;
  transform: none;
}

.metrics {
  display: grid;
  justify-content: start;
  margin-left: auto;
  align-content: center;
  gap: 16px;
  min-height: 240px;
  width: 214px;
  max-width: 214px;
  padding: 14px 12px;
  border-radius: 18px;
}

.state-bar {
  display: grid;
  grid-template-columns: repeat(5, minmax(0, 1fr));
  gap: 12px;
  background: transparent;
  border: 0;
  box-shadow: none;
  padding: 0;
}

.pill {
  min-height: 72px;
  display: grid;
  align-content: center;
  justify-items: center;
  gap: 10px;
  padding: 5px 12px;
  background: transparent;
  border: 0;
  text-align: center;
  border-radius: 18px;
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
    justify-content: space-between;
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

  .test-switch {
    width: fit-content;
  }

  .mode-btn {
    flex: 1;
    width: auto;
  }

  .hud-grid,
  .state-bar {
    grid-template-columns: 1fr;
  }

  .hud-grid {
    display: grid;
  }

  .action-groups {
    flex-wrap: wrap;
  }

  .motion-actions {
    grid-auto-flow: column;
    grid-template-columns: repeat(3, 53px);
    min-height: auto;
  }

  .manual-actions {
    grid-auto-flow: column;
    grid-template-columns: repeat(2, 53px);
    min-height: auto;
  }

  .test-actions {
    grid-template-columns: repeat(3, minmax(0, 1fr));
    min-height: auto;
    min-width: 0;
    width: 100%;
  }

  .test-meta {
    grid-column: 1 / -1;
  }

  .test-action-btn {
    min-width: 0;
  }

  .metrics {
    justify-content: start;
    margin-left: 0;
    width: 100%;
    max-width: none;
  }
}
</style>
