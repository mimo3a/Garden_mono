import { useEffect, useState } from 'react'
import { Link } from 'react-router-dom'
import { getSensors, getSensorHistory } from '../api/sensors'
import LoadingSpinner from '../components/LoadingSpinner'

export default function SensorList() {
  const [sensors, setSensors] = useState([])
  const [lastSeen, setLastSeen] = useState({})
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    getSensors().then(async data => {
      setSensors(data)
      const map = {}
      await Promise.all(
        data.map(async s => {
          const history = await getSensorHistory(s.deviceId)
          if (history.length > 0) map[s.deviceId] = history[0].timestamp
        })
      )
      setLastSeen(map)
      setLoading(false)
    })
  }, [])

  function isOnline(ts) {
    if (!ts) return false
    return (Date.now() - new Date(ts).getTime()) < 30 * 60 * 1000
  }

  if (loading) return <LoadingSpinner />

  return (
    <div className="p-6">
      <h1 className="text-2xl font-bold text-white mb-6">Sensors</h1>
      <div className="bg-gray-800 border border-gray-700 rounded-xl overflow-hidden">
        <table className="w-full text-sm">
          <thead>
            <tr className="border-b border-gray-700 text-gray-400 text-left">
              <th className="px-4 py-3">Device ID</th>
              <th className="px-4 py-3">Name</th>
              <th className="px-4 py-3">Location</th>
              <th className="px-4 py-3">Last seen</th>
              <th className="px-4 py-3">Status</th>
            </tr>
          </thead>
          <tbody>
            {sensors.map(s => (
              <tr key={s.deviceId} className="border-b border-gray-700 hover:bg-gray-750">
                <td className="px-4 py-3 text-gray-300">
                  <Link to={`/sensors/${s.deviceId}`} className="text-green-400 hover:underline">
                    #{s.deviceId}
                  </Link>
                </td>
                <td className="px-4 py-3 text-white">{s.name || '—'}</td>
                <td className="px-4 py-3 text-gray-400">{s.location || '—'}</td>
                <td className="px-4 py-3 text-gray-400">
                  {lastSeen[s.deviceId]
                    ? new Date(lastSeen[s.deviceId]).toLocaleString()
                    : '—'}
                </td>
                <td className="px-4 py-3">
                  <span className={`px-2 py-0.5 rounded-full text-xs font-medium ${
                    isOnline(lastSeen[s.deviceId])
                      ? 'bg-green-900 text-green-300'
                      : 'bg-gray-700 text-gray-400'
                  }`}>
                    {isOnline(lastSeen[s.deviceId]) ? 'Online' : 'Offline'}
                  </span>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  )
}
