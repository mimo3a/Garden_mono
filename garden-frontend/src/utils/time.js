const LOCALE = 'de-AT'
const TZ     = 'Europe/Vienna'

// Backend returns LocalDateTime without timezone — append Z to treat as UTC
const toDate = ts => new Date(ts + 'Z')

export const formatDateTime = ts => toDate(ts).toLocaleString(LOCALE, { timeZone: TZ })
export const formatTime     = ts => toDate(ts).toLocaleTimeString(LOCALE, { timeZone: TZ })
