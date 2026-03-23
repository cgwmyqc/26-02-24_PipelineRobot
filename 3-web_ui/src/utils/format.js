export function formatValue(value, fractionDigits = 1) {
  const numeric = Number(value)
  if (Number.isNaN(numeric)) {
    return '--'
  }
  return numeric.toFixed(fractionDigits)
}
