const imageModules = import.meta.glob('../assets/images/*', {
  eager: true,
  import: 'default'
})

export function resolveImageAsset(baseName) {
  if (!baseName) {
    return ''
  }

  const normalizedName = String(baseName).trim().toLowerCase()
  const match = Object.entries(imageModules).find(([path]) => {
    const fileName = path.split('/').pop() || ''
    const nameWithoutExtension = fileName.replace(/\.[^.]+$/, '').toLowerCase()
    return nameWithoutExtension === normalizedName
  })

  return match?.[1] || ''
}
