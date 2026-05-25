import { NavLink } from 'react-router-dom'

const links = [
  { to: '/',             label: 'Dashboard' },
  { to: '/sensors',      label: 'Sensors' },
  { to: '/measurements', label: 'Measurements' },
  { to: '/admin',        label: 'Admin' },
]

export default function Navbar() {
  return (
    <nav className="bg-gray-900 border-b border-gray-700 px-6 py-3 flex items-center gap-6">
      <span className="text-green-400 font-bold text-lg mr-4">Smart Garden</span>
      {links.map(l => (
        <NavLink
          key={l.to}
          to={l.to}
          end={l.to === '/'}
          className={({ isActive }) =>
            isActive
              ? 'text-green-400 font-semibold'
              : 'text-gray-400 hover:text-white transition-colors'
          }
        >
          {l.label}
        </NavLink>
      ))}
    </nav>
  )
}
