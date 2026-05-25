import { Routes, Route } from 'react-router-dom'
import Navbar from './components/Navbar'
import Dashboard from './pages/Dashboard'
import SensorList from './pages/SensorList'
import SensorDetail from './pages/SensorDetail'
import Measurements from './pages/Measurements'
import Admin from './pages/Admin'

export default function App() {
  return (
    <div className="min-h-screen bg-gray-950 text-white">
      <Navbar />
      <main>
        <Routes>
          <Route path="/"                    element={<Dashboard />} />
          <Route path="/sensors"             element={<SensorList />} />
          <Route path="/sensors/:deviceId"   element={<SensorDetail />} />
          <Route path="/measurements"        element={<Measurements />} />
          <Route path="/admin"               element={<Admin />} />
        </Routes>
      </main>
    </div>
  )
}
