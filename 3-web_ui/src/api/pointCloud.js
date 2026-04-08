import request from '../utils/request'

export function fetchPipeDatasetPointCloud(stopId) {
  return request.get(`/files/pipe-dataset/${stopId}/cloud_accum.pcd`, {
    responseType: 'arraybuffer'
  }).then((response) => response.data)
}
