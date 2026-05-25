import { useEffect, useState } from 'react'
import { getSensors, getSensorHistory } from '../api/sensors'
import SensorCard from '../components/SensorCard'
import LoadingSpinner from '../components/LoadingSpinner'

export default function Dashboard() {
  const [sensors, setSensors] = useState([])
  const [latestMap, setLatestMap] = useState({})
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    getSensors().then(async data => {
      setSensors(data)
      const map = {}
      await Promise.all(
        data.map(async s => {
          const history = await getSensorHistory(s.deviceId)
          const byType = {}
          for (const m of history) {
            if (!byType[m.type]) byType[m.type] = m
          }
          map[s.deviceId] = Object.values(byType)
        })
      )
      setLatestMap(map)
      setLoading(false)
    })
  }, [])

  if (loading) return <LoadingSpinner />

  return (
    <div className="p-6">
      <h1 className="text-2xl font-bold text-white mb-6">Dashboard</h1>
      {sensors.length === 0 ? (
        <p className="text-gray-500">No sensors registered yet.</p>
      ) : (
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4">
          {sensors.map(s => (
            <SensorCard key={s.deviceId} sensor={s} latest={latestMap[s.deviceId]} />
          ))}
        </div>
      )}
    </div>
  )
}
