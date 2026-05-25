import { Link } from 'react-router-dom'
import { getStatus, STATUS_COLOR, STATUS_BG } from '../constants/thresholds'

function SoilBar({ label, value }) {
  const status = getStatus('soil', value)
  return (
    <div className="mt-1">
      <div className="flex justify-between text-xs text-gray-400 mb-0.5">
        <span>{label}</span>
        <span className={STATUS_COLOR[status]}>{value ?? '—'}%</span>
      </div>
      <div className="w-full bg-gray-700 rounded-full h-1.5">
        <div
          className={`h-1.5 rounded-full ${STATUS_BG[status]}`}
          style={{ width: `${Math.min(value ?? 0, 100)}%` }}
        />
      </div>
    </div>
  )
}

export default function SensorCard({ sensor, latest }) {
  const temp = latest?.find(m => m.type === 'temperature')
  const soils = ['soil1', 'soil2', 'soil3', 'soil4']
    .map(t => ({ label: t, value: latest?.find(m => m.type === t)?.value }))
    .filter(s => s.value != null)

  const tempStatus = temp ? getStatus('temperature', temp.value) : 'ok'

  return (
    <Link
      to={`/sensors/${sensor.deviceId}`}
      className="block bg-gray-800 border border-gray-700 rounded-xl p-4 hover:border-green-500 transition-colors"
    >
      <div className="flex justify-between items-start mb-3">
        <div>
          <p className="font-semibold text-white">{sensor.name || `Device ${sensor.deviceId}`}</p>
          <p className="text-xs text-gray-500">{sensor.location || 'No location'}</p>
        </div>
        <span className="text-xs text-gray-600">#{sensor.deviceId}</span>
      </div>

      <div className={`text-3xl font-bold mb-3 ${STATUS_COLOR[tempStatus]}`}>
        {temp ? `${temp.value.toFixed(1)}°C` : '—'}
      </div>

      {soils.map(s => (
        <SoilBar key={s.label} label={s.label} value={Math.round(s.value)} />
      ))}
    </Link>
  )
}
