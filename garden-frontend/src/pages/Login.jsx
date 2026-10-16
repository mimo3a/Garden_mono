import { useState } from 'react'
import { Navigate, useLocation } from 'react-router-dom'
import { useAuth } from '../auth/AuthContext'

export default function Login() {
  const { user, login } = useAuth()
  const location = useLocation()
  const [email, setEmail] = useState('')
  const [password, setPassword] = useState('')
  const [passwordVisible, setPasswordVisible] = useState(false)
  const [error, setError] = useState('')
  const [submitting, setSubmitting] = useState(false)
  if (user) return <Navigate to={location.state?.from?.pathname || '/'} replace />

  async function submit(event) {
    event.preventDefault(); setError(''); setSubmitting(true)
    try {
      await login(email, password)
    } catch (requestError) {
      setError(requestError?.response?.status === 401
        ? 'Email or password is incorrect.'
        : 'Unable to sign in right now. Please try again shortly.')
    } finally { setSubmitting(false) }
  }

  return <main className="min-h-screen bg-gray-950 grid place-items-center p-6"><form onSubmit={submit} className="w-full max-w-md bg-gray-800 border border-gray-700 rounded-xl p-6 space-y-5"><div><p className="text-green-400 font-semibold">Smart Garden</p><h1 className="text-2xl font-bold text-white mt-1">Welcome back</h1><p className="text-sm text-gray-400 mt-1">Sign in to view and manage your garden.</p></div>{error && <p className="rounded-lg bg-red-950/60 border border-red-900 text-red-300 p-3 text-sm">{error}</p>}<label className="block text-sm text-gray-300">Email<input type="email" autoComplete="email" required value={email} onChange={event => setEmail(event.target.value)} className="mt-1 w-full bg-gray-700 border border-gray-600 text-white rounded-lg px-3 py-2" /></label><label className="block text-sm text-gray-300">Password<div className="relative mt-1"><input type={passwordVisible ? 'text' : 'password'} autoComplete="current-password" required value={password} onChange={event => setPassword(event.target.value)} className="w-full bg-gray-700 border border-gray-600 text-white rounded-lg px-3 py-2 pr-12" /><button type="button" onClick={() => setPasswordVisible(visible => !visible)} className="absolute inset-y-0 right-0 px-3 text-gray-400 hover:text-white" aria-label={passwordVisible ? 'Hide password' : 'Show password'} title={passwordVisible ? 'Hide password' : 'Show password'}><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" className="h-5 w-5" aria-hidden="true"><path d="M2 12s3.5-6 10-6 10 6 10 6-3.5 6-10 6S2 12 2 12Z" /><circle cx="12" cy="12" r="2.5" />{passwordVisible && <path d="m4 4 16 16" />}</svg></button></div></label><button disabled={submitting} className="w-full bg-green-500 hover:bg-green-400 disabled:opacity-50 text-gray-950 font-semibold rounded-lg py-2">{submitting ? 'Signing in…' : 'Sign in'}</button></form></main>
}
