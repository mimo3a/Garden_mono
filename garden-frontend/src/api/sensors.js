import axios from 'axios'

const api = axios.create({
  baseURL: import.meta.env.VITE_API_URL ?? '',
})

export const getSensors = () =>
  api.get('/api/sensors').then(r => r.data)

export const getSensorHistory = (deviceId, params = {}) =>
  api.get(`/api/sensors/${deviceId}/history`, { params }).then(r => r.data)

export const getLatestTemperature = (deviceId) =>
  api.get(`/api/sensors/${deviceId}/latest-temperature`).then(r => r.data)

export const updateSensor = (deviceId, data) =>
  api.put(`/api/sensors/${deviceId}`, data).then(r => r.data)

export const deleteSensor = (deviceId) =>
  api.delete(`/api/sensors/${deviceId}`)
