<template>
  <div ref="container" class="three-canvas"></div>
</template>

<script setup>
import { onBeforeUnmount, onMounted, ref } from 'vue'
import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js'

const container = ref(null)
let renderer
let scene
let camera
let controls
let frameId
let resizeObserver
let pipeGroup

function createInspectionCabin() {
  const group = new THREE.Group()

  const shell = new THREE.Mesh(
    new THREE.CylinderGeometry(1.05, 1.05, 3.3, 32, 1, true),
    new THREE.MeshPhysicalMaterial({
      color: 0xe9f3f5,
      transparent: true,
      opacity: 0.92,
      roughness: 0.16,
      transmission: 0.05
    })
  )
  group.add(shell)

  const capMaterial = new THREE.MeshStandardMaterial({ color: 0x6edce4, metalness: 0.75, roughness: 0.25 })
  const top = new THREE.Mesh(new THREE.CylinderGeometry(1.2, 1.2, 0.18, 32), capMaterial)
  top.position.y = 1.72
  group.add(top)

  const bottom = top.clone()
  bottom.position.y = -1.72
  group.add(bottom)

  const core = new THREE.Mesh(
    new THREE.CylinderGeometry(0.72, 0.72, 2.9, 28),
    new THREE.MeshStandardMaterial({ color: 0xfafdfd, metalness: 0.15, roughness: 0.6 })
  )
  group.add(core)

  return group
}

function getContainerSize() {
  const width = container.value?.clientWidth || 0
  const height = container.value?.clientHeight || 0
  return { width, height }
}

function createScene() {
  scene = new THREE.Scene()
  scene.fog = new THREE.FogExp2(0x08111d, 0.035)

  camera = new THREE.PerspectiveCamera(35, 1, 0.1, 100)
  camera.position.set(0, 7.4, 18)

  renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true })
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2))
  renderer.outputColorSpace = THREE.SRGBColorSpace
  container.value.appendChild(renderer.domElement)

  controls = new OrbitControls(camera, renderer.domElement)
  controls.enablePan = false
  controls.enableDamping = true
  controls.dampingFactor = 0.08
  controls.minDistance = 8
  controls.maxDistance = 30
  controls.minPolarAngle = 0.45
  controls.maxPolarAngle = 2.25
  controls.target.set(0, 1.2, 0)

  scene.add(new THREE.AmbientLight(0x89d6ff, 1.8))

  const side = new THREE.PointLight(0x64f5d6, 24, 80)
  side.position.set(6, 8, 10)
  scene.add(side)

  const fill = new THREE.PointLight(0x1659c0, 18, 80)
  fill.position.set(-8, 6, -8)
  scene.add(fill)

  pipeGroup = new THREE.Group()

  const material = new THREE.MeshPhysicalMaterial({
    color: 0x46dce6,
    transparent: true,
    opacity: 0.85,
    roughness: 0.2,
    transmission: 0.16
  })

  const mainPipe = new THREE.Mesh(new THREE.CylinderGeometry(2.25, 2.25, 13, 44, 1, true), material)
  mainPipe.rotation.z = Math.PI / 2
  pipeGroup.add(mainPipe)

  const ringMaterial = new THREE.MeshStandardMaterial({ color: 0x33bec9, metalness: 0.8, roughness: 0.28 })
  const leftRing = new THREE.Mesh(new THREE.TorusGeometry(2.25, 0.12, 16, 64), ringMaterial)
  leftRing.position.x = -6.5
  leftRing.rotation.y = Math.PI / 2
  pipeGroup.add(leftRing)

  const rightRing = leftRing.clone()
  rightRing.position.x = 6.5
  pipeGroup.add(rightRing)

  const leftCabinPipe = new THREE.Mesh(new THREE.CylinderGeometry(1.5, 1.5, 4, 36, 1, true), material)
  leftCabinPipe.position.set(-4.4, 2.7, 0)
  pipeGroup.add(leftCabinPipe)

  const rightCabinPipe = leftCabinPipe.clone()
  rightCabinPipe.position.x = 4.4
  pipeGroup.add(rightCabinPipe)

  const leftCabin = createInspectionCabin()
  leftCabin.position.set(-4.4, 5.8, 0)
  pipeGroup.add(leftCabin)

  const rightCabin = createInspectionCabin()
  rightCabin.position.set(4.4, 5.8, 0)
  pipeGroup.add(rightCabin)

  const robot = new THREE.Mesh(
    new THREE.BoxGeometry(0.7, 0.5, 0.8),
    new THREE.MeshStandardMaterial({ color: 0x84d8ff, emissive: 0x0d7fa2, emissiveIntensity: 0.65 })
  )
  robot.position.set(0, -0.6, 0)
  pipeGroup.add(robot)

  scene.add(pipeGroup)

  const ground = new THREE.Mesh(
    new THREE.CircleGeometry(10, 64),
    new THREE.MeshBasicMaterial({
      color: 0x2ee4d4,
      transparent: true,
      opacity: 0.08
    })
  )
  ground.rotation.x = -Math.PI / 2
  ground.position.y = -3.6
  scene.add(ground)
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
  renderer?.dispose()
  scene?.traverse((object) => {
    if (object.geometry) {
      object.geometry.dispose()
    }
    if (object.material) {
      const materials = Array.isArray(object.material) ? object.material : [object.material]
      materials.forEach((material) => material?.dispose?.())
    }
  })
  scene?.clear()
  if (renderer?.domElement?.parentNode) {
    renderer.domElement.parentNode.removeChild(renderer.domElement)
  }
  renderer = null
  scene = null
  camera = null
  controls = null
  pipeGroup = null
}

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
.three-canvas {
  width: 100%;
  height: 100%;
}
</style>
