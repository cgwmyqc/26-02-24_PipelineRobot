<template>
  <div class="dashboard-page">
    <AppHeader
      :connected="rosConnected"
      :user-name="authStore.user?.displayName || authStore.user?.username || 'admin'"
      @logout="handleLogout"
      @open-ota="openDialog"
    />

    <main class="dashboard-shell">
      <section class="dashboard-primary">
        <div class="panel control-panel-shell">
          <div class="panel-title">主控面板</div>
          <div class="panel-body control-panel-body">
            <ControlPanel
              :patrol-mode="uiPatrolMode"
              :temperature="temperature"
              :humidity="humidity"
              :travel-meters="travelMeters"
              :motor-enabled="motorEnabled"
              :motor-run-state="motorRunState"
              :motion-reached="motionReached"
              :laser-distance1-mm="laserDistance1Mm"
              :laser-distance2-mm="laserDistance2Mm"
              :laser-distance3-mm="laserDistance3Mm"
              :set-patrol-mode="setPatrolMode"
              :publish-move-command="publishMoveCommand"
              :start-auto-inspection="startAutoInspection"
              :manual-recording-active="manualRecordingActive"
              :toggle-manual-recording="toggleManualRecording"
              :capture-manual-snapshot="captureManualSnapshot"
              :publish-ui-script-command="publishUiScriptCommand"
              :test-mode-enabled="testModeEnabled"
              :test-state="testState"
              :trigger-count="triggerCount"
              :cooldown-until="cooldownUntil"
              :set-test-mode-enabled="setTestModeEnabled"
              :start-test-sequence="startTestSequence"
              :trigger-test-capture="triggerTestCapture"
              :finish-test-sequence="finishTestSequence"
            />
          </div>
        </div>

        <div class="secondary-grid">
          <AnalysisPanel />
          <HistoryQueryPanel
            :list="historyList"
            :loading="historyLoading"
            :exporting-id="exportingId"
            :page="historyPagination.page"
            :page-size="historyPagination.pageSize"
            :total="historyPagination.total"
            :initial-filters="historyFilters"
            @search="loadHistory"
            @page-change="changeHistoryPage"
            @detail="openDetail"
            @export="exportRecord"
          />
        </div>
      </section>

      <section class="dashboard-aside">
        <VideoPanel />
        <section class="panel point-panel">
          <div class="panel-title">点云数据</div>
          <div class="panel-body point-panel-body">
            <label :class="['fit-switch hud-panel', { disabled: isFitSwitchDisabled }]">
              <span class="fit-switch-label">点云拟合</span>
              <input
                class="fit-switch-input"
                type="checkbox"
                :checked="fitViewEnabled"
                :disabled="isFitSwitchDisabled"
                @change="handleFitViewToggle"
              >
              <span :class="['fit-switch-track', { active: fitViewEnabled && !isFitSwitchDisabled }]">
                <span class="fit-switch-thumb" />
              </span>
            </label>
            <PointCloudScene
              :mode="pointCloudSceneMode"
              :points="pointCloudPoints"
              :segment-labels="pointCloudSegmentLabels"
              :fitted-data="fittedPipeData"
            />
            <div v-if="fitViewEnabled && fittedPipeLoading" class="point-cloud-overlay">
              正在加载拟合结果...
            </div>
            <div v-else-if="fitViewEnabled && fittedPipeLoadError" class="point-cloud-overlay error">
              {{ fittedPipeLoadError }}
            </div>
          </div>
        </section>
      </section>
    </main>

    <InspectionDetailDialog
      :visible="detailVisible"
      :loading="detailLoading"
      :record="detailRecord"
      @close="closeDetail"
    />

    <FirmwareOtaDialog
      :visible="dialogVisible"
      :targets="targetCards"
      @request-close="handleOtaDialogRequestClose"
      @select-file="selectFile"
      @start="startUpload"
    />
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { storeToRefs } from 'pinia'
import { useRouter } from 'vue-router'
import AnalysisPanel from '../components/AnalysisPanel.vue'
import AppHeader from '../components/AppHeader.vue'
import ControlPanel from '../components/ControlPanel.vue'
import FirmwareOtaDialog from '../components/FirmwareOtaDialog.vue'
import HistoryQueryPanel from '../components/HistoryQueryPanel.vue'
import InspectionDetailDialog from '../components/InspectionDetailDialog.vue'
import PointCloudScene from '../components/PointCloudScene.vue'
import VideoPanel from '../components/VideoPanel.vue'
import { useFirmwareOta } from '../composables/useFirmwareOta'
import { useRosDashboard } from '../composables/useRosDashboard'
import { useAuthStore } from '../stores/auth'
import { useDashboardStore } from '../stores/dashboard'

const router = useRouter()
const authStore = useAuthStore()
const store = useDashboardStore()

const {
  patrolMode: storePatrolMode,
  temperature,
  humidity,
  travelMeters,
  motorEnabled,
  motorRunState,
  motionReached,
  laserDistance1Mm,
  laserDistance2Mm,
  laserDistance3Mm,
  rosConnected,
  historyList,
  historyLoading,
  historyFilters,
  historyPagination,
  detailVisible,
  detailLoading,
  detailRecord,
  exportingId,
  pointCloudPoints,
  pointCloudSegmentLabels,
  testModeEnabled,
  testState,
  triggerCount,
  cooldownUntil,
  fitViewEnabled,
  fitViewAvailable,
  fittedPipeData,
  fittedPipeLoading,
  fittedPipeLoadError
} = storeToRefs(store)

