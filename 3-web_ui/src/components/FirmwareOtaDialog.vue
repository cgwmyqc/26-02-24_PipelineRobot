<template>
  <el-dialog
    :model-value="visible"
    title="固件刷写"
    width="min(1080px, calc(100vw - 32px))"
    destroy-on-close
    :before-close="handleBeforeClose"
  >
    <div class="ota-dialog-shell">
      <section
        v-for="target in targets"
        :key="target.key"
        class="ota-card"
      >
        <div class="ota-card-header">
          <div>
            <div class="ota-card-title">{{ target.label }}</div>
            <div class="ota-card-subtitle">{{ target.key }}</div>
          </div>
          <span :class="['ota-phase-tag', `phase-${target.state.statusPhase}`]">
            {{ phaseLabelMap[target.state.statusPhase] || '待命' }}
          </span>
        </div>

        <div class="ota-meta">
          <div class="ota-meta-item">
            <span class="meta-label">命令话题</span>
            <code>{{ target.commandTopic.name }}</code>
          </div>
          <div class="ota-meta-item">
            <span class="meta-label">状态话题</span>
            <code>{{ target.statusTopic.name }}</code>
          </div>
          <div class="ota-meta-item">
            <span class="meta-label">进度话题</span>
            <code>{{ target.progressTopic.name }}</code>
          </div>
        </div>

        <label class="ota-file-picker">
          <span>选择本地 .bin 固件</span>
          <input
            type="file"
            accept=".bin,application/octet-stream"
            :disabled="target.isLocked"
            @change="handleFileChange(target.key, $event)"
          >
        </label>

        <div class="ota-file-name">
          {{ target.state.fileName || '尚未选择文件' }}
        </div>

        <div class="ota-progress-group">
          <div class="ota-progress-row">
            <span>上传进度</span>
            <strong>{{ target.state.uploadProgress }}%</strong>
          </div>
          <el-progress :percentage="target.state.uploadProgress" :stroke-width="10" />
        </div>

        <div class="ota-progress-group">
          <div class="ota-progress-row">
            <span>设备刷写进度</span>
            <strong>{{ target.state.deviceProgress }}%</strong>
          </div>
          <el-progress
            :percentage="target.state.deviceProgress"
            :stroke-width="10"
            :status="target.state.statusPhase === 'error' ? 'exception' : undefined"
          />
        </div>

        <div class="ota-status-box">
          <div class="ota-status-title">当前状态</div>
          <div class="ota-status-message">{{ target.state.statusMessage || '待命' }}</div>
          <div v-if="target.state.error" class="ota-status-error">{{ target.state.error }}</div>
        </div>

        <el-button
          type="success"
          :loading="target.state.busy"
          :disabled="!target.canStart"
          @click="$emit('start', target.key)"
        >
          {{ target.startButtonText }}
        </el-button>
      </section>
    </div>
  </el-dialog>
</template>

<script setup>
defineProps({
  visible: {
    type: Boolean,
    default: false
  },
  targets: {
    type: Array,
    default: () => []
  }
})

const emit = defineEmits(['request-close', 'select-file', 'start'])

const phaseLabelMap = {
  idle: '待命',
  uploading: '上传中',
  queued: '命令已发送',
  downloading: '下载中',
  validating: '校验中',
  writing: '写入中',
  success: '成功',
  error: '失败'
}

function handleFileChange(targetKey, event) {
  const [file] = event?.target?.files || []
  emit('select-file', targetKey, file || null)
}

function handleBeforeClose(done) {
  emit('request-close', done)
}
</script>

<style scoped>
.ota-dialog-shell {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 16px;
}

.ota-card {
  padding: 18px;
  border: 1px solid rgba(103, 212, 255, 0.12);
  background: linear-gradient(180deg, rgba(14, 28, 44, 0.88), rgba(9, 20, 34, 0.94));
  box-shadow: inset 0 0 0 1px rgba(117, 240, 194, 0.03);
}

.ota-card-header,
.ota-progress-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
}

.ota-card-title {
  font-size: 20px;
  font-weight: 700;
}

.ota-card-subtitle {
  margin-top: 4px;
  color: var(--text-dim);
  font-size: 12px;
  letter-spacing: 0.4px;
}

.ota-phase-tag {
  padding: 6px 10px;
  border: 1px solid rgba(103, 212, 255, 0.16);
  background: rgba(9, 18, 30, 0.58);
  font-size: 12px;
}

.phase-success {
  color: #75f0c2;
}

.phase-error {
  color: #ff897d;
}

.ota-meta {
  display: grid;
  gap: 10px;
  margin: 18px 0 16px;
}

.ota-meta-item,
.ota-file-picker,
.ota-status-box {
  display: grid;
  gap: 8px;
}

.meta-label,
.ota-status-title {
  color: var(--text-dim);
  font-size: 12px;
}

.ota-meta-item code {
  padding: 8px 10px;
  background: rgba(8, 17, 29, 0.88);
  border: 1px solid rgba(103, 212, 255, 0.08);
  color: #9adfff;
  word-break: break-all;
}

.ota-file-picker {
  margin-bottom: 10px;
}

.ota-file-picker input {
  width: 100%;
  padding: 10px;
  color: var(--text);
  background: rgba(8, 17, 29, 0.88);
  border: 1px solid rgba(103, 212, 255, 0.08);
}

.ota-file-name {
  min-height: 20px;
  margin-bottom: 14px;
  color: var(--text-dim);
  font-size: 13px;
}

.ota-progress-group {
  margin-bottom: 14px;
}

.ota-status-box {
  min-height: 92px;
  margin-bottom: 16px;
  padding: 12px;
  background: rgba(6, 14, 24, 0.76);
  border: 1px solid rgba(103, 212, 255, 0.08);
}

.ota-status-message {
  color: var(--text);
}

.ota-status-error {
  color: #ff897d;
  font-size: 13px;
  word-break: break-word;
}

@media (max-width: 900px) {
  .ota-dialog-shell {
    grid-template-columns: minmax(0, 1fr);
  }
}
</style>
