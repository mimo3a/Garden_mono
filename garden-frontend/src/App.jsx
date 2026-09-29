import { useEffect, useState } from 'react'
import { Navigate, Outlet, Routes, Route, useLocation } from 'react-router-dom'
import Navbar from './components/Navbar'
import Dashboard from './pages/Dashboard'
import SensorList from './pages/SensorList'
import SensorDetail from './pages/SensorDetail'
import Measurements from './pages/Measurements'
import Admin from './pages/Admin'
import Login from './pages/Login'
import AcceptInvitation from './pages/AcceptInvitation'
import LoadingSpinner from './components/LoadingSpinner'
import { useAuth } from './auth/AuthContext'

function Protected() {
  const { user, loading } = useAuth()
  const location = useLocation()
  if (loading) return <LoadingSpinner />
  return user ? <Outlet /> : <Navigate to="/login" state={{ from: location }} replace />
}

export default function App() {
  const { user, logout } = useAuth()
  const [theme, setTheme] = useState(() => localStorage.getItem('garden-theme') || 'dark')

  useEffect(() => { localStorage.setItem('garden-theme', theme) }, [theme])

  return (
    <div className={`garden-app min-h-screen bg-gray-950 text-white theme-${theme}`}>
      {user && <Navbar user={user} onLogout={logout} theme={theme} onThemeChange={() => setTheme(current => current === 'dark' ? 'light' : 'dark')} />}
      <main>
        <Routes>
          <Route path="/login" element={<Login />} />
          <Route path="/accept-invitation" element={<AcceptInvitation />} />
          <Route element={<Protected />}>
            <Route path="/" element={<Dashboard />} />
            <Route path="/sensors" element={<SensorList />} />
            <Route path="/sensors/:deviceId" element={<SensorDetail />} />
            <Route path="/measurements" element={<Measurements />} />
            <Route path="/admin" element={<Admin />} />
          </Route>
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </main>
    </div>
  )
}
