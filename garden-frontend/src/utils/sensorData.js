import { getStatus } from '../constants/thresholds'

export const ONLINE_WINDOW_MS = 30 * 60 * 1000

export function toDate(timestamp) {
  if (!timestamp) return null
  return new Date(/[zZ]|[+-]\d\d:\d\d$/.test(timestamp) ? timestamp : `${timestamp}Z`)
}

export function latestByType(history = []) {
  const latest = {}
  history.forEach(measurement => {
    if (!latest[measurement.type]) latest[measurement.type] = measurement
  })
  return latest
}

export function latestTimestamp(history = []) {
  return history.reduce((latest, measurement) => {
    if (!latest || toDate(measurement.timestamp) > toDate(latest)) return measurement.timestamp
    return latest
  }, null)
}

export function isOnline(timestamp, now = Date.now()) {
  const date = toDate(timestamp)
  return Boolean(date && now - date.getTime() <= ONLINE_WINDOW_MS && now >= date.getTime())
}

export function ageLabel(timestamp, now = Date.now()) {
  const date = toDate(timestamp)
  if (!date) return 'No readings received'
  const minutes = Math.max(0, Math.floor((now - date.getTime()) / 60000))
  if (minutes < 1) return 'Just now'
  if (minutes < 60) return `${minutes} min ago`
  const hours = Math.floor(minutes / 60)
  if (hours < 24) return `${hours} h ago`
  return `${Math.floor(hours / 24)} d ago`
}

export function measurementUnit(type) {
  if (type === 'temperature') return '°C'
  if (type === 'battery') return 'mV'
  return '%'
}

export function measurementWarnings(latest) {
  return Object.values(latest).flatMap(measurement => {
    const status = getStatus(measurement.type, measurement.value)
    if (status === 'ok') return []
    const label = measurement.type === 'temperature'
      ? 'Temperature'
      : measurement.type === 'battery'
        ? 'Battery'
        : measurement.type.replace('soil', 'Soil ')
    return [{ status, label, measurement }]
  })
}

export function filterToRange(history, hours) {
  const after = Date.now() - hours * 60 * 60 * 1000
  return history.filter(measurement => toDate(measurement.timestamp)?.getTime() >= after)
}

// Keep dense hardware readings readable without inventing data: average real samples per bucket.
export function aggregateSeries(history, type, hours) {
  const bucketMs = hours <= 24 ? 60 * 60 * 1000 : hours <= 168 ? 6 * 60 * 60 * 1000 : 24 * 60 * 60 * 1000
  const buckets = new Map()
  filterToRange(history, hours).filter(measurement => measurement.type === type).forEach(measurement => {
    const time = toDate(measurement.timestamp)
    const key = Math.floor(time.getTime() / bucketMs) * bucketMs
    const bucket = buckets.get(key) || { timestamp: key, total: 0, count: 0 }
    bucket.total += measurement.value
    bucket.count += 1
    buckets.set(key, bucket)
  })
  return [...buckets.values()]
    .sort((a, b) => a.timestamp - b.timestamp)
    .map(bucket => ({
      time: new Date(bucket.timestamp).toLocaleString([], {
        month: 'short',
        day: 'numeric',
        hour: hours <= 168 ? '2-digit' : undefined,
        minute: hours <= 168 ? '2-digit' : undefined,
      }),
      value: Number((bucket.total / bucket.count).toFixed(2)),
    }))
}
