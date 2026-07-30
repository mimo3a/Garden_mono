const LOCALE = 'de-AT'

// Backend returns LocalDateTime without timezone — append Z to treat as UTC
const toDate = ts => new Date(ts + 'Z')

export const formatDateTime = ts => toDate(ts).toLocaleString(LOCALE)
export const formatTime     = ts => toDate(ts).toLocaleTimeString(LOCALE)
