import { useEffect, useState } from 'react'
import { getSensors, updateSensor, deleteSensor } from '../api/sensors'
import LoadingSpinner from '../components/LoadingSpinner'

export default function Admin() {
  const [sensors, setSensors] = useState([])
  const [edits, setEdits] = useState({})
  const [loading, setLoading] = useState(true)
  const [saving, setSaving] = useState({})
  const [confirm, setConfirm] = useState(null)

  useEffect(() => {
    getSensors().then(data => {
      setSensors(data)
      const e = {}
      data.forEach(s => { e[s.deviceId] = { name: s.name || '', location: s.location || '' } })
      setEdits(e)
      setLoading(false)
    })
  }, [])

  function handleChange(deviceId, field, value) {
    setEdits(prev => ({ ...prev, [deviceId]: { ...prev[deviceId], [field]: value } }))
  }

  async function handleSave(deviceId) {
    setSaving(prev => ({ ...prev, [deviceId]: true }))
    try {
      await updateSensor(deviceId, edits[deviceId])
      setSensors(prev => prev.map(s =>
        s.deviceId === deviceId ? { ...s, ...edits[deviceId] } : s
      ))
    } finally {
      setSaving(prev => ({ ...prev, [deviceId]: false }))
    }
  }

  async function handleDelete(deviceId) {
    await deleteSensor(deviceId)
    setSensors(prev => prev.filter(s => s.deviceId !== deviceId))
    setConfirm(null)
  }

  if (loading) return <LoadingSpinner />

  return (
    <div className="p-6 space-y-4">
      <div className="flex items-center justify-between">
        <h1 className="text-2xl font-bold text-white">Admin</h1>
        <span className="text-gray-400 text-sm">{sensors.length} sensors registered</span>
      </div>

      <div className="bg-gray-800 border border-gray-700 rounded-xl overflow-hidden">
        <table className="w-full text-sm">
          <thead>
            <tr className="border-b border-gray-700 text-gray-400 text-left">
              <th className="px-4 py-3">Device ID</th>
              <th className="px-4 py-3">Name</th>
              <th className="px-4 py-3">Location</th>
              <th className="px-4 py-3">Actions</th>
            </tr>
          </thead>
          <tbody>
            {sensors.map(s => (
              <tr key={s.deviceId} className="border-b border-gray-700">
                <td className="px-4 py-3 text-gray-400">#{s.deviceId}</td>
                <td className="px-4 py-2">
                  <input
                    value={edits[s.deviceId]?.name ?? ''}
                    onChange={e => handleChange(s.deviceId, 'name', e.target.value)}
                    className="bg-gray-700 border border-gray-600 text-white rounded px-2 py-1 w-full text-sm"
                  />
                </td>
                <td className="px-4 py-2">
                  <input
                    value={edits[s.deviceId]?.location ?? ''}
                    onChange={e => handleChange(s.deviceId, 'location', e.target.value)}
                    className="bg-gray-700 border border-gray-600 text-white rounded px-2 py-1 w-full text-sm"
                  />
                </td>
                <td className="px-4 py-2">
                  <div className="flex gap-2">
                    <button
                      onClick={() => handleSave(s.deviceId)}
                      disabled={saving[s.deviceId]}
                      className="px-3 py-1 bg-green-700 hover:bg-green-600 text-white rounded text-xs disabled:opacity-50"
                    >
                      {saving[s.deviceId] ? 'Saving…' : 'Save'}
                    </button>
                    <button
                      onClick={() => setConfirm(s.deviceId)}
                      className="px-3 py-1 bg-red-800 hover:bg-red-700 text-white rounded text-xs"
                    >
                      Delete
                    </button>
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {confirm && (
        <div className="fixed inset-0 bg-black/60 flex items-center justify-center z-50">
          <div className="bg-gray-800 border border-gray-700 rounded-xl p-6 w-80">
            <p className="text-white mb-4">Delete sensor #{confirm} and all its measurements?</p>
            <div className="flex gap-3 justify-end">
              <button onClick={() => setConfirm(null)}
                className="px-4 py-2 bg-gray-700 text-white rounded text-sm">Cancel</button>
              <button onClick={() => handleDelete(confirm)}
                className="px-4 py-2 bg-red-700 hover:bg-red-600 text-white rounded text-sm">Delete</button>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
