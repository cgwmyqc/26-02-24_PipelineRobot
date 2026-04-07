<template>
  <div ref="container" class="three-canvas"></div>
</template>

<script setup>
import { onBeforeUnmount, onMounted, ref } from 'vue'
import * as THREE from 'three'
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js'
import piperobotModelUrl from '../assets/models/piperobot.glb?url'

const container = ref(null)
const loader = new GLTFLoader()
const FRESNEL_BASE_COLOR = new THREE.Color('#2dd6ff')
const FRESNEL_EDGE_COLOR = new THREE.Color('#b8f7ff')
const FRESNEL_BASE_OPACITY = 0.04
const FRESNEL_POWER = 2.8
const FRESNEL_EDGE_INTENSITY = 1.85
const MODEL_DISPLAY_SIZE = 4.8
const TARGET_HEIGHT_RATIO = 0.34
const CAMERA_X_OFFSET_FACTOR = 1.55
const CAMERA_Y_OFFSET_FACTOR = 0.82
const CAMERA_Z_OFFSET_FACTOR = 1.55

const fresnelVertexShader = `
  varying vec3 vWorldPosition;
  varying vec3 vWorldNormal;

  void main() {
    vec4 worldPosition = modelMatrix * vec4(position, 1.0);
    vWorldPosition = worldPosition.xyz;
    vWorldNormal = normalize(mat3(modelMatrix) * normal);
    gl_Position = projectionMatrix * viewMatrix * worldPosition;
  }
`

const fresnelFragmentShader = `
  uniform vec3 uBaseColor;
  uniform vec3 uEdgeColor;
  uniform float uBaseOpacity;
  uniform float uFresnelPower;
  uniform float uEdgeIntensity;
  uniform vec3 uCameraPosition;

  varying vec3 vWorldPosition;
  varying vec3 vWorldNormal;

  void main() {
    vec3 viewDir = normalize(uCameraPosition - vWorldPosition);
    float fresnel = pow(1.0 - max(dot(normalize(vWorldNormal), viewDir), 0.0), uFresnelPower);
    float edgeMix = clamp(fresnel * uEdgeIntensity, 0.0, 1.0);
    vec3 color = mix(uBaseColor, uEdgeColor, edgeMix);
    float alpha = clamp(uBaseOpacity + fresnel * 0.22, 0.0, 0.32);
    gl_FragColor = vec4(color, alpha);
  }
`

let renderer
let scene
let camera
let controls
let frameId
let resizeObserver
let rootModel
let ground
let axesHelper
let activeLoadToken = 0
const fresnelMaterials = []

function getContainerSize() {
  const width = container.value?.clientWidth || 0
  const height = container.value?.clientHeight || 0
  return { width, height }
}

function createScene() {
  scene = new THREE.Scene()
  scene.fog = new THREE.FogExp2(0x08111d, 0.035)

  camera = new THREE.PerspectiveCamera(35, 1, 0.1, 200)
  camera.position.set(0, 3.4, 10)


  renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true })
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2))
  renderer.outputColorSpace = THREE.SRGBColorSpace
  renderer.domElement.style.display = 'block'
  renderer.domElement.style.width = '100%'
  renderer.domElement.style.height = '100%'
  container.value.appendChild(renderer.domElement)

  controls = new OrbitControls(camera, renderer.domElement)
  controls.enablePan = false
  controls.enableDamping = true
  controls.dampingFactor = 0.08
  controls.minDistance = 2.5
  controls.maxDistance = 20
  controls.minPolarAngle = 0.45
  controls.maxPolarAngle = 2.25
  controls.target.set(0, 0.8, 0)

  scene.add(new THREE.AmbientLight(0x89d6ff, 2.1))

  const side = new THREE.PointLight(0x64f5d6, 28, 90)
  side.position.set(6, 8, 10)
  scene.add(side)

  const fill = new THREE.PointLight(0x1659c0, 20, 90)
  fill.position.set(-8, 6, -8)
  scene.add(fill)

  axesHelper = new THREE.AxesHelper(1.6)
  scene.add(axesHelper)

  ground = new THREE.Mesh(
    new THREE.CircleGeometry(10, 64),
    new THREE.MeshBasicMaterial({
      color: 0x2ee4d4,
      transparent: true,
      opacity: 0.08
    })
  )
  ground.rotation.x = -Math.PI / 2
  ground.position.y = -2.2
  scene.add(ground)
}

function cloneRobotMaterial(material) {
  if (!material) {
    return material
  }

  const cloned = material.clone()
  cloned.transparent = false
  cloned.opacity = 1
  cloned.depthWrite = true
  cloned.needsUpdate = true
  return cloned
}

