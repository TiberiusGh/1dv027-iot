;(() => {
  const STORAGE_KEY = 'theme-choice'
  const DARK_QUERY = '(prefers-color-scheme: dark)'

  const ICONS = {
    dark: '<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M4.93 19.07l1.41-1.41M17.66 6.34l1.41-1.41"/></svg>',
    light:
      '<svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/></svg>'
  }

  const readStoredTheme = () => {
    try {
      const stored = localStorage.getItem(STORAGE_KEY)
      return stored === 'light' || stored === 'dark' ? stored : null
    } catch {
      return null
    }
  }

  const systemTheme = () =>
    window.matchMedia(DARK_QUERY).matches ? 'dark' : 'light'

  const currentTheme = () =>
    document.documentElement.getAttribute('data-theme') || 'light'

  const applyTheme = (theme) => {
    document.documentElement.setAttribute('data-theme', theme)
  }

  // Pre-paint: set theme attribute before CSS paints to avoid a flash.
  applyTheme(readStoredTheme() ?? systemTheme())

  const setupToggle = () => {
    const btn = document.getElementById('theme-toggle')
    if (!btn) return

    const renderToggle = (theme) => {
      btn.innerHTML = ICONS[theme]
      const label =
        theme === 'dark' ? 'Switch to light mode' : 'Switch to dark mode'
      btn.setAttribute('aria-label', label)
      btn.setAttribute('title', label)
    }

    renderToggle(currentTheme())

    btn.addEventListener('click', () => {
      const next = currentTheme() === 'dark' ? 'light' : 'dark'
      applyTheme(next)
      renderToggle(next)
      try {
        localStorage.setItem(STORAGE_KEY, next)
      } catch {}
    })
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', setupToggle)
  } else {
    setupToggle()
  }
})()
