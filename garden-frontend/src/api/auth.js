import { api } from './sensors'

export const getCsrf = () => api.get('/api/auth/csrf')
export const getMe = () => api.get('/api/auth/me').then(response => response.data)
export const login = (email, password) => api.post('/api/auth/login', { email, password }).then(response => response.data)
export const logout = () => api.post('/api/auth/logout')
export const acceptInvitation = (token, password) => api.post('/api/auth/accept-invitation', { token, password }).then(response => response.data)
export const createInvitation = (email, deviceIds) => api.post('/api/admin/invitations', { email, deviceIds }).then(response => response.data)