function createFresnelMaterial() {
  const material = new THREE.ShaderMaterial({
    uniforms: {
      uBaseColor: { value: FRESNEL_BASE_COLOR.clone() },
      uEdgeColor: { value: FRESNEL_EDGE_COLOR.clone() },
      uBaseOpacity: { value: FRESNEL_BASE_OPACITY },
      uFresnelPower: { value: FRESNEL_POWER },
      uEdgeIntensity: { value: FRESNEL_EDGE_INTENSITY },
      uCameraPosition: { value: new THREE.Vector3() }
    },
    vertexShader: fresnelVertexShader,
    fragmentShader: fresnelFragmentShader,
    transparent: true,
    depthWrite: false,
    side: THREE.DoubleSide
  })

  fresnelMaterials.push(material)
  return material
}

function applyModelAppearance(model) {
  model.traverse((object) => {
    if (!object.isMesh) {
      return
    }

    object.castShadow = false
    object.receiveShadow = false

    const isRobot = object.name === 'robot'
    if (Array.isArray(object.material)) {
      object.material = object.material.map((material) => (
        isRobot ? cloneRobotMaterial(material) : createFresnelMaterial()
      ))
    } else {
      object.material = isRobot
        ? cloneRobotMaterial(object.material)
        : createFresnelMaterial()
    }
  })
}

function frameModel(model) {
  const box = new THREE.Box3().setFromObject(model)
  if (box.isEmpty()) {
    return
  }

  const size = box.getSize(new THREE.Vector3())
  const maxDimension = Math.max(size.x, size.y, size.z, 1)
  const scale = MODEL_DISPLAY_SIZE / maxDimension

  model.scale.setScalar(scale)
  model.rotation.set(0, -Math.PI / 2, 0)
  model.position.set(0, 0, 0)

  const alignedBox = new THREE.Box3().setFromObject(model)
  const alignedCenter = alignedBox.getCenter(new THREE.Vector3())
  model.position.set(
    0,
    -alignedBox.min.y,
    -alignedCenter.z
  )

  const fittedBox = new THREE.Box3().setFromObject(model)
  const fittedSize = fittedBox.getSize(new THREE.Vector3())
  const target = new THREE.Vector3(
    0,
    fittedBox.min.y + fittedSize.y * TARGET_HEIGHT_RATIO,
    0
  )
  const radius = Math.max(fittedSize.length() * 0.42, 2.5)

  controls.target.copy(target)
  camera.position.set(
    target.x + radius * CAMERA_X_OFFSET_FACTOR,
    target.y + radius * CAMERA_Y_OFFSET_FACTOR,
    target.z + radius * CAMERA_Z_OFFSET_FACTOR
  )
  controls.minDistance = Math.max(radius * 0.5, 2)
  controls.maxDistance = Math.max(radius * 3.2, 8)
  camera.near = 0.1
  camera.far = Math.max(radius * 12, 80)
  camera.updateProjectionMatrix()

  if (ground) {
    ground.position.y = fittedBox.min.y - 0.55
  }
  if (axesHelper) {
    axesHelper.position.copy(target)
  }

  controls.update()
}

async function loadModel() {
  const loadToken = ++activeLoadToken

  try {
    const gltf = await loader.loadAsync(piperobotModelUrl)
    if (loadToken !== activeLoadToken || !scene) {
      gltf.scene?.traverse?.((object) => {
        if (object.geometry) {
          object.geometry.dispose()
        }
      })
      return
    }

    rootModel = gltf.scene
    applyModelAppearance(rootModel)
    scene.add(rootModel)
    frameModel(rootModel)
  } catch (error) {
    console.error('[PipeThreeScene] failed to load piperobot.glb', error)
  }
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
  fresnelMaterials.forEach((material) => {
    material.uniforms.uCameraPosition.value.copy(camera.position)
  })
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
  loadModel()
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
  activeLoadToken += 1
  cancelAnimationFrame(frameId)
  resizeObserver?.disconnect()
  controls?.dispose()

  if (rootModel?.parent) {
    rootModel.parent.remove(rootModel)
  }

  scene?.traverse((object) => {
    if (object.geometry) {
      object.geometry.dispose()
    }
    if (object.material) {
      const materials = Array.isArray(object.material) ? object.material : [object.material]
      materials.forEach((material) => {
        if (material?.map) {
          material.map.dispose?.()
        }
        material?.dispose?.()
      })
    }
  })
  fresnelMaterials.splice(0).forEach((material) => material.dispose())

  renderer?.dispose()
  scene?.clear()
  if (renderer?.domElement?.parentNode) {
    renderer.domElement.parentNode.removeChild(renderer.domElement)
  }
  renderer = null
  scene = null
  camera = null
  controls = null
  rootModel = null
  ground = null
  axesHelper = null
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
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  overflow: hidden;
}
</style>