const {
  patrolMode: rosPatrolMode,
  setPatrolMode,
  publishMoveCommand,
  startAutoInspection,
  manualRecordingActive,
  toggleManualRecording,
  captureManualSnapshot,
  publishUiScriptCommand,
  setTestModeEnabled,
  startTestSequence,
  triggerTestCapture,
  finishTestSequence
} = useRosDashboard()
const {
  dialogVisible,
  targetCards,
  openDialog,
  requestCloseDialog,
  selectFile,
  startUpload
} = useFirmwareOta()

const uiPatrolMode = computed(() => rosPatrolMode.value || storePatrolMode.value)
const pointCloudSceneMode = computed(() => (fitViewEnabled.value ? 'fitted' : 'raw'))
const isFitSwitchDisabled = computed(() => !fitViewAvailable.value || fittedPipeLoading.value)
const { loadHistory, changeHistoryPage, openDetail, closeDetail, exportRecord } = store

function handleLogout() {
  authStore.logout()
  router.push('/login')
}

function handleFitViewToggle(event) {
  store.setFitViewEnabled(Boolean(event?.target?.checked))
}

async function handleOtaDialogRequestClose(done) {
  const closed = await requestCloseDialog()
  if (closed) {
    done()
  }
}
</script>

<style scoped>
.dashboard-page {
  min-height: 100vh;
  min-height: 100dvh;
  display: flex;
  flex-direction: column;
}

.dashboard-page > :deep(.app-header) {
  flex: 0 0 auto;
}

.dashboard-page > .dashboard-shell {
  flex: 1 1 auto;
  min-height: 100vh;
  min-height: 0;
}

.dashboard-shell {
  display: grid;
  grid-template-columns: minmax(0, 56.25fr) minmax(0, 43.75fr);
  grid-template-rows: minmax(0, 1fr);
  align-items: stretch;
  gap: 16px;
  padding: 0 18px 18px;
  min-height: 0;
}

.dashboard-primary,
.dashboard-aside,
.secondary-grid {
  display: grid;
  gap: 16px;
  min-height: 0;
}

.dashboard-primary,
.dashboard-aside {
  grid-template-rows: minmax(0, 1fr);
  height: 100%;
}

.secondary-grid > * {
  height: 100%;
}

.control-panel-shell {
  min-height: 400px;
  height: 100%;
  display: flex;
  flex-direction: column;
}

.control-panel-body {
  flex: 1;
  min-height: 0;
  display: flex;
  padding: 16px 18px 14px;
}

.secondary-grid {
  grid-template-columns: minmax(0, 0.72fr) minmax(0, 1.28fr);
  align-items: stretch;
}

.dashboard-aside {
  /* grid-template-rows: 420px minmax(0, 1fr); */
  grid-template-rows: 500px minmax(0, 1fr);
  height: 100%;
}

.point-panel {
  height: 100%;
  min-height: 300px;
  display: flex;
  flex-direction: column;
}

.point-panel-body {
  flex: 1;
  position: relative;
  height: calc(100% - 46px);
  min-height: 320px;
  padding: 0;
  overflow: hidden;
}

.fit-switch {
  position: absolute;
  top: 14px;
  right: 14px;
  z-index: 2;
  display: inline-flex;
  align-items: center;
  gap: 12px;
  min-height: 40px;
  padding: 8px 14px;
  border-radius: 999px;
  cursor: pointer;
  user-select: none;
  pointer-events: auto;
}

.fit-switch-label {
  color: var(--text-primary);
  white-space: nowrap;
  font-size: 14px;
  line-height: 1;
}

.fit-switch-input {
  position: absolute;
  opacity: 0;
  pointer-events: none;
}

.fit-switch-track {
  position: relative;
  width: 46px;
  height: 24px;
  border-radius: 999px;
  background: rgba(140, 159, 178, 0.35);
  transition: background 0.2s ease;
}

.fit-switch-track.active {
  background: linear-gradient(135deg, #88f1ba, #60f2df);
}

.fit-switch-thumb {
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

.fit-switch-track.active .fit-switch-thumb {
  transform: translateX(22px);
}

.fit-switch.disabled {
  cursor: not-allowed;
  opacity: 0.72;
}

.point-cloud-overlay {
  position: absolute;
  left: 16px;
  right: 16px;
  bottom: 14px;
  padding: 10px 12px;
  border-radius: 12px;
  background: rgba(6, 16, 27, 0.76);
  border: 1px solid rgba(105, 140, 168, 0.35);
  color: rgba(227, 240, 252, 0.9);
  font-size: 13px;
  line-height: 1.4;
  backdrop-filter: blur(8px);
}

.point-cloud-overlay.error {
  color: rgba(255, 214, 214, 0.96);
  border-color: rgba(212, 89, 89, 0.45);
  background: rgba(42, 11, 11, 0.72);
}

.dashboard-primary > *,
.dashboard-aside > * {
  min-height: 0;
}

@media (max-width: 1440px) {
  .dashboard-shell,
  .secondary-grid {
    grid-template-columns: 1fr;
  }

  .dashboard-shell {
    grid-template-rows: auto;
  }

  .dashboard-primary,
  .dashboard-aside {
    grid-template-rows: auto;
  }
}
</style>
