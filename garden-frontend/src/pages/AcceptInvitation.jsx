import { useState } from 'react'
import { Link, useSearchParams } from 'react-router-dom'
import { acceptInvitation } from '../api/auth'

export default function AcceptInvitation() {
  const [params] = useSearchParams(); const token = params.get('token')
  const [password, setPassword] = useState(''); const [error, setError] = useState(''); const [done, setDone] = useState(false)
  async function submit(event) { event.preventDefault(); setError(''); try { await acceptInvitation(token, password); setDone(true) } catch (response) { setError(response.response?.data?.message || 'This invitation cannot be accepted.') } }
  if (!token) return <main className="min-h-screen bg-gray-950 grid place-items-center text-gray-300">This invitation link is incomplete.</main>
  return <main className="min-h-screen bg-gray-950 grid place-items-center p-6"><form onSubmit={submit} className="w-full max-w-md bg-gray-800 border border-gray-700 rounded-xl p-6 space-y-5"><p className="text-green-400 font-semibold">Smart Garden</p><h1 className="text-2xl font-bold text-white">Create your account</h1>{done ? <p className="text-green-400">Account created. <Link to="/login" className="underline">Sign in now</Link>.</p> : <><p className="text-sm text-gray-400">Choose a password with at least 12 characters.</p>{error && <p className="rounded-lg bg-red-950/60 border border-red-900 text-red-300 p-3 text-sm">{error}</p>}<label className="block text-sm text-gray-300">Password<input type="password" autoComplete="new-password" minLength="12" required value={password} onChange={event => setPassword(event.target.value)} className="mt-1 w-full bg-gray-700 border border-gray-600 text-white rounded-lg px-3 py-2" /></label><button className="w-full bg-green-500 hover:bg-green-400 text-gray-950 font-semibold rounded-lg py-2">Create account</button></>}</form></main>
}
