import { useEffect, useMemo, useState } from 'react'
import { useParams } from 'react-router-dom'
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from 'recharts'
import { getSensors, getSensorHistory } from '../api/sensors'
import { STATUS_COLOR } from '../constants/thresholds'
import { formatDateTime } from '../utils/time'
import { ageLabel, aggregateSeries, filterToRange, isOnline, latestByType, latestTimestamp, measurementUnit, measurementWarnings } from '../utils/sensorData'
import LoadingSpinner from '../components/LoadingSpinner'

const SOIL_COLORS = ['#22c55e', '#3b82f6', '#f59e0b', '#ec4899']
const RANGES = [{ label: '24h', hours: 24 }, { label: '7d', hours: 168 }, { label: '30d', hours: 720 }]

function exportCSV(rows) {
  const blob = new Blob([['timestamp,type,value', ...rows.map(row => `${row.timestamp},${row.type},${row.value}`)].join('\n')], { type: 'text/csv' })
  const url = URL.createObjectURL(blob)
  Object.assign(document.createElement('a'), { href: url, download: 'measurements.csv' }).click()
  URL.revokeObjectURL(url)
}

function EmptyChart({ children }) { return <div className="h-[220px] grid place-items-center text-sm text-gray-500 text-center px-6">{children}</div> }

function ValueTile({ label, measurement }) {
  return <div className="bg-gray-800 border border-gray-700 rounded-xl p-4"><p className="text-xs uppercase tracking-wide text-gray-500">{label}</p><p className="mt-1 text-2xl font-semibold text-white">{measurement ? `${measurement.value.toFixed(measurement.type === 'temperature' ? 1 : 0)} ${measurementUnit(measurement.type)}` : '—'}</p></div>
}

function HistoryChart({ title, data, children, empty }) {
  return <section className="bg-gray-800 border border-gray-700 rounded-xl p-4"><h2 className="text-white font-semibold mb-4">{title}</h2>{data.length ? <ResponsiveContainer width="100%" height={220}><LineChart data={data}><CartesianGrid strokeDasharray="3 3" stroke="#374151" /><XAxis dataKey="time" tick={{ fill: '#9ca3af', fontSize: 11 }} interval="preserveStartEnd" /><YAxis tick={{ fill: '#9ca3af', fontSize: 11 }} /><Tooltip contentStyle={{ background: '#1f2937', border: '1px solid #374151', color: '#fff' }} />{children}</LineChart></ResponsiveContainer> : <EmptyChart>{empty}</EmptyChart>}</section>
}

