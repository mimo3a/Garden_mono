export const THRESHOLDS = {
  temperature: { ok: [10, 35], warn: [5, 40] },
  soil:        { ok: [40, 80], warn: [20, 40] },
}

export function getStatus(type, value) {
  const t = type.startsWith('soil') ? THRESHOLDS.soil : THRESHOLDS.temperature
  if (value >= t.ok[0] && value <= t.ok[1]) return 'ok'
  if (value >= t.warn[0] && value <= t.warn[1]) return 'warn'
  return 'critical'
}

export const STATUS_COLOR = {
  ok:       'text-green-400',
  warn:     'text-yellow-400',
  critical: 'text-red-400',
}

export const STATUS_BG = {
  ok:       'bg-green-500',
  warn:     'bg-yellow-500',
  critical: 'bg-red-500',
}
