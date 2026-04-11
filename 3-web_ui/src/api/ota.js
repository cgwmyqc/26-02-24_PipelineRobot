import request from '../utils/request'

export async function uploadFirmware({ target, file, onUploadProgress } = {}) {
  const formData = new FormData()
  formData.append('target', String(target || '').trim())
  formData.append('file', file)

  const response = await request.post('/ota/firmware', formData, {
    headers: {
      'Content-Type': 'multipart/form-data'
    },
    onUploadProgress
  })

  return response.data
}
