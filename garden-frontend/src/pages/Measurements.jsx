import { useEffect, useState } from 'react'
import { getSensors, getSensorHistory } from '../api/sensors'
import { formatDateTime } from '../utils/time'
import LoadingSpinner from '../components/LoadingSpinner'

function exportCSV(rows) {
  const header = 'timestamp,sensor,type,value'
  const lines = rows.map(r => `${r.timestamp},${r.sensorName},${r.type},${r.value}`)
  const blob = new Blob([[header, ...lines].join('\n')], { type: 'text/csv' })
  const a = Object.assign(document.createElement('a'), { href: URL.createObjectURL(blob), download: 'measurements.csv' })
  a.click()
}

export default function Measurements() {
  const [allRows, setAllRows] = useState([])
  const [sensors, setSensors] = useState([])
  const [loading, setLoading] = useState(true)
  const [filterSensor, setFilterSensor] = useState('')
  const [filterType, setFilterType] = useState('')
  const [page, setPage] = useState(0)
  const PAGE_SIZE = 50

  useEffect(() => {
    getSensors().then(async data => {
      setSensors(data)
      const rows = []
      await Promise.all(
        data.map(async s => {
          const hist = await getSensorHistory(s.deviceId)
          hist.forEach(m => rows.push({ ...m, sensorName: s.name || `Device ${s.deviceId}`, deviceId: s.deviceId }))
        })
      )
      rows.sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp))
      setAllRows(rows)
      setLoading(false)
    })
  }, [])

  const types = [...new Set(allRows.map(r => r.type))].sort()

  const filtered = allRows.filter(r =>
    (!filterSensor || String(r.deviceId) === filterSensor) &&
    (!filterType   || r.type === filterType)
  )

  const pageRows = filtered.slice(page * PAGE_SIZE, (page + 1) * PAGE_SIZE)
  const totalPages = Math.ceil(filtered.length / PAGE_SIZE)

  if (loading) return <LoadingSpinner />

  return (
    <div className="p-6 space-y-4">
      <div className="flex items-center justify-between flex-wrap gap-3">
        <h1 className="text-2xl font-bold text-white">Measurements</h1>
        <button onClick={() => exportCSV(filtered)}
          className="px-4 py-2 bg-gray-700 hover:bg-gray-600 text-white rounded-lg text-sm transition-colors">
          Export CSV
        </button>
      </div>

      <div className="flex gap-3 flex-wrap">
        <select value={filterSensor} onChange={e => { setFilterSensor(e.target.value); setPage(0) }}
          className="bg-gray-800 border border-gray-700 text-white rounded-lg px-3 py-2 text-sm">
          <option value="">All sensors</option>
          {sensors.map(s => (
            <option key={s.deviceId} value={s.deviceId}>{s.name || `Device ${s.deviceId}`}</option>
          ))}
        </select>
        <select value={filterType} onChange={e => { setFilterType(e.target.value); setPage(0) }}
          className="bg-gray-800 border border-gray-700 text-white rounded-lg px-3 py-2 text-sm">
          <option value="">All types</option>
          {types.map(t => <option key={t} value={t}>{t}</option>)}
        </select>
      </div>

      <div className="bg-gray-800 border border-gray-700 rounded-xl overflow-hidden">
        <table className="w-full text-sm">
          <thead>
            <tr className="border-b border-gray-700 text-gray-400 text-left">
              <th className="px-4 py-3">Timestamp</th>
              <th className="px-4 py-3">Sensor</th>
              <th className="px-4 py-3">Type</th>
              <th className="px-4 py-3">Value</th>
            </tr>
          </thead>
          <tbody>
            {pageRows.map(m => (
              <tr key={m.id} className="border-b border-gray-700">
                <td className="px-4 py-2 text-gray-400">{formatDateTime(m.timestamp)}</td>
                <td className="px-4 py-2 text-gray-300">{m.sensorName}</td>
                <td className="px-4 py-2 text-gray-300">{m.type}</td>
                <td className="px-4 py-2 text-white">
                  {m.value.toFixed(2)}{m.type === 'temperature' ? ' °C' : m.type === 'battery' ? ' mV' : ' %'}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
        <div className="flex items-center justify-between px-4 py-3 text-sm text-gray-400">
          <span>{filtered.length} total</span>
          <div className="flex gap-2">
            <button onClick={() => setPage(p => Math.max(0, p - 1))} disabled={page === 0}
              className="px-3 py-1 bg-gray-700 rounded disabled:opacity-40">Prev</button>
            <span>{page + 1} / {totalPages || 1}</span>
            <button onClick={() => setPage(p => Math.min(totalPages - 1, p + 1))} disabled={page >= totalPages - 1}
              className="px-3 py-1 bg-gray-700 rounded disabled:opacity-40">Next</button>
          </div>
        </div>
      </div>
    </div>
  )
}
