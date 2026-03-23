<template>
  <div ref="container" class="point-cloud-scene"></div>
</template>

<script setup>
import { onBeforeUnmount, onMounted, ref, watch } from 'vue'
import * as THREE from 'three'

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
let frameId
let pointCloud

function buildPoints() {
  const geometry = new THREE.BufferGeometry()
  const positions = []
  const colors = []

  props.points.forEach((point) => {
    positions.push(point.x, point.y, point.z)
    const ratio = Math.min(1, Math.abs(point.z) / 8)
    colors.push(0.1 + ratio * 0.4, 0.7 + ratio * 0.2, 0.2)
  })

  geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3))
  geometry.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3))

  const material = new THREE.PointsMaterial({
    size: 0.28,
    vertexColors: true
  })

  if (pointCloud) {
    scene.remove(pointCloud)
    pointCloud.geometry.dispose()
    pointCloud.material.dispose()
  }

  pointCloud = new THREE.Points(geometry, material)
  scene.add(pointCloud)
}

function initScene() {
  const width = container.value.clientWidth
  const height = container.value.clientHeight

  scene = new THREE.Scene()
  camera = new THREE.PerspectiveCamera(42, width / height, 0.1, 400)
  camera.position.set(0, 0, 92)

  renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true })
  renderer.setPixelRatio(window.devicePixelRatio)
  renderer.setSize(width, height)
  container.value.appendChild(renderer.domElement)

  scene.add(new THREE.AmbientLight(0xffffff, 1.2))
  buildPoints()
  animate()
}

function animate() {
  frameId = requestAnimationFrame(animate)
  if (pointCloud) {
    pointCloud.rotation.z += 0.0022
    pointCloud.rotation.x = 0.25
  }
  renderer.render(scene, camera)
}

function handleResize() {
  if (!container.value || !renderer || !camera) {
    return
  }
  const width = container.value.clientWidth
  const height = container.value.clientHeight
  camera.aspect = width / height
  camera.updateProjectionMatrix()
  renderer.setSize(width, height)
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
  window.addEventListener('resize', handleResize)
})

onBeforeUnmount(() => {
  cancelAnimationFrame(frameId)
  window.removeEventListener('resize', handleResize)
  renderer?.dispose()
  scene?.clear()
})
</script>

<style scoped>
.point-cloud-scene {
  width: 100%;
  height: 100%;
  background:
    linear-gradient(rgba(83, 145, 198, 0.08) 1px, transparent 1px),
    linear-gradient(90deg, rgba(83, 145, 198, 0.08) 1px, transparent 1px);
  background-size: 32px 32px;
}
</style>
