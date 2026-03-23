<template>
  <div class="dashboard-page">
    <AppHeader :connected="rosConnected" />

    <div class="dashboard-grid top-grid">
      <section class="left-stage">
        <ControlPanel
          :patrol-mode="patrolMode"
          :temperature="temperature"
          :sludge-thickness="sludgeThickness"
          :publish-patrol-mode="publishPatrolMode"
          :publish-move-command="publishMoveCommand"
        />
      </section>

      <div class="right-stage">
        <VideoPanel :frame="videoFrame" :connected="rosConnected" />
        <HistoryQueryPanel :list="historyList" :loading="historyLoading" @search="loadHistory" />
      </div>
    </div>

    <div class="dashboard-grid bottom-grid">
      <AnalysisPanel />
      <section class="panel point-panel">
        <div class="panel-title">点云数据</div>
        <div class="panel-body">
          <PointCloudScene :points="pointCloudPoints" />
        </div>
      </section>
    </div>
  </div>
</template>

<script setup>
import { storeToRefs } from 'pinia'
import AnalysisPanel from '../components/AnalysisPanel.vue'
import AppHeader from '../components/AppHeader.vue'
import ControlPanel from '../components/ControlPanel.vue'
import HistoryQueryPanel from '../components/HistoryQueryPanel.vue'
import PointCloudScene from '../components/PointCloudScene.vue'
import VideoPanel from '../components/VideoPanel.vue'
import { useRosDashboard } from '../composables/useRosDashboard'
import { useDashboardStore } from '../stores/dashboard'

const store = useDashboardStore()
const { patrolMode, temperature, sludgeThickness, rosConnected, videoFrame, historyList, historyLoading, pointCloudPoints } = storeToRefs(store)
const { publishPatrolMode, publishMoveCommand } = useRosDashboard()
const { loadHistory } = store
</script>

<style scoped>
.dashboard-page {
  min-height: 100vh;
}
.top-grid {
  align-items: start;
}
.left-stage {
  min-height: 760px;
  padding: 18px 12px 0 12px;
}
.right-stage {
  display: grid;
  gap: 14px;
}
.bottom-grid {
  margin-top: -270px;
  grid-template-columns: 30% 28%;
  width: 58%;
}
.point-panel {
  min-height: 285px;
}
@media (max-width: 1440px) {
  .bottom-grid {
    margin-top: -220px;
  }
}
@media (max-width: 1280px) {
  .dashboard-grid,
  .bottom-grid {
    grid-template-columns: 1fr;
    width: auto;
  }
  .bottom-grid {
    margin-top: 16px;
  }
  .left-stage {
    min-height: auto;
  }
}
</style>
