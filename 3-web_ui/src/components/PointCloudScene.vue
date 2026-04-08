<template>
  <div ref="container" class="point-cloud-scene"></div>
</template>

<script setup>
import { onBeforeUnmount, onMounted, ref, watch } from 'vue'
import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js'

const props = defineProps({
  points: {
    type: Array,
    default: () => []
  }
})

const container = ref(null)
let renderer
let scene
let camera
let controls
let frameId
let initRetryId
let resizeObserver
let pointCloud
let pointGeometry
let pointMaterial
let gridHelper
let axesHelper
let hasFramedPointCloud = false

const PIPE_LENGTH_METERS = 6
const PIPE_RADIUS_METERS = 0.5

function getContainerSize() {
  const width = container.value?.clientWidth || 0
  const height = container.value?.clientHeight || 0
  return { width, height }
}

function getColorComponents(zValue) {
  const ratio = Math.min(1, Math.abs(zValue) / (PIPE_LENGTH_METERS / 2))
  return [
    0.12 + ratio * 0.45,
    0.62 + ratio * 0.24,
    0.24 + ratio * 0.32
  ]
}

function ensurePointAttributes(pointCount) {
  if (!pointGeometry) {
    return
  }

  const requiredLength = Math.max(pointCount * 3, 3)
  const positionAttribute = pointGeometry.getAttribute('position')
  const colorAttribute = pointGeometry.getAttribute('color')

  if (!positionAttribute || positionAttribute.array.length < requiredLength) {
    pointGeometry.setAttribute('position', new THREE.Float32BufferAttribute(new Float32Array(requiredLength), 3))
  }
  if (!colorAttribute || colorAttribute.array.length < requiredLength) {
    pointGeometry.setAttribute('color', new THREE.Float32BufferAttribute(new Float32Array(requiredLength), 3))
  }
}

function updatePoints() {
  if (!pointGeometry) {
    return
  }

  const pointCount = props.points.length
  ensurePointAttributes(pointCount)

  const positionAttribute = pointGeometry.getAttribute('position')
  const colorAttribute = pointGeometry.getAttribute('color')
  const positions = positionAttribute.array
  const colors = colorAttribute.array

  for (let index = 0; index < pointCount; index += 1) {
    const offset = index * 3
    const point = props.points[index]

    positions[offset] = point.x
    positions[offset + 1] = point.y
    positions[offset + 2] = point.z

    const [red, green, blue] = getColorComponents(point.z)
    colors[offset] = red
    colors[offset + 1] = green
    colors[offset + 2] = blue
  }

  positionAttribute.needsUpdate = true
  colorAttribute.needsUpdate = true
  pointGeometry.setDrawRange(0, pointCount)
  pointGeometry.computeBoundingBox()
  pointGeometry.computeBoundingSphere()

  if (!hasFramedPointCloud && pointCount > 0) {
    framePointCloud()
    hasFramedPointCloud = true
  }
}

function createScene() {
  scene = new THREE.Scene()

  camera = new THREE.PerspectiveCamera(42, 1, 0.1, 400)
  camera.position.set(18, 14, 26)

  renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true })
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2))
  renderer.outputColorSpace = THREE.SRGBColorSpace
  renderer.domElement.style.display = 'block'
  renderer.domElement.style.width = '100%'
  renderer.domElement.style.height = '100%'
  container.value.appendChild(renderer.domElement)

  controls = new OrbitControls(camera, renderer.domElement)
  controls.enableDamping = true
  controls.dampingFactor = 0.08
  controls.enablePan = false
  controls.minDistance = 4
  controls.maxDistance = 160

  scene.add(new THREE.AmbientLight(0xffffff, 1.1))

  const keyLight = new THREE.DirectionalLight(0x8fdcff, 1.2)
  keyLight.position.set(12, 16, 10)
  scene.add(keyLight)

  gridHelper = new THREE.GridHelper(5, 20, 0x2e9fd8, 0x18496f)
  gridHelper.position.y = -1.0
  scene.add(gridHelper)

  axesHelper = new THREE.AxesHelper(1.0)
  scene.add(axesHelper)

  pointGeometry = new THREE.BufferGeometry()
  pointMaterial = new THREE.PointsMaterial({
    size: 0.02,
    sizeAttenuation: true,
    vertexColors: true
  })
  pointCloud = new THREE.Points(pointGeometry, pointMaterial)
  scene.add(pointCloud)

  updatePoints()
}

function framePointCloud() {
  if (!pointGeometry?.boundingBox || !camera || !controls) {
    return
  }

  const center = new THREE.Vector3()
  const size = new THREE.Vector3()
  pointGeometry.boundingBox.getCenter(center)
  pointGeometry.boundingBox.getSize(size)

  const radius = Math.max(size.length() * 0.72, PIPE_LENGTH_METERS * 0.75, PIPE_RADIUS_METERS * 6)
  controls.target.copy(center)
  camera.position.set(center.x + radius * 0.52, center.y + radius * 0.56, center.z + radius * 1.1)
  camera.near = 0.1
  camera.far = Math.max(radius * 18, 120)
  camera.updateProjectionMatrix()
  controls.update()
}

function updateRendererSize() {
  if (!container.value || !renderer || !camera) {
    return false
  }

  const { width, height } = getContainerSize()
  if (!width || !height) {
    return false
  }

  camera.aspect = width / height
  camera.updateProjectionMatrix()
  renderer.setSize(width, height, false)
  return true
}

function animate() {
  frameId = requestAnimationFrame(animate)
  controls?.update()
  renderer?.render(scene, camera)
}

function scheduleInitRetry() {
  cancelAnimationFrame(initRetryId)
  initRetryId = requestAnimationFrame(() => {
    initScene()
  })
}

function initScene() {
  if (renderer || !container.value) {
    return
  }

  createScene()
  if (!updateRendererSize()) {
    renderer.setSize(1, 1, false)
    scheduleInitRetry()
  }
  animate()
}

function handleResize() {
  if (!renderer) {
    initScene()
    return
  }
  updateRendererSize()
}

function disposeScene() {
  cancelAnimationFrame(frameId)
  cancelAnimationFrame(initRetryId)
  resizeObserver?.disconnect()
  controls?.dispose()
  pointGeometry?.dispose()
  pointMaterial?.dispose()
  renderer?.dispose()
  scene?.clear()
  if (renderer?.domElement?.parentNode) {
    renderer.domElement.parentNode.removeChild(renderer.domElement)
  }
  renderer = null
  scene = null
  camera = null
  controls = null
  pointCloud = null
  pointGeometry = null
  pointMaterial = null
  gridHelper = null
  axesHelper = null
  hasFramedPointCloud = false
}

watch(
  () => props.points,
  () => {
    if (scene) {
      updatePoints()
    }
  }
)

onMounted(() => {
  initScene()
  resizeObserver = new ResizeObserver(() => {
    handleResize()
  })
  if (container.value) {
    resizeObserver.observe(container.value)
  }
  window.addEventListener('resize', handleResize)
})

onBeforeUnmount(() => {
  window.removeEventListener('resize', handleResize)
  disposeScene()
})
</script>

<style scoped>
.point-cloud-scene {
  position: relative;
  width: 100%;
  height: 100%;
  min-height: 320px;
  overflow: hidden;
  background:
    radial-gradient(circle at 20% 20%, rgba(79, 171, 222, 0.12), transparent 28%),
    linear-gradient(180deg, rgba(7, 18, 29, 0.7), rgba(4, 10, 20, 0.92));
}
</style>
