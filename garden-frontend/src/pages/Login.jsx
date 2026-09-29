import { useState } from 'react'
import { Navigate, useLocation } from 'react-router-dom'
import { useAuth } from '../auth/AuthContext'

export default function Login() {
  const { user, login } = useAuth()
  const location = useLocation()
  const [email, setEmail] = useState('')
  const [password, setPassword] = useState('')
  const [error, setError] = useState('')
  const [submitting, setSubmitting] = useState(false)
  if (user) return <Navigate to={location.state?.from?.pathname || '/'} replace />

  async function submit(event) {
    event.preventDefault(); setError(''); setSubmitting(true)
    try { await login(email, password) } catch { setError('Email or password is incorrect.') } finally { setSubmitting(false) }
  }

  return <main className="min-h-screen bg-gray-950 grid place-items-center p-6"><form onSubmit={submit} className="w-full max-w-md bg-gray-800 border border-gray-700 rounded-xl p-6 space-y-5"><div><p className="text-green-400 font-semibold">Smart Garden</p><h1 className="text-2xl font-bold text-white mt-1">Welcome back</h1><p className="text-sm text-gray-400 mt-1">Sign in to view and manage your garden.</p></div>{error && <p className="rounded-lg bg-red-950/60 border border-red-900 text-red-300 p-3 text-sm">{error}</p>}<label className="block text-sm text-gray-300">Email<input type="email" autoComplete="email" required value={email} onChange={event => setEmail(event.target.value)} className="mt-1 w-full bg-gray-700 border border-gray-600 text-white rounded-lg px-3 py-2" /></label><label className="block text-sm text-gray-300">Password<input type="password" autoComplete="current-password" required value={password} onChange={event => setPassword(event.target.value)} className="mt-1 w-full bg-gray-700 border border-gray-600 text-white rounded-lg px-3 py-2" /></label><button disabled={submitting} className="w-full bg-green-500 hover:bg-green-400 disabled:opacity-50 text-gray-950 font-semibold rounded-lg py-2">{submitting ? 'Signing in…' : 'Sign in'}</button></form></main>
}
