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
            <strong :class="{ 'result-anomaly': detailResultSummary === '异常' }">
              {{ detailResultSummary }}
            </strong>
          </div>
        </section>

        <section class="detail-card video-card">
          <div class="section-title">巡检视频</div>
          <video
            v-if="primaryVideoUrl"
            :src="primaryVideoUrl"
            controls
            class="video-player"
          ></video>
          <div v-else class="empty-state">当前记录未配置视频</div>

          <div v-if="videoList.length > 1" class="asset-list">
            <button
              v-for="item in videoList"
              :key="item.filePath || item.fileUrl"
              class="asset-item"
              type="button"
              @click="activeVideo = item.fileUrl"
            >
              {{ item.fileName || item.filePath || '视频文件' }}
            </button>
          </div>
        </section>
      </div>

      <section class="detail-card image-card">
        <div class="section-title">异常图片</div>
        <div v-if="imageList.length" class="image-grid">
          <button
            v-for="item in imageList"
            :key="item.id || item.filePath || item.imageUrl"
            type="button"
            class="image-tile"
            @click="activeImage = item.fileUrl || item.imageUrl"
          >
            <img
              v-if="item.fileUrl || item.imageUrl"
              :src="item.fileUrl || item.imageUrl"
              :alt="getAnomalyLabel(item)"
            />
            <div v-else class="tile-empty">暂无图片</div>
            <div class="image-caption">
              <strong>{{ getAnomalyLabel(item) }}</strong>
            </div>
          </button>
        </div>
        <div v-else class="empty-state">该巡检记录未发现异常图片</div>
      </section>

      <section v-if="activeImage" class="detail-card preview-card">
        <div class="section-title">图片预览</div>
        <img :src="activeImage" alt="异常图片预览" class="preview-image" />
      </section>

      <section class="detail-card point-card">
        <div class="section-title">点云数据</div>
        <div v-if="pointList.length" class="point-content">
          <div v-if="pointList.length > 1" class="asset-list">
            <button
              v-for="item in pointList"
              :key="item.filePath || item.fileUrl"
              type="button"
              class="asset-item"
              :class="{ active: activePointKey === getPointItemKey(item) }"
              @click="setActivePointFile(item)"
            >
              {{ item.fileName || item.filePath || '点云文件' }}
            </button>
          </div>

          <div v-if="pointCloudLoading" class="point-placeholder">点云文件加载中...</div>
          <div v-else-if="pointCloudLoadError" class="point-placeholder">
            {{ pointCloudLoadError }}
          </div>
          <div v-else-if="detailPointCloudPoints.length" class="point-scene-wrap">
            <PointCloudScene :points="detailPointCloudPoints" mode="raw" />
          </div>
          <div v-else class="point-placeholder">当前记录暂无可展示的点云数据</div>
        </div>
        <div v-else class="empty-state">当前记录暂无点云文件</div>
      </section>
    </div>
  </el-dialog>
</template>

<script setup>
import { computed, ref, watch } from 'vue'
import { PCDLoader } from 'three/examples/jsm/loaders/PCDLoader.js'
import PointCloudScene from './PointCloudScene.vue'
import request from '../utils/request'

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

const ANOMALY_MAP = Object.freeze({
  PL: '破裂',
  BX: '变形',
  SG: '树根',
  ZAW: '阻碍物',
  RG: '人工识别'
})
const MAX_IMPORTED_PCD_POINTS = 5000
const pcdLoader = new PCDLoader()

const activeImage = ref('')
const activeVideo = ref('')
const activePointFile = ref(null)
const pointCloudLoading = ref(false)
const pointCloudLoadError = ref('')
const detailPointCloudPoints = ref([])

const videoList = computed(() => {
  if (Array.isArray(props.record?.videos) && props.record.videos.length) {
    return props.record.videos
  }
  if (props.record?.videoUrl) {
    return [{ fileUrl: props.record.videoUrl, fileName: '巡检视频' }]
  }
  return []
})

const imageList = computed(() => {
  if (Array.isArray(props.record?.images) && props.record.images.length) {
    return props.record.images
  }
  if (Array.isArray(props.record?.anomalies) && props.record.anomalies.length) {
    return props.record.anomalies
  }
  return []
})

const pointList = computed(() => {
  if (Array.isArray(props.record?.points) && props.record.points.length) {
    return props.record.points
  }
  return []
})
const activePointKey = computed(() => getPointItemKey(activePointFile.value))
const detailResultSummary = computed(() => normalizeDetailResult(props.record?.result))

const primaryVideoUrl = computed(() => {
  if (activeVideo.value) {
    return activeVideo.value
  }
  return videoList.value[0]?.fileUrl || ''
})

function normalizeDetailResult(value) {
  if (value === null || value === undefined) {
    return '--'
  }

  const normalized = String(value).trim()
  if (!normalized) {
    return '--'
  }

  if (normalized === '0' || normalized === '无异常') {
    return '无异常'
  }

  if (normalized === '--') {
    return '--'
  }

  return '异常'
}

