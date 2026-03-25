<template>
  <el-dialog
    :model-value="visible"
    width="min(960px, 92vw)"
    class="inspection-dialog"
    destroy-on-close
    @close="$emit('close')"
  >
    <template #header>
      <div class="dialog-header">
        <div>
          <h3>巡检详情</h3>
          <p>{{ record?.inspectionTime || record?.createdAt || '--' }}</p>
        </div>
      </div>
    </template>

    <div v-loading="loading" class="dialog-body">
      <div class="detail-grid">
        <section class="detail-card meta-card">
          <div class="meta-row">
            <span>巡检方式</span>
            <strong>{{ record?.mode || '--' }}</strong>
          </div>
          <div class="meta-row">
            <span>管道环境</span>
            <strong>{{ record?.environment || '--' }}</strong>
          </div>
          <div class="meta-row">
            <span>操作人</span>
            <strong>{{ record?.operator || '--' }}</strong>
          </div>
          <div class="meta-row">
            <span>分析结果</span>
            <strong>{{ record?.result || '--' }}</strong>
          </div>
        </section>

        <section class="detail-card video-card">
          <div class="section-title">巡检视频</div>
          <video v-if="record?.videoUrl" :src="record.videoUrl" controls class="video-player"></video>
          <div v-else class="empty-state">当前记录未配置视频</div>
        </section>
      </div>

      <section class="detail-card image-card">
        <div class="section-title">异常图片</div>
        <div v-if="record?.anomalies?.length" class="image-grid">
          <button
            v-for="item in record.anomalies"
            :key="item.id"
            type="button"
            class="image-tile"
            @click="activeImage = item.imageUrl"
          >
            <img v-if="item.imageUrl" :src="item.imageUrl" :alt="item.anomalyType || '异常图片'" />
            <div v-else class="tile-empty">暂无图片</div>
            <div class="image-caption">
              <strong>{{ item.anomalyType || '异常' }}</strong>
              <span>{{ item.remark || '未填写备注' }}</span>
            </div>
          </button>
        </div>
        <div v-else class="empty-state">该巡检记录未发现异常图片</div>
      </section>

      <section v-if="activeImage" class="detail-card preview-card">
        <div class="section-title">图片预览</div>
        <img :src="activeImage" alt="异常图片预览" class="preview-image" />
      </section>
    </div>
  </el-dialog>
</template>

<script setup>
import { ref, watch } from 'vue'

const props = defineProps({
  visible: {
    type: Boolean,
    default: false
  },
  loading: {
    type: Boolean,
    default: false
  },
  record: {
    type: Object,
    default: null
  }
})

defineEmits(['close'])

const activeImage = ref('')

watch(
  () => props.record,
  (value) => {
    activeImage.value = value?.anomalies?.[0]?.imageUrl || ''
  },
  { immediate: true }
)
</script>

<style scoped>
.dialog-header h3 {
  margin: 0 0 6px;
  font-size: 22px;
}

.dialog-header p {
  margin: 0;
  color: var(--text-dim);
}

.dialog-body {
  display: grid;
  gap: 16px;
}

.detail-grid {
  display: grid;
  grid-template-columns: 280px 1fr;
  gap: 16px;
}

.detail-card {
  border: 1px solid rgba(103, 212, 255, 0.14);
  background: rgba(7, 18, 30, 0.82);
  padding: 18px;
}

.section-title {
  margin-bottom: 14px;
  color: #93f5cf;
  font-size: 16px;
  font-weight: 700;
}

.meta-card {
  display: grid;
  gap: 14px;
}

.meta-row {
  display: flex;
  justify-content: space-between;
  gap: 12px;
  padding-bottom: 10px;
  border-bottom: 1px solid rgba(103, 212, 255, 0.08);
}

.meta-row span {
  color: var(--text-dim);
}

.meta-row strong {
  text-align: right;
}

.video-player {
  width: 100%;
  min-height: 320px;
  background: #07111d;
}

.image-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(210px, 1fr));
  gap: 14px;
}

.image-tile {
  padding: 0;
  border: 1px solid rgba(103, 212, 255, 0.12);
  background: rgba(14, 26, 40, 0.92);
  color: inherit;
  text-align: left;
  cursor: pointer;
}

.image-tile img,
.tile-empty {
  width: 100%;
  height: 140px;
  object-fit: cover;
  display: block;
  background: #07111d;
}

.tile-empty,
.empty-state {
  display: grid;
  place-items: center;
  color: var(--text-dim);
}

.image-caption {
  display: grid;
  gap: 6px;
  padding: 12px;
}

.image-caption span {
  color: var(--text-dim);
  font-size: 13px;
}

.preview-image {
  width: 100%;
  max-height: 420px;
  object-fit: contain;
  background: #07111d;
}

@media (max-width: 900px) {
  .detail-grid {
    grid-template-columns: 1fr;
  }
}
</style>
