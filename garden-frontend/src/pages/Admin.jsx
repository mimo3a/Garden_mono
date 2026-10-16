import { useEffect, useState } from 'react'
import { getSensors, updateSensor, deleteSensor } from '../api/sensors'
import { createInvitation } from '../api/auth'
import { useAuth } from '../auth/AuthContext'
import LoadingSpinner from '../components/LoadingSpinner'

export default function Admin() {
  const { user } = useAuth()
  const [sensors, setSensors] = useState([])
  const [edits, setEdits] = useState({})
  const [loading, setLoading] = useState(true)
  const [saving, setSaving] = useState({})
  const [confirm, setConfirm] = useState(null)
  const [inviteEmail, setInviteEmail] = useState('')
  const [inviteDeviceIds, setInviteDeviceIds] = useState([])
  const [inviteLink, setInviteLink] = useState('')
  const [inviteError, setInviteError] = useState('')
  const [inviting, setInviting] = useState(false)

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

  async function handleInvite(event) {
    event.preventDefault()
    setInviteError(''); setInviteLink(''); setInviting(true)
    try {
      const invitation = await createInvitation(inviteEmail, inviteDeviceIds)
      setInviteLink(`${window.location.origin}/accept-invitation?token=${invitation.token}`)
      setInviteEmail(''); setInviteDeviceIds([])
    } catch (error) {
      setInviteError(error.response?.data?.message || 'Unable to create invitation.')
    } finally { setInviting(false) }
  }

  function toggleInviteSensor(deviceId) {
    setInviteDeviceIds(current => current.includes(deviceId) ? current.filter(id => id !== deviceId) : [...current, deviceId])
  }

  if (loading) return <LoadingSpinner />

  return (
    <div className="p-6 space-y-4">
      <div className="flex items-center justify-between">
        <h1 className="text-2xl font-bold text-white">Admin</h1>
        <span className="text-gray-400 text-sm">{sensors.length} sensors registered</span>
      </div>

      {user.role === 'ADMIN' && <form onSubmit={handleInvite} className="bg-gray-800 border border-gray-700 rounded-xl p-4 space-y-3">
        <div><h2 className="text-white font-semibold">Invite a user</h2><p className="text-sm text-gray-400">Choose the sensors they may access. The link expires after 7 days and can be used once.</p></div>
        <input type="email" required placeholder="person@example.com" value={inviteEmail} onChange={event => setInviteEmail(event.target.value)} className="bg-gray-700 border border-gray-600 text-white rounded px-3 py-2 w-full sm:max-w-md text-sm" />
        <div className="flex flex-wrap gap-x-4 gap-y-2 text-sm text-gray-300">{sensors.map(sensor => <label key={sensor.deviceId} className="inline-flex items-center gap-2"><input type="checkbox" checked={inviteDeviceIds.includes(sensor.deviceId)} onChange={() => toggleInviteSensor(sensor.deviceId)} />#{sensor.deviceId} · {sensor.name || `Device ${sensor.deviceId}`}</label>)}</div>
        {inviteError && <p className="text-sm text-red-400">{inviteError}</p>}
        {inviteLink && <div className="text-sm"><p className="text-green-400 mb-1">Invitation created. Send this private link:</p><input readOnly value={inviteLink} className="bg-gray-900 border border-gray-600 text-gray-300 rounded px-3 py-2 w-full" onFocus={event => event.target.select()} /></div>}
        <button disabled={inviting || inviteDeviceIds.length === 0} className="px-4 py-2 bg-green-700 hover:bg-green-600 disabled:opacity-50 text-white rounded text-sm">{inviting ? 'Creating…' : 'Create invitation'}</button>
      </form>}

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
