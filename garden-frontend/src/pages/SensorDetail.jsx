import { useEffect, useState } from 'react'
import { useParams } from 'react-router-dom'
import {
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer
} from 'recharts'
import { getSensors, getSensorHistory } from '../api/sensors'
import { formatDateTime, formatTime } from '../utils/time'
import LoadingSpinner from '../components/LoadingSpinner'

const SOIL_COLORS = ['#22c55e', '#3b82f6', '#f59e0b', '#ec4899']

function exportCSV(rows) {
  const header = 'timestamp,type,value'
  const lines = rows.map(r => `${r.timestamp},${r.type},${r.value}`)
  const blob = new Blob([[header, ...lines].join('\n')], { type: 'text/csv' })
  const a = Object.assign(document.createElement('a'), { href: URL.createObjectURL(blob), download: 'measurements.csv' })
  a.click()
}

export default function SensorDetail() {
  const { deviceId } = useParams()
  const [sensor, setSensor] = useState(null)
  const [history, setHistory] = useState([])
  const [loading, setLoading] = useState(true)
  const [page, setPage] = useState(0)
  const PAGE_SIZE = 50

  useEffect(() => {
    Promise.all([
      getSensors(),
      getSensorHistory(deviceId),
    ]).then(([sensors, hist]) => {
      setSensor(sensors.find(s => String(s.deviceId) === String(deviceId)))
      setHistory(hist)
      setLoading(false)
    })
  }, [deviceId])

  if (loading) return <LoadingSpinner />

  const tempData = history
    .filter(m => m.type === 'temperature')
    .slice().reverse()
    .map(m => ({ time: formatTime(m.timestamp), value: m.value }))

  const soilTypes = ['soil1', 'soil2', 'soil3', 'soil4']
  const soilData = (() => {
    const byTime = {}
    history.filter(m => m.type.startsWith('soil')).forEach(m => {
      const t = formatTime(m.timestamp)
      if (!byTime[t]) byTime[t] = { time: t }
      byTime[t][m.type] = m.value
    })
    return Object.values(byTime).reverse()
  })()

  const pageRows = history.slice(page * PAGE_SIZE, (page + 1) * PAGE_SIZE)
  const totalPages = Math.ceil(history.length / PAGE_SIZE)

  return (
    <div className="p-6 space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-white">
            {sensor?.name || `Device ${deviceId}`}
          </h1>
          <p className="text-gray-400 text-sm">{sensor?.location || 'No location'} · #{deviceId}</p>
        </div>
        <button
          onClick={() => exportCSV(history)}
          className="px-4 py-2 bg-gray-700 hover:bg-gray-600 text-white rounded-lg text-sm transition-colors"
        >
          Export CSV
        </button>
      </div>

      <div className="bg-gray-800 border border-gray-700 rounded-xl p-4">
        <h2 className="text-white font-semibold mb-4">Temperature (°C)</h2>
        <ResponsiveContainer width="100%" height={220}>
          <LineChart data={tempData}>
            <CartesianGrid strokeDasharray="3 3" stroke="#374151" />
            <XAxis dataKey="time" tick={{ fill: '#9ca3af', fontSize: 11 }} interval="preserveStartEnd" />
            <YAxis tick={{ fill: '#9ca3af', fontSize: 11 }} />
            <Tooltip contentStyle={{ background: '#1f2937', border: '1px solid #374151', color: '#fff' }} />
            <Line type="monotone" dataKey="value" stroke="#22c55e" dot={false} strokeWidth={2} />
          </LineChart>
        </ResponsiveContainer>
      </div>

      <div className="bg-gray-800 border border-gray-700 rounded-xl p-4">
        <h2 className="text-white font-semibold mb-4">Soil moisture (%)</h2>
        <ResponsiveContainer width="100%" height={220}>
          <LineChart data={soilData}>
            <CartesianGrid strokeDasharray="3 3" stroke="#374151" />
            <XAxis dataKey="time" tick={{ fill: '#9ca3af', fontSize: 11 }} interval="preserveStartEnd" />
            <YAxis domain={[0, 100]} tick={{ fill: '#9ca3af', fontSize: 11 }} />
            <Tooltip contentStyle={{ background: '#1f2937', border: '1px solid #374151', color: '#fff' }} />
            <Legend wrapperStyle={{ color: '#9ca3af' }} />
            {soilTypes.map((t, i) => (
              <Line key={t} type="monotone" dataKey={t} stroke={SOIL_COLORS[i]} dot={false} strokeWidth={2} />
            ))}
          </LineChart>
        </ResponsiveContainer>
      </div>

      <div className="bg-gray-800 border border-gray-700 rounded-xl overflow-hidden">
        <table className="w-full text-sm">
          <thead>
            <tr className="border-b border-gray-700 text-gray-400 text-left">
              <th className="px-4 py-3">Timestamp</th>
              <th className="px-4 py-3">Type</th>
              <th className="px-4 py-3">Value</th>
            </tr>
          </thead>
          <tbody>
            {pageRows.map(m => (
              <tr key={m.id} className="border-b border-gray-700">
                <td className="px-4 py-2 text-gray-400">{formatDateTime(m.timestamp)}</td>
                <td className="px-4 py-2 text-gray-300">{m.type}</td>
                <td className="px-4 py-2 text-white">
                  {m.value.toFixed(2)}{m.type === 'temperature' ? ' °C' : ' %'}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
        <div className="flex items-center justify-between px-4 py-3 text-sm text-gray-400">
          <span>{history.length} total</span>
          <div className="flex gap-2">
            <button onClick={() => setPage(p => Math.max(0, p - 1))} disabled={page === 0}
              className="px-3 py-1 bg-gray-700 rounded disabled:opacity-40">Prev</button>
            <span>{page + 1} / {totalPages}</span>
            <button onClick={() => setPage(p => Math.min(totalPages - 1, p + 1))} disabled={page >= totalPages - 1}
              className="px-3 py-1 bg-gray-700 rounded disabled:opacity-40">Next</button>
          </div>
        </div>
      </div>
    </div>
  )
}
