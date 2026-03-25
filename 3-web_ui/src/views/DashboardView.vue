<template>
  <div class="dashboard-page">
    <AppHeader :connected="rosConnected" :user-name="authStore.user?.displayName || authStore.user?.username || 'admin'" @logout="handleLogout" />

    <main class="dashboard-shell">
      <section class="dashboard-primary">
        <div class="panel control-panel-shell">
          <div class="panel-title">主控面板</div>
          <div class="panel-body control-panel-body">
            <ControlPanel
              :patrol-mode="patrolMode"
              :temperature="temperature"
              :travel-meters="travelMeters"
              :motor-enabled="motorEnabled"
              :motor-run-state="motorRunState"
              :encoder-count="encoderCount"
              :water-detected="waterDetected"
              :motion-reached="motionReached"
              :set-patrol-mode="setPatrolMode"
              :publish-move-command="publishMoveCommand"
              :start-auto-inspection="startAutoInspection"
              :mark-detect-done="markDetectDone"
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
        <VideoPanel :frame="videoFrame" :connected="rosConnected" />
        <section class="panel point-panel">
          <div class="panel-title">点云数据</div>
          <div class="panel-body">
            <PointCloudScene :points="pointCloudPoints" />
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
  </div>
</template>

<script setup>
import { storeToRefs } from 'pinia'
import { useRouter } from 'vue-router'
import AnalysisPanel from '../components/AnalysisPanel.vue'
import AppHeader from '../components/AppHeader.vue'
import ControlPanel from '../components/ControlPanel.vue'
import HistoryQueryPanel from '../components/HistoryQueryPanel.vue'
import InspectionDetailDialog from '../components/InspectionDetailDialog.vue'
import PointCloudScene from '../components/PointCloudScene.vue'
import VideoPanel from '../components/VideoPanel.vue'
import { useRosDashboard } from '../composables/useRosDashboard'
import { useAuthStore } from '../stores/auth'
import { useDashboardStore } from '../stores/dashboard'

const router = useRouter()
const authStore = useAuthStore()
const store = useDashboardStore()

const {
  patrolMode,
  temperature,
  travelMeters,
  motorEnabled,
  motorRunState,
  encoderCount,
  waterDetected,
  motionReached,
  rosConnected,
  videoFrame,
  historyList,
  historyLoading,
  historyFilters,
  historyPagination,
  detailVisible,
  detailLoading,
  detailRecord,
  exportingId,
  pointCloudPoints
} = storeToRefs(store)

const { setPatrolMode, publishMoveCommand, startAutoInspection, markDetectDone } = useRosDashboard()
const { loadHistory, changeHistoryPage, openDetail, closeDetail, exportRecord } = store

function handleLogout() {
  authStore.logout()
  router.push('/login')
}
</script>

<style scoped>
.dashboard-page {
  min-height: 100vh;
}

.dashboard-shell {
  display: grid;
  grid-template-columns: minmax(0, 1.8fr) minmax(360px, 1.1fr);
  gap: 16px;
  padding: 18px;
}

.dashboard-primary,
.dashboard-aside,
.secondary-grid {
  display: grid;
  gap: 16px;
}

.secondary-grid > * {
  height: 100%;
}

.control-panel-shell {
  min-height: 620px;
}

.control-panel-body {
  padding: 18px;
}

.secondary-grid {
  grid-template-columns: minmax(0, 0.7fr) minmax(0, 1.3fr);
  align-items: stretch;
}

.point-panel {
  min-height: 340px;
}

@media (max-width: 1440px) {
  .dashboard-shell,
  .secondary-grid {
    grid-template-columns: 1fr;
  }
}
</style>
