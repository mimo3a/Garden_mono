import { NavLink } from 'react-router-dom'

const links = [
  { to: '/',             label: 'Dashboard' },
  { to: '/sensors',      label: 'Sensors' },
  { to: '/measurements', label: 'Measurements' },
  { to: '/admin',        label: 'Admin' },
]

export default function Navbar({ theme, onThemeChange }) {
  return (
    <nav className="garden-nav bg-gray-900 border-b border-gray-700 px-4 sm:px-6 py-3 flex items-center gap-4 sm:gap-6">
      <span className="garden-brand text-green-400 font-bold text-lg mr-1 sm:mr-4"><span aria-hidden="true">✦</span> Smart Garden</span>
      <div className="flex items-center gap-4 sm:gap-6 flex-1 overflow-x-auto">
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
      </div>
      <button
        type="button"
        onClick={onThemeChange}
        className="shrink-0 rounded-lg border border-gray-700 bg-gray-800 px-3 py-1.5 text-sm text-gray-300 hover:bg-gray-700 hover:text-white transition-colors"
        aria-label={theme === 'dark' ? 'Switch to light theme' : 'Switch to dark theme'}
        title={theme === 'dark' ? 'Switch to light theme' : 'Switch to dark theme'}
      >
        {theme === 'dark' ? '☀ Light' : '☾ Dark'}
      </button>
    </nav>
  )
}
