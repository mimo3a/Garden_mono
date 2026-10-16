import { useEffect, useState } from 'react'
import { getSensors, getSensorHistory } from '../api/sensors'
import SensorCard from '../components/SensorCard'
import LoadingSpinner from '../components/LoadingSpinner'
import { ageLabel, isOnline, latestTimestamp, measurementWarnings } from '../utils/sensorData'

export default function Dashboard() {
  const [sensors, setSensors] = useState([])
  const [latestMap, setLatestMap] = useState({})
  const [loading, setLoading] = useState(true)
  const [now, setNow] = useState(Date.now())

  useEffect(() => {
    let mounted = true
    async function load() {
      try {
        const data = await getSensors()
        const histories = await Promise.all(data.map(async sensor => {
          try { return [sensor.deviceId, await getSensorHistory(sensor.deviceId)] } catch { return [sensor.deviceId, []] }
        }))
        if (!mounted) return
        setSensors(data)
        setLatestMap(Object.fromEntries(histories.map(([id, history]) => {
          const byType = {}
          history.forEach(measurement => { if (!byType[measurement.type]) byType[measurement.type] = measurement })
          return [id, Object.values(byType)]
        })))
      } finally {
        if (mounted) setLoading(false)
      }
    }
    load()
    const timer = setInterval(() => setNow(Date.now()), 60_000)
    return () => { mounted = false; clearInterval(timer) }
  }, [])

  if (loading) return <LoadingSpinner />

  const summaries = sensors.map(sensor => {
    const latest = latestMap[sensor.deviceId] || []
    const lastSeen = latestTimestamp(latest)
    return { sensor, latest, lastSeen, online: isOnline(lastSeen, now), warnings: measurementWarnings(Object.fromEntries(latest.map(m => [m.type, m]))) }
  })
  const onlineCount = summaries.filter(summary => summary.online).length
  const warningCount = summaries.reduce((count, summary) => count + summary.warnings.length, 0)
  const newest = summaries.map(summary => summary.lastSeen).filter(Boolean).sort((a, b) => new Date(b) - new Date(a))[0]

  return (
    <div className="p-6">
      <div className="flex flex-col gap-1 sm:flex-row sm:items-end sm:justify-between mb-6">
        <div><h1 className="text-2xl font-bold text-white">Garden overview</h1><p className="text-sm text-gray-400">Last garden update: {ageLabel(newest, now)}</p></div>
        <p className="text-xs text-gray-500">Live status refreshes every minute</p>
      </div>
      {sensors.length === 0 ? (
        <p className="text-gray-500">No sensors registered yet.</p>
      ) : (
        <>
          <div className="grid grid-cols-1 sm:grid-cols-3 gap-4 mb-6">
            <div className="bg-gray-800 border border-gray-700 rounded-xl p-4"><p className="text-sm text-gray-400">Sensors online</p><p className="text-3xl font-bold text-green-400">{onlineCount}<span className="text-base text-gray-500"> / {sensors.length}</span></p></div>
            <div className="bg-gray-800 border border-gray-700 rounded-xl p-4"><p className="text-sm text-gray-400">Offline sensors</p><p className="text-3xl font-bold text-red-400">{sensors.length - onlineCount}</p></div>
            <div className="bg-gray-800 border border-gray-700 rounded-xl p-4"><p className="text-sm text-gray-400">Active warnings</p><p className={`text-3xl font-bold ${warningCount ? 'text-yellow-400' : 'text-green-400'}`}>{warningCount}</p></div>
          </div>
          {summaries.some(summary => !summary.online || summary.warnings.length) && <div className="mb-6 bg-gray-800 border border-yellow-900/70 rounded-xl p-4"><h2 className="font-semibold text-white mb-2">Needs attention</h2><div className="space-y-1 text-sm text-gray-300">{summaries.filter(summary => !summary.online || summary.warnings.length).map(summary => <p key={summary.sensor.deviceId}><span className={!summary.online ? 'text-red-400' : 'text-yellow-400'}>{!summary.online ? 'Offline' : 'Warning'}</span> · {summary.sensor.name || `Device ${summary.sensor.deviceId}`} — {!summary.online ? `last signal ${ageLabel(summary.lastSeen, now)}` : `${summary.warnings.map(warning => warning.label).join(', ')} outside the normal range`}</p>)}</div></div>}
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
            {summaries.map(({ sensor, latest }) => <SensorCard key={sensor.deviceId} sensor={sensor} latest={latest} now={now} />)}
          </div>
        </>
      )}
    </div>
  )
}
