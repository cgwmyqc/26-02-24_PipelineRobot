import { computed, onBeforeUnmount, onMounted, reactive, ref, watch } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { uploadFirmware } from '../api/ota'
import { appConfig } from '../config/app'
import { rosService } from '../services/ros'

const OTA_TARGETS = Object.freeze([
  {
    key: 'mobile_part',
    label: '移动端固件',
    commandTopic: appConfig.topics.mobileOtaCommand,
    statusTopic: appConfig.topics.mobileOtaStatus,
    progressTopic: appConfig.topics.mobileOtaProgress
  },
  {
    key: 'fixed_part',
    label: '固定端固件',
    commandTopic: appConfig.topics.fixedOtaCommand,
    statusTopic: appConfig.topics.fixedOtaStatus,
    progressTopic: appConfig.topics.fixedOtaProgress
  }
])

function createTargetState(target) {
  return reactive({
    key: target.key,
    label: target.label,
    file: null,
    fileName: '',
    uploadProgress: 0,
    deviceProgress: 0,
    statusPhase: 'idle',
    statusMessage: '待命',
    currentJobId: '',
    uploadedPayload: null,
    busy: false,
    error: ''
  })
}

function safeJsonParse(rawValue) {
  try {
    return JSON.parse(String(rawValue || ''))
  } catch (_error) {
    return null
  }
}

function isBusyPhase(phase) {
  return ['uploading', 'queued', 'downloading', 'validating', 'writing'].includes(String(phase || ''))
}

export function useFirmwareOta() {
  const dialogVisible = ref(false)
  const targets = reactive(
    OTA_TARGETS.reduce((map, target) => {
      map[target.key] = createTargetState(target)
      return map
    }, {})
  )
  const unsubscribers = []
  const anyLocked = computed(() => Object.values(targets).some((state) => isBusyPhase(state.statusPhase)))

  function beforeUnloadHandler(event) {
    if (!anyLocked.value) {
      return undefined
    }
    event.preventDefault()
    event.returnValue = '当前正在刷写或上传，离开页面不会帮你取消任务，可能看不到后续进度。'
    return event.returnValue
  }

  function resetUploadedPayload(targetKey) {
    targets[targetKey].uploadedPayload = null
    targets[targetKey].uploadProgress = 0
    targets[targetKey].deviceProgress = 0
    targets[targetKey].currentJobId = ''
    targets[targetKey].statusPhase = 'idle'
    targets[targetKey].statusMessage = '待命'
    targets[targetKey].error = ''
  }

  function openDialog() {
    dialogVisible.value = true
  }

  async function requestCloseDialog() {
    if (!anyLocked.value) {
      dialogVisible.value = false
      return true
    }

    try {
      await ElMessageBox.confirm(
        '设备可能仍在刷写中，关闭弹窗不会中止刷写。确认关闭吗？',
        '关闭提示',
        {
          type: 'warning',
          confirmButtonText: '确认关闭',
          cancelButtonText: '继续查看'
        }
      )
      dialogVisible.value = false
      return true
    } catch (_error) {
      return false
    }
  }

  function closeDialog() {
    dialogVisible.value = false
  }

  function selectFile(targetKey, file) {
    const state = targets[targetKey]
    if (isBusyPhase(state.statusPhase)) {
      ElMessage.warning(`${state.label} 正在刷写，请等待完成后再更换固件`)
      return
    }
    state.file = file || null
    state.fileName = file?.name || ''
    resetUploadedPayload(targetKey)
  }

  async function startUpload(targetKey) {
    const state = targets[targetKey]
    if (isBusyPhase(state.statusPhase)) {
      ElMessage.warning(`${state.label} 当前正在刷写，请勿重复操作`)
      return
    }
    if (!state.file) {
      ElMessage.warning(`请先选择 ${state.label} 的 .bin 文件`)
      return
    }

    state.busy = true
    state.error = ''
    state.statusPhase = 'uploading'
    state.statusMessage = '正在上传固件'
    state.uploadProgress = 0
    state.deviceProgress = 0

    try {
      const payload = await uploadFirmware({
        target: targetKey,
        file: state.file,
        onUploadProgress(event) {
          const total = Number(event?.total || 0)
          const loaded = Number(event?.loaded || 0)
          state.uploadProgress = total > 0 ? Math.min(100, Math.round((loaded / total) * 100)) : 0
        }
      })

      state.uploadedPayload = payload
      state.currentJobId = payload.jobId || ''
      state.statusPhase = 'queued'
      state.statusMessage = '上传完成，OTA 命令已发送'
      rosService.publish(OTA_TARGETS.find((item) => item.key === targetKey).commandTopic, {
        data: JSON.stringify({
          job_id: payload.jobId,
          url: payload.url,
          sha256: payload.sha256,
          size: payload.size,
          filename: payload.filename
        })
      })
      ElMessage.success(`${state.label} OTA 命令已发送`)
    } catch (error) {
      state.statusPhase = 'error'
      state.statusMessage = '上传失败'
      state.error = error?.response?.data?.message || error?.message || '上传失败'
      ElMessage.error(state.error)
    } finally {
      state.busy = false
    }
  }

  function bindRosSubscriptions() {
    OTA_TARGETS.forEach((target) => {
      unsubscribers.push(
        rosService.subscribe(target.statusTopic, (message) => {
          const parsed = safeJsonParse(message?.data)
          const state = targets[target.key]
          if (!parsed) {
            state.statusPhase = 'error'
            state.error = '收到无法解析的 OTA 状态消息'
            state.statusMessage = '状态消息格式错误'
            return
          }
          if (state.currentJobId && parsed.job_id && parsed.job_id !== state.currentJobId) {
            return
          }
          state.currentJobId = parsed.job_id || state.currentJobId
          state.statusPhase = parsed.phase || state.statusPhase
          state.statusMessage = parsed.message || state.statusMessage
          state.deviceProgress = Number.isFinite(Number(parsed.progress))
            ? Math.max(0, Math.min(100, Number(parsed.progress)))
            : state.deviceProgress
          state.error = parsed.phase === 'error' ? (parsed.message || '设备刷写失败') : ''
        }),
        rosService.subscribe(target.progressTopic, (message) => {
          const state = targets[target.key]
          const nextValue = Math.max(0, Math.min(100, Number(message?.data || 0)))
          state.deviceProgress = Number.isFinite(nextValue) ? nextValue : state.deviceProgress
        })
      )
    })
  }

  onMounted(() => {
    rosService.ensureConnection()
    bindRosSubscriptions()
    window.addEventListener('beforeunload', beforeUnloadHandler)
  })

  onBeforeUnmount(() => {
    window.removeEventListener('beforeunload', beforeUnloadHandler)
    unsubscribers.splice(0).forEach((unsubscribe) => unsubscribe?.())
  })

  const targetCards = computed(() => OTA_TARGETS.map((target) => ({
    ...target,
    state: targets[target.key],
    isLocked: isBusyPhase(targets[target.key].statusPhase),
    isUploading: targets[target.key].statusPhase === 'uploading',
    canStart: Boolean(targets[target.key].file) && !isBusyPhase(targets[target.key].statusPhase),
    startButtonText: isBusyPhase(targets[target.key].statusPhase) ? '刷写中' : '开始刷写'
  })))

  return {
    dialogVisible,
    targetCards,
    anyLocked,
    openDialog,
    closeDialog,
    requestCloseDialog,
    selectFile,
    startUpload
  }
}
