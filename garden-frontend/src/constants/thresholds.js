export const THRESHOLDS = {
  temperature: { ok: [10, 35], warn: [5, 40] },
  soil:        { ok: [40, 80], warn: [20, 40] },
  battery:     { ok: [3600, 4300], warn: [3400, 3600] },
}

export function getStatus(type, value) {
  const key = type.startsWith('soil') ? 'soil' : type
  const t = THRESHOLDS[key] ?? THRESHOLDS.temperature
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
