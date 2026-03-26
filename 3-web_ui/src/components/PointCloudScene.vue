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
let pointCloud
let resizeObserver
let gridHelper
let axesHelper

function getContainerSize() {
  const width = container.value?.clientWidth || 0
  const height = container.value?.clientHeight || 0
  return { width, height }
}

function createScene() {
  scene = new THREE.Scene()
  scene.background = null

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

  gridHelper = new THREE.GridHelper(40, 20, 0x2e9fd8, 0x18496f)
  gridHelper.position.y = -10
  scene.add(gridHelper)

  axesHelper = new THREE.AxesHelper(10)
  scene.add(axesHelper)

  buildPoints()
}

function buildPoints() {
  const geometry = new THREE.BufferGeometry()
  const positions = []
  const colors = []

  props.points.forEach((point) => {
    positions.push(point.x, point.y, point.z)
    const ratio = Math.min(1, Math.abs(point.z) / 8)
    colors.push(0.12 + ratio * 0.45, 0.62 + ratio * 0.24, 0.24 + ratio * 0.32)
  })

  geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3))
  geometry.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3))
  geometry.computeBoundingBox()
  geometry.computeBoundingSphere()

  const material = new THREE.PointsMaterial({
    size: 0.34,
    sizeAttenuation: true,
    vertexColors: true
  })

  if (pointCloud) {
    scene.remove(pointCloud)
    pointCloud.geometry.dispose()
    pointCloud.material.dispose()
  }

  pointCloud = new THREE.Points(geometry, material)
  scene.add(pointCloud)
  framePointCloud()
}

function framePointCloud() {
  if (!pointCloud?.geometry?.boundingBox || !camera || !controls) {
    return
  }

  const box = pointCloud.geometry.boundingBox
  const center = new THREE.Vector3()
  const size = new THREE.Vector3()
  box.getCenter(center)
  box.getSize(size)

  const radius = Math.max(size.length() * 0.6, 8)
  controls.target.copy(center)
  camera.position.set(center.x + radius, center.y + radius * 0.75, center.z + radius)
  camera.near = 0.1
  camera.far = Math.max(radius * 20, 200)
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

function initScene() {
  if (renderer || !container.value) {
    return
  }

  const { width, height } = getContainerSize()
  if (!width || !height) {
    return
  }

  createScene()
  updateRendererSize()
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
  resizeObserver?.disconnect()
  controls?.dispose()
  if (pointCloud) {
    pointCloud.geometry.dispose()
    pointCloud.material.dispose()
  }
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
  gridHelper = null
  axesHelper = null
}

watch(
  () => props.points,
  () => {
    if (scene) {
      buildPoints()
    }
  },
  { deep: true }
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
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  overflow: hidden;
  background:
    radial-gradient(circle at 20% 20%, rgba(79, 171, 222, 0.12), transparent 28%),
    linear-gradient(180deg, rgba(7, 18, 29, 0.7), rgba(4, 10, 20, 0.92));
}
</style>
