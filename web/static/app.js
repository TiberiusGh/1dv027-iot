const btn = document.getElementById('buzz')
const login = document.getElementById('login')
const status = document.getElementById('status')

async function init() {
  let creds
  try {
    const r = await fetch('/whoami', { credentials: 'include' })
    if (!r.ok) throw new Error('unauthorised')
    creds = await r.json()
  } catch {
    login.classList.remove('hidden')
    status.textContent = 'Charts public. Controls require sign-in.'
    return
  }

  btn.classList.remove('hidden')
  btn.disabled = true
  status.textContent = 'Connecting…'

  const client = mqtt.connect('wss://' + location.host + '/mqtt', {
    username: creds.user,
    password: creds.pass,
    clientId: 'web-' + Math.random().toString(36).slice(2, 10),
    reconnectPeriod: 3000
  })

  client.on('connect', () => {
    btn.disabled = false
    status.textContent = 'Ready.'
  })
  client.on('reconnect', () => {
    status.textContent = 'Reconnecting…'
  })
  client.on('error', (err) => {
    status.textContent = 'MQTT error: ' + err.message
  })
  client.on('close', () => {
    btn.disabled = true
  })

  btn.addEventListener('click', () => {
    client.publish('screams/cmd/buzz', '1')
    status.textContent = 'Buzz sent at ' + new Date().toLocaleTimeString()
  })
}

init()