function parseAnomalyFromFileName(item) {
  const candidates = [
    item?.fileName,
    item?.remark,
    item?.filePath,
    item?.imageUrl,
    item?.fileUrl
  ]
    .map((value) => String(value || '').trim())
    .filter(Boolean)

  for (const candidate of candidates) {
    const fileName = candidate.split('/').pop() || candidate
    const baseName = fileName.replace(/\.[^.]+$/, '')
    const parts = baseName
      .split('_')
      .map((part) => part.trim().toUpperCase())
      .filter((part) => ANOMALY_MAP[part])

    if (parts.length) {
      return parts.map((part) => ANOMALY_MAP[part]).join('/')
    }
  }

  return ''
}

function mapAnomalyType(value) {
  const normalized = String(value || '').trim().toUpperCase()
  if (!normalized) {
    return '异常'
  }

  const parts = normalized
    .split('+')
    .map((item) => item.trim())
    .filter(Boolean)

  if (!parts.length) {
    return normalized || '异常'
  }

  return parts.map((item) => ANOMALY_MAP[item] || item).join(' / ')
}

function getAnomalyLabel(itemOrValue) {
  if (itemOrValue && typeof itemOrValue === 'object') {
    return parseAnomalyFromFileName(itemOrValue) || mapAnomalyType(itemOrValue.anomalyType)
  }
  return mapAnomalyType(itemOrValue)
}

function getPointItemKey(item) {
  if (!item) {
    return ''
  }
  return item.filePath || item.fileUrl || item.fileName || ''
}

function parsePcdArrayBuffer(arrayBuffer) {
  const parsed = pcdLoader.parse(arrayBuffer, '')
  const positionAttribute = parsed?.geometry?.getAttribute('position')
  if (!positionAttribute?.array?.length) {
    return []
  }

  const step = Math.max(1, Math.ceil(positionAttribute.count / MAX_IMPORTED_PCD_POINTS))
  const points = []

  for (let index = 0; index < positionAttribute.count; index += step) {
    const offset = index * 3
    const x = positionAttribute.array[offset]
    const y = positionAttribute.array[offset + 1]
    const z = positionAttribute.array[offset + 2]

    if (Number.isFinite(x) && Number.isFinite(y) && Number.isFinite(z)) {
      points.push({ x, y, z })
    }
  }

  return points
}

async function loadPointCloud(item) {
  const fileUrl = item?.fileUrl
  if (!fileUrl) {
    detailPointCloudPoints.value = []
    pointCloudLoadError.value = '当前点云文件地址无效'
    return
  }

  pointCloudLoading.value = true
  pointCloudLoadError.value = ''

  try {
    const response = await request.get(fileUrl, {
      responseType: 'arraybuffer',
      baseURL: ''
    })
    const points = parsePcdArrayBuffer(response.data)
    if (!points.length) {
      throw new Error('点云文件中没有可用点数据')
    }
    detailPointCloudPoints.value = points
  } catch (error) {
    detailPointCloudPoints.value = []
    pointCloudLoadError.value = error?.message || '点云文件加载失败'
  } finally {
    pointCloudLoading.value = false
  }
}

function setActivePointFile(item) {
  activePointFile.value = item || null
}

watch(
  () => props.record,
  (value) => {
    activeVideo.value = value?.videos?.[0]?.fileUrl || value?.videoUrl || ''
    const firstImage = value?.images?.[0]?.fileUrl || value?.anomalies?.[0]?.imageUrl || ''
    activeImage.value = firstImage
    activePointFile.value = value?.points?.[0] || null
    detailPointCloudPoints.value = []
    pointCloudLoadError.value = ''
    pointCloudLoading.value = false
  },
  { immediate: true }
)

watch(
  () => activePointFile.value,
  (value) => {
    detailPointCloudPoints.value = []
    pointCloudLoadError.value = ''
    if (!value) {
      pointCloudLoading.value = false
      return
    }
    loadPointCloud(value)
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

.result-anomaly {
  color: #ff5a5f;
}

.video-player {
  width: 100%;
  min-height: 260px;
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
  gap: 4px;
  padding: 12px;
}

.preview-image {
  width: 100%;
  max-height: 420px;
  object-fit: contain;
  background: #07111d;
}

.asset-list {
  margin-top: 10px;
  display: grid;
  gap: 8px;
}

.asset-item,
.asset-link {
  min-height: 34px;
  padding: 8px 10px;
  border-radius: 8px;
  border: 1px solid rgba(103, 212, 255, 0.2);
  background: rgba(12, 24, 38, 0.8);
  color: var(--text-primary);
  text-align: left;
  text-decoration: none;
}

.asset-item {
  cursor: pointer;
}

.asset-item.active {
  border-color: rgba(117, 240, 194, 0.48);
  box-shadow: inset 0 0 0 1px rgba(117, 240, 194, 0.18);
}

.point-content {
  display: grid;
  gap: 12px;
}

.point-scene-wrap {
  min-height: 340px;
}

.point-placeholder {
  min-height: 340px;
  display: grid;
  place-items: center;
  color: var(--text-dim);
  border: 1px dashed rgba(103, 212, 255, 0.16);
  background: rgba(12, 24, 38, 0.46);
}

@media (max-width: 900px) {
  .detail-grid {
    grid-template-columns: 1fr;
  }
}
</style>
