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
  },
  mode: {
    type: String,
    default: 'raw'
  },
  fittedData: {
    type: Object,
    default: null
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
let fitGroup
let animatedDefects = []
let hasFramedScene = false

const PIPE_LENGTH_METERS = 6
const PIPE_RADIUS_METERS = 0.5
const FITTED_COLOR_MAP = Object.freeze({
  PL: 0xff8d57,
  BX: 0xff5f7a,
  ZAW: 0xffd166,
  SG: 0x9c89b8
})
const FITTED_WALL_THICKNESS = 0.026
const LABEL_SCALE = 0.3
const Z_AXIS_ROTATION_OFFSET = Math.PI

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

function getFittedColor(className) {
  return FITTED_COLOR_MAP[String(className || '').trim()] || 0x5ce1e6
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

function clearObject3D(object) {
  if (!object) {
    return
  }

  while (object.children?.length) {
    const child = object.children[0]
    clearObject3D(child)
    object.remove(child)
  }

  if (object.geometry) {
    object.geometry.dispose()
  }

  if (object.material) {
    const materials = Array.isArray(object.material)
      ? object.material
      : [object.material]
    materials.forEach((material) => material?.dispose?.())
  }
}

function resetAnimatedDefects() {
  animatedDefects = []
}

function resetFraming() {
  hasFramedScene = false
}

function updateRawPoints() {
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

  pointCloud.visible = true
  positionAttribute.needsUpdate = true
  colorAttribute.needsUpdate = true
  pointGeometry.setDrawRange(0, pointCount)
  pointGeometry.computeBoundingBox()
  pointGeometry.computeBoundingSphere()

  if (!hasFramedScene && pointCount > 0) {
    frameRawPointCloud()
    hasFramedScene = true
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

  const rimLight = new THREE.DirectionalLight(0xf9fbff, 0.55)
  rimLight.position.set(-10, 6, -14)
  scene.add(rimLight)

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

  fitGroup = new THREE.Group()
  fitGroup.visible = false
  scene.add(fitGroup)

  updateSceneContent()
}

function frameRawPointCloud() {
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

function frameFittedScene(placement) {
  if (!placement || !camera || !controls) {
    return
  }

  const radius = Math.max(placement.length * 1.45, placement.radius * 12, PIPE_LENGTH_METERS * 0.8)
  controls.target.copy(placement.center)
  camera.position.set(
    placement.center.x + radius * 0.55,
    placement.center.y + radius * 0.62,
    placement.center.z + radius * 1.15
  )
  camera.near = 0.05
  camera.far = Math.max(radius * 24, 120)
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
  const elapsed = performance.now() * 0.001
  animatedDefects.forEach((item, index) => {
    const phase = elapsed * 2.15 + index * 0.85
    const pulse = 0.5 + 0.5 * Math.sin(phase)
    if (item.meshMaterial?.emissiveIntensity !== undefined) {
      item.meshMaterial.emissiveIntensity = item.baseEmissive + pulse * item.emissiveRange
    }
    if (item.haloMaterial) {
      item.haloMaterial.opacity = item.baseHaloOpacity + pulse * item.haloRange
    }
    if (item.mesh) {
      const scale = item.baseScale + pulse * item.scaleRange
      item.mesh.scale.setScalar(scale)
    }
    if (item.regionMaterial) {
      item.regionMaterial.opacity = item.baseRegionOpacity + pulse * item.regionOpacityRange
    }
    if (item.labelSprite?.material) {
      item.labelSprite.material.opacity = item.baseLabelOpacity + pulse * item.labelOpacityRange
    }
  })
  controls?.update()
  renderer?.render(scene, camera)
}

function scheduleInitRetry() {
  cancelAnimationFrame(initRetryId)
  initRetryId = requestAnimationFrame(() => {
    initScene()
  })
}

function normalizePlacementValue(value, fallback) {
  const number = Number(value)
  return Number.isFinite(number) ? number : fallback
}

function createFittedPlacement(data) {
  const axis = new THREE.Vector3(0, 0, 1)

  const radius = Math.max(normalizePlacementValue(data?.R_global_m, PIPE_RADIUS_METERS), 0.05)
  const zRange = Array.isArray(data?.z_range_m) ? data.z_range_m : [0, normalizePlacementValue(data?.pipe_length_m, 4)]
  const zMin = normalizePlacementValue(zRange[0], 0)
  const zMax = normalizePlacementValue(zRange[1], zMin + normalizePlacementValue(data?.pipe_length_m, 4))
  const length = Math.max(zMax - zMin, 0.1)
  const start = new THREE.Vector3(0, 0, zMin)
  const end = new THREE.Vector3(0, 0, zMax)
  const center = start.clone().lerp(end, 0.5)
  const basisU = new THREE.Vector3(1, 0, 0)
  const basisV = new THREE.Vector3(0, 1, 0)
  const quaternion = new THREE.Quaternion()

  return {
    axis,
    radius,
    length,
    zMin,
    zMax,
    start,
    end,
    center,
    basisU,
    basisV,
    quaternion
  }
}

function createPointOnPipe(placement, zValue, thetaValue, radiusScale = 1.01) {
  return placement.start.clone()
    .addScaledVector(placement.axis, zValue - placement.zMin)
    .addScaledVector(placement.basisU, Math.cos(thetaValue) * placement.radius * radiusScale)
    .addScaledVector(placement.basisV, Math.sin(thetaValue) * placement.radius * radiusScale)
}

function createTextSprite(text, options = {}) {
  const fontSize = options.fontSize || 44
  const paddingX = options.paddingX || 18
  const paddingY = options.paddingY || 12
  const canvas = document.createElement('canvas')
  const context = canvas.getContext('2d')
  const font = `700 ${fontSize}px sans-serif`
  context.font = font
  const metrics = context.measureText(String(text))
  const width = Math.ceil(metrics.width + paddingX * 2)
  const height = Math.ceil(fontSize + paddingY * 2)
  canvas.width = width
  canvas.height = height

  context.font = font
  context.textAlign = 'center'
  context.textBaseline = 'middle'
  if (options.background) {
    context.fillStyle = options.background
    context.beginPath()
    const radius = Math.min(width, height) * 0.18
    context.moveTo(radius, 0)
    context.lineTo(width - radius, 0)
    context.quadraticCurveTo(width, 0, width, radius)
    context.lineTo(width, height - radius)
    context.quadraticCurveTo(width, height, width - radius, height)
    context.lineTo(radius, height)
    context.quadraticCurveTo(0, height, 0, height - radius)
    context.lineTo(0, radius)
    context.quadraticCurveTo(0, 0, radius, 0)
    context.closePath()
    context.fill()
  }
  context.fillStyle = options.color || '#F7FFFB'
  context.fillText(String(text), width / 2, height / 2)

  const texture = new THREE.CanvasTexture(canvas)
  texture.colorSpace = THREE.SRGBColorSpace
  const material = new THREE.SpriteMaterial({
    map: texture,
    transparent: true,
    depthWrite: false,
    depthTest: false
  })
  const sprite = new THREE.Sprite(material)
  const aspectRatio = width / height
  sprite.scale.set(LABEL_SCALE * aspectRatio, LABEL_SCALE, 1)
  sprite.renderOrder = options.renderOrder ?? 16
  return sprite
}

function addAxisMeasurementLabels(placement) {
  const startLabel = createTextSprite(`${placement.zMin.toFixed(1)}m`, {
    color: '#DFF7FF',
    background: 'rgba(8, 26, 41, 0.82)',
    renderOrder: 18
  })
  startLabel.position.copy(
    placement.start.clone()
      .addScaledVector(placement.axis, -0.03)
      .addScaledVector(placement.basisU, placement.radius * -0.18)
      .addScaledVector(placement.basisV, placement.radius * -0.1)
  )
  fitGroup.add(startLabel)

  const endLabel = createTextSprite(`${placement.zMax.toFixed(1)}m`, {
    color: '#DFF7FF',
    background: 'rgba(8, 26, 41, 0.82)',
    renderOrder: 18
  })
  endLabel.position.copy(
    placement.end.clone()
      .addScaledVector(placement.axis, 0.03)
      .addScaledVector(placement.basisU, placement.radius * -0.18)
      .addScaledVector(placement.basisV, placement.radius * -0.1)
  )
  fitGroup.add(endLabel)
}

function getFittedLabelOffsetScale(placement) {
  return Math.max(0.2, 1 - FITTED_WALL_THICKNESS / placement.radius) + 0.6
}

function getDiameterLabelPosition(placement, isStart) {
  const circumferenceTheta = Math.atan2(-0.1, -0.18)
  const zValue = isStart ? placement.zMin : placement.zMax
  const axisOffset = isStart ? -0.03 : 0.03
  return createPointOnPipe(
    placement,
    zValue,
    circumferenceTheta,
    getFittedLabelOffsetScale(placement)
  ).addScaledVector(placement.axis, axisOffset)
}

function addDiameterLabels(placement) {
  const diameterText = `${(placement.radius * 2).toFixed(1)}m`
  const labelOptions = {
    color: '#DFF7FF',
    background: 'rgba(8, 26, 41, 0.82)',
    renderOrder: 18
  }

  const startLabel = createTextSprite(diameterText, labelOptions)
  startLabel.position.copy(getDiameterLabelPosition(placement, true))
  fitGroup.add(startLabel)

  const endLabel = createTextSprite(diameterText, labelOptions)
  endLabel.position.copy(getDiameterLabelPosition(placement, false))
  fitGroup.add(endLabel)
}

function createDefectPatchGeometry(placement, defect) {
  const thetaCenter = normalizePlacementValue(defect?.theta_center, normalizePlacementValue(defect?.theta_rad, 0))
    + Z_AXIS_ROTATION_OFFSET
  const thetaMinOffset = normalizePlacementValue(defect?.theta_min_offset, -0.14)
  const thetaMaxOffset = normalizePlacementValue(defect?.theta_max_offset, 0.14)
  const thetaMin = thetaCenter + Math.min(thetaMinOffset, thetaMaxOffset)
  const thetaMax = thetaCenter + Math.max(thetaMinOffset, thetaMaxOffset)
  const zMin = normalizePlacementValue(defect?.z_min, normalizePlacementValue(defect?.z_m, placement.zMin))
  const zMax = normalizePlacementValue(defect?.z_max, normalizePlacementValue(defect?.z_m, placement.zMin)) || zMin + 0.1
  const safeZMax = Math.max(zMax, zMin + 0.05)
  const thetaSegments = Math.max(10, Math.ceil(Math.abs(thetaMax - thetaMin) / (Math.PI / 30)))
  const zSegments = Math.max(2, Math.ceil((safeZMax - zMin) / 0.06))
  const positions = []
  const indices = []

  for (let zIndex = 0; zIndex <= zSegments; zIndex += 1) {
    const zRatio = zIndex / zSegments
    const zValue = zMin + (safeZMax - zMin) * zRatio

    for (let thetaIndex = 0; thetaIndex <= thetaSegments; thetaIndex += 1) {
      const thetaRatio = thetaIndex / thetaSegments
      const thetaValue = thetaMin + (thetaMax - thetaMin) * thetaRatio
      const point = createPointOnPipe(
        placement,
        zValue,
        thetaValue,
        Math.max(0.2, 1 - FITTED_WALL_THICKNESS / placement.radius - 0.002)
      )
      positions.push(point.x, point.y, point.z)
    }
  }

  const rowSize = thetaSegments + 1
  for (let zIndex = 0; zIndex < zSegments; zIndex += 1) {
    for (let thetaIndex = 0; thetaIndex < thetaSegments; thetaIndex += 1) {
      const topLeft = zIndex * rowSize + thetaIndex
      const topRight = topLeft + 1
      const bottomLeft = topLeft + rowSize
      const bottomRight = bottomLeft + 1

      indices.push(topLeft, bottomLeft, topRight)
      indices.push(topRight, bottomLeft, bottomRight)
    }
  }

  const geometry = new THREE.BufferGeometry()
  geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3))
  geometry.setIndex(indices)
  geometry.computeVertexNormals()
  return geometry
}

function getDefectLabelPosition(placement, defect) {
  if (defect?.region_type) {
    const thetaCenter = normalizePlacementValue(defect?.theta_center, normalizePlacementValue(defect?.theta_rad, 0))
      + Z_AXIS_ROTATION_OFFSET
    const zValue = normalizePlacementValue(defect?.z_m, placement.zMin + placement.length / 2)
    return createPointOnPipe(
      placement,
      zValue,
      thetaCenter,
      getFittedLabelOffsetScale(placement) + 0.1
    )
  }

  const theta = normalizePlacementValue(defect?.theta_rad, 0) + Z_AXIS_ROTATION_OFFSET
  const zValue = normalizePlacementValue(defect?.z_m, placement.zMin + placement.length / 2)
  return createPointOnPipe(
    placement,
    zValue,
    theta,
    getFittedLabelOffsetScale(placement)
  )
}

function renderFittedScene() {
  if (!fitGroup) {
    return null
  }

  clearObject3D(fitGroup)
  resetAnimatedDefects()
  fitGroup.visible = true
  pointCloud.visible = false
  pointGeometry.setDrawRange(0, 0)

  if (!props.fittedData) {
    return null
  }

  const placement = createFittedPlacement(props.fittedData)
  const shellGeometry = new THREE.CylinderGeometry(
    placement.radius,
    placement.radius,
    placement.length,
    72,
    1,
    true
  )
  shellGeometry.rotateX(Math.PI / 2)
  const shellMaterial = new THREE.MeshPhysicalMaterial({
    color: 0x59c4df,
    transparent: true,
    opacity: 0.12,
    side: THREE.DoubleSide,
    roughness: 0.36,
    metalness: 0.08,
    clearcoat: 0.36,
    transmission: 0.18
  })
  const shell = new THREE.Mesh(shellGeometry, shellMaterial)
  shell.position.copy(placement.center)
  shell.quaternion.copy(placement.quaternion)
  shell.renderOrder = 1
  fitGroup.add(shell)

  const shellEdges = new THREE.LineSegments(
    new THREE.EdgesGeometry(shellGeometry, 20),
    new THREE.LineBasicMaterial({
      color: 0x7fe8ff,
      transparent: true,
      opacity: 0.38
    })
  )
  shellEdges.position.copy(placement.center)
  shellEdges.quaternion.copy(placement.quaternion)
  shellEdges.renderOrder = 2
  fitGroup.add(shellEdges)

  const axisLine = new THREE.Line(
    new THREE.BufferGeometry().setFromPoints([placement.start, placement.end]),
    new THREE.LineDashedMaterial({
      color: 0xa8d6ff,
      dashSize: 0.12,
      gapSize: 0.07,
      transparent: true,
      opacity: 0.88,
      depthTest: false,
      depthWrite: false
    })
  )
  axisLine.computeLineDistances()
  axisLine.renderOrder = 16
  fitGroup.add(axisLine)
  addAxisMeasurementLabels(placement)
  addDiameterLabels(placement)

  const defects = Array.isArray(props.fittedData?.defects) ? props.fittedData.defects : []
  defects.forEach((defect) => {
    const color = getFittedColor(defect?.class_name)
    const labelPosition = getDefectLabelPosition(placement, defect)
    const typeLabel = createTextSprite(String(defect?.class_name || '').toUpperCase(), {
      color: '#FFFFFF',
      background: 'rgba(10, 20, 34, 0.84)',
      renderOrder: 18
    })
    typeLabel.position.copy(labelPosition)
    fitGroup.add(typeLabel)

    if (defect?.region_type) {
      const regionMaterial = new THREE.MeshBasicMaterial({
        color,
        transparent: true,
        opacity: 0.54,
        side: THREE.DoubleSide,
        depthWrite: false,
        depthTest: false
      })
      const regionMesh = new THREE.Mesh(
        createDefectPatchGeometry(placement, defect),
        regionMaterial
      )
      regionMesh.renderOrder = 12
      fitGroup.add(regionMesh)
      animatedDefects.push({
        regionMaterial,
        baseRegionOpacity: 0.46,
        regionOpacityRange: 0.22,
        labelSprite: typeLabel,
        baseLabelOpacity: 0.74,
        labelOpacityRange: 0.16
      })
      return
    }

    const theta = normalizePlacementValue(defect?.theta_rad, 0) + Z_AXIS_ROTATION_OFFSET
    const zValue = normalizePlacementValue(defect?.z_m, placement.zMin + placement.length / 2)
    const markerPosition = createPointOnPipe(
      placement,
      zValue,
      theta,
      Math.max(0.2, 1 - FITTED_WALL_THICKNESS / placement.radius - 0.004)
    )
    const markerMaterial = new THREE.MeshStandardMaterial({
      color,
      emissive: color,
      emissiveIntensity: 0.72,
      roughness: 0.22,
      metalness: 0.05,
      depthWrite: false,
      depthTest: false
    })
    const marker = new THREE.Mesh(
      new THREE.SphereGeometry(0.05, 18, 18),
      markerMaterial
    )
    marker.position.copy(markerPosition)
    marker.renderOrder = 13
    fitGroup.add(marker)

    const haloMaterial = new THREE.MeshBasicMaterial({
      color,
      transparent: true,
      opacity: 0.18,
      depthWrite: false,
      depthTest: false
    })
    const halo = new THREE.Mesh(
      new THREE.SphereGeometry(0.082, 18, 18),
      haloMaterial
    )
    halo.position.copy(markerPosition)
    halo.renderOrder = 14
    fitGroup.add(halo)
    animatedDefects.push({
      mesh: marker,
      meshMaterial: markerMaterial,
      haloMaterial,
      labelSprite: typeLabel,
      baseEmissive: 0.62,
      emissiveRange: 15.0,
      baseHaloOpacity: 0.12,
      haloRange: 0.24,
      baseScale: 1,
      scaleRange: 0.2,
      baseLabelOpacity: 0.82,
      labelOpacityRange: 0.12
    })
  })

  if (!hasFramedScene) {
    frameFittedScene(placement)
    hasFramedScene = true
  }

  return placement
}

function updateSceneContent() {
  if (!scene || !pointCloud || !fitGroup) {
    return
  }

  const fittedMode = props.mode === 'fitted'
  gridHelper.visible = true
  axesHelper.visible = true
  gridHelper.position.y = fittedMode ? -0.95 : -1.0
  axesHelper.scale.setScalar(fittedMode ? 1.15 : 1.0)

  if (fittedMode) {
    renderFittedScene()
    return
  }

  clearObject3D(fitGroup)
  fitGroup.visible = false
  updateRawPoints()
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
  clearObject3D(fitGroup)
  resetAnimatedDefects()
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
  fitGroup = null
  hasFramedScene = false
}

watch(
  () => props.points,
  () => {
    if (props.mode === 'raw') {
      updateSceneContent()
    }
  }
)

watch(
  () => props.mode,
  () => {
    resetFraming()
    updateSceneContent()
  }
)

watch(
  () => props.fittedData,
  () => {
    if (props.mode === 'fitted') {
      resetFraming()
      updateSceneContent()
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