export default function SensorDetail() {
  const { deviceId } = useParams()
  const [sensor, setSensor] = useState(null)
  const [history, setHistory] = useState([])
  const [loading, setLoading] = useState(true)
  const [range, setRange] = useState(24)
  const [page, setPage] = useState(0)
  const PAGE_SIZE = 50

  useEffect(() => {
    let mounted = true
    Promise.all([getSensors(), getSensorHistory(deviceId)]).then(([sensors, measurements]) => {
      if (!mounted) return
      setSensor(sensors.find(item => String(item.deviceId) === String(deviceId)))
      setHistory(measurements)
    }).finally(() => { if (mounted) setLoading(false) })
    return () => { mounted = false }
  }, [deviceId])

  const latest = useMemo(() => latestByType(history), [history])
  const lastSeen = latestTimestamp(history)
  const online = isOnline(lastSeen)
  const warnings = measurementWarnings(latest)
  const selected = filterToRange(history, range)
  const temperature = aggregateSeries(history, 'temperature', range)
  const battery = aggregateSeries(history, 'battery', range)
  const soilTypes = ['soil1', 'soil2', 'soil3', 'soil4'].filter(type => history.some(item => item.type === type))
  const soilData = useMemo(() => {
    const values = new Map()
    soilTypes.forEach(type => aggregateSeries(history, type, range).forEach(point => {
      const row = values.get(point.time) || { time: point.time }
      row[type] = point.value
      values.set(point.time, row)
    }))
    return [...values.values()]
  }, [history, range, soilTypes.join('|')])
  const pageRows = history.slice(page * PAGE_SIZE, (page + 1) * PAGE_SIZE)
  const totalPages = Math.max(1, Math.ceil(history.length / PAGE_SIZE))

  if (loading) return <LoadingSpinner />

  return <div className="p-6 space-y-6">
    <div className="flex flex-col gap-4 sm:flex-row sm:items-start sm:justify-between"><div><div className="flex items-center gap-2"><span className={`h-2.5 w-2.5 rounded-full ${online ? 'bg-green-400' : 'bg-red-400'}`} /><span className={`text-sm font-medium ${online ? 'text-green-400' : 'text-red-400'}`}>{online ? 'Online' : 'Offline'}</span></div><h1 className="mt-1 text-2xl font-bold text-white">{sensor?.name || `Device ${deviceId}`}</h1><p className="text-gray-400 text-sm">{sensor?.location || 'No location'} · #{deviceId} · Last signal {ageLabel(lastSeen)}</p></div><button onClick={() => exportCSV(history)} className="px-4 py-2 bg-gray-700 hover:bg-gray-600 text-white rounded-lg text-sm transition-colors">Export CSV</button></div>

    <section><h2 className="text-sm font-semibold uppercase tracking-wide text-gray-400 mb-3">Current readings</h2><div className="grid grid-cols-2 lg:grid-cols-5 gap-3"><ValueTile label="Temperature" measurement={latest.temperature} />{['soil1', 'soil2', 'soil3', 'soil4'].map(type => <ValueTile key={type} label={type.replace('soil', 'Soil ')} measurement={latest[type]} />)}</div></section>

    <section className="bg-gray-800 border border-gray-700 rounded-xl p-4"><div className="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between mb-4"><div><h2 className="text-white font-semibold">History</h2><p className="text-xs text-gray-500">Averages of recorded readings; longer ranges use wider time buckets.</p></div><div className="inline-flex rounded-lg bg-gray-900 p-1">{RANGES.map(item => <button key={item.hours} onClick={() => { setRange(item.hours); setPage(0) }} className={`px-3 py-1.5 rounded-md text-sm ${range === item.hours ? 'bg-green-500 text-gray-950 font-semibold' : 'text-gray-400 hover:text-white'}`}>{item.label}</button>)}</div></div><h3 className="text-sm font-medium text-gray-300 mb-2">Temperature (°C)</h3>{temperature.length ? <ResponsiveContainer width="100%" height={220}><LineChart data={temperature}><CartesianGrid strokeDasharray="3 3" stroke="#374151" /><XAxis dataKey="time" tick={{ fill: '#9ca3af', fontSize: 11 }} interval="preserveStartEnd" /><YAxis tick={{ fill: '#9ca3af', fontSize: 11 }} /><Tooltip contentStyle={{ background: '#1f2937', border: '1px solid #374151', color: '#fff' }} /><Line type="monotone" dataKey="value" name="Temperature" stroke="#22c55e" dot={false} strokeWidth={2} /></LineChart></ResponsiveContainer> : <EmptyChart>No temperature readings in this range.</EmptyChart>}</section>

    <section className="bg-gray-800 border border-gray-700 rounded-xl p-4"><h2 className="text-white font-semibold mb-4">Soil moisture (%)</h2>{soilData.length ? <ResponsiveContainer width="100%" height={220}><LineChart data={soilData}><CartesianGrid strokeDasharray="3 3" stroke="#374151" /><XAxis dataKey="time" tick={{ fill: '#9ca3af', fontSize: 11 }} interval="preserveStartEnd" /><YAxis domain={[0, 100]} tick={{ fill: '#9ca3af', fontSize: 11 }} /><Tooltip contentStyle={{ background: '#1f2937', border: '1px solid #374151', color: '#fff' }} /><Legend wrapperStyle={{ color: '#9ca3af' }} />{soilTypes.map((type, index) => <Line key={type} type="monotone" dataKey={type} stroke={SOIL_COLORS[index]} dot={false} strokeWidth={2} />)}</LineChart></ResponsiveContainer> : <EmptyChart>No soil readings in this range.</EmptyChart>}</section>

    <HistoryChart title="Battery (mV)" data={battery} empty="Battery readings are not available from this device yet."><Line type="monotone" dataKey="value" name="Battery" stroke="#60a5fa" dot={false} strokeWidth={2} /></HistoryChart>

    <section className="bg-gray-800 border border-gray-700 rounded-xl p-4"><h2 className="text-white font-semibold mb-3">Diagnostics</h2><div className="grid grid-cols-1 sm:grid-cols-3 gap-3 text-sm"><div><p className="text-gray-500">Last communication</p><p className={online ? 'text-green-400' : 'text-red-400'}>{ageLabel(lastSeen)}</p></div><div><p className="text-gray-500">Readings in selected range</p><p className="text-white">{selected.length}</p></div><div><p className="text-gray-500">Available measurement types</p><p className="text-white">{Object.keys(latest).sort().join(', ') || 'None'}</p></div></div><div className="mt-4 border-t border-gray-700 pt-3 text-sm">{warnings.length ? warnings.map(warning => <p key={warning.measurement.type} className={STATUS_COLOR[warning.status]}>{warning.label}: {warning.measurement.value} {measurementUnit(warning.measurement.type)} is outside the normal range.</p>) : <p className="text-green-400">No current threshold warnings from available readings.</p>}</div></section>

    <section className="bg-gray-800 border border-gray-700 rounded-xl overflow-hidden"><div className="px-4 py-3 flex items-center justify-between"><h2 className="text-white font-semibold">All recorded measurements</h2><span className="text-xs text-gray-500">{history.length} total</span></div><div className="overflow-x-auto"><table className="w-full text-sm"><thead><tr className="border-y border-gray-700 text-gray-400 text-left"><th className="px-4 py-3">Timestamp</th><th className="px-4 py-3">Type</th><th className="px-4 py-3">Value</th></tr></thead><tbody>{pageRows.map(measurement => <tr key={measurement.id} className="border-b border-gray-700"><td className="px-4 py-2 text-gray-400">{formatDateTime(measurement.timestamp)}</td><td className="px-4 py-2 text-gray-300">{measurement.type}</td><td className="px-4 py-2 text-white">{measurement.value.toFixed(2)} {measurementUnit(measurement.type)}</td></tr>)}{!pageRows.length && <tr><td colSpan="3" className="px-4 py-6 text-center text-gray-500">No measurements received yet.</td></tr>}</tbody></table></div><div className="flex items-center justify-between px-4 py-3 text-sm text-gray-400"><span>Page {page + 1} / {totalPages}</span><div className="flex gap-2"><button onClick={() => setPage(value => Math.max(0, value - 1))} disabled={page === 0} className="px-3 py-1 bg-gray-700 rounded disabled:opacity-40">Prev</button><button onClick={() => setPage(value => Math.min(totalPages - 1, value + 1))} disabled={page >= totalPages - 1} className="px-3 py-1 bg-gray-700 rounded disabled:opacity-40">Next</button></div></div></section>
  </div>
}
