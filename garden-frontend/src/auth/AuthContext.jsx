import { createContext, useContext, useEffect, useState } from 'react'
import { getCsrf, getMe, login as loginRequest, logout as logoutRequest } from '../api/auth'

const AuthContext = createContext(null)

export function AuthProvider({ children }) {
  const [user, setUser] = useState(null)
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    getCsrf().then(() => getMe()).then(setUser).catch(() => setUser(null)).finally(() => setLoading(false))
  }, [])

  async function login(email, password) {
    const current = await loginRequest(email, password)
    setUser(current)
    return current
  }

  async function logout() {
    await logoutRequest()
    setUser(null)
  }

  return <AuthContext.Provider value={{ user, loading, login, logout }}>{children}</AuthContext.Provider>
}

export function useAuth() { return useContext(AuthContext) }
