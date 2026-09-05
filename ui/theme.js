/**
 * Lite Browser - Global Theme Manager Module (theme.js)
 * Supports: 'light', 'dark', 'system'
 * Features:
 *  - Instant synchronous DOM theme attribute initialization (prevents FOUC)
 *  - System OS prefers-color-scheme dynamic listener
 *  - Inter-browser IPC synchronization with C backend (set-theme via on_before_browse)
 *  - BroadcastChannel & storage event multi-layer instant sync across all tabs and frames
 *  - Window global callbacks: window.applyTheme(theme), window.setLiteTheme(theme)
 */
(function() {
  const THEME_STORAGE_KEY = 'lite_theme';
  let currentThemeMode = 'system'; // 'light' | 'dark' | 'system'
  let systemDarkMediaQuery = null;
  let themeChannel = null;

  try {
    if (typeof BroadcastChannel !== 'undefined') {
      themeChannel = new BroadcastChannel('lite_theme_broadcast');
      themeChannel.onmessage = function(e) {
        if (e.data && e.data.mode) {
          internalApplyTheme(e.data.mode, false);
        }
      };
    }
  } catch(e) {}

  window.addEventListener('storage', function(e) {
    if (e.key === THEME_STORAGE_KEY && e.newValue) {
      internalApplyTheme(e.newValue, false);
    }
  });

  function getSystemPreference() {
    return window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  }

  function resolveEffectiveTheme(mode) {
    if (mode === 'dark' || mode === 'light') {
      return mode;
    }
    return getSystemPreference();
  }

  function applyThemeToDOM(effectiveTheme, mode) {
    const root = document.documentElement;
    root.setAttribute('data-theme', effectiveTheme);
    root.setAttribute('data-theme-mode', mode);

    // Dispatch event for components that need custom re-rendering
    window.dispatchEvent(new CustomEvent('litethemechange', {
      detail: { effectiveTheme, mode }
    }));
  }

  function onSystemThemeChanged(e) {
    if (currentThemeMode === 'system') {
      const effectiveTheme = e.matches ? 'dark' : 'light';
      applyThemeToDOM(effectiveTheme, 'system');
    }
  }

  function internalApplyTheme(mode, broadcast = true) {
    if (mode !== 'dark' && mode !== 'light' && mode !== 'system') {
      mode = 'system';
    }
    currentThemeMode = mode;
    try {
      localStorage.setItem(THEME_STORAGE_KEY, mode);
    } catch (e) {}
    
    const effective = resolveEffectiveTheme(mode);
    applyThemeToDOM(effective, mode);

    if (broadcast && themeChannel) {
      try {
        themeChannel.postMessage({ mode: mode });
      } catch(e) {}
    }
  }

  function initTheme() {
    try {
      const saved = localStorage.getItem(THEME_STORAGE_KEY);
      if (saved === 'dark' || saved === 'light' || saved === 'system') {
        currentThemeMode = saved;
      } else {
        currentThemeMode = 'system';
      }
    } catch (e) {
      currentThemeMode = 'system';
    }

    const effective = resolveEffectiveTheme(currentThemeMode);
    applyThemeToDOM(effective, currentThemeMode);

    if (window.matchMedia) {
      systemDarkMediaQuery = window.matchMedia('(prefers-color-scheme: dark)');
      if (systemDarkMediaQuery.addEventListener) {
        systemDarkMediaQuery.addEventListener('change', onSystemThemeChanged);
      } else if (systemDarkMediaQuery.addListener) {
        systemDarkMediaQuery.addListener(onSystemThemeChanged);
      }
    }
  }

  // Exposed API
  window.getCurrentThemeMode = function() {
    return currentThemeMode;
  };

  window.getEffectiveTheme = function() {
    return resolveEffectiveTheme(currentThemeMode);
  };

  window.applyTheme = function(mode) {
    internalApplyTheme(mode, true);
  };

  window.setLiteTheme = function(mode) {
    window.applyTheme(mode);
    
    // Send to C backend IPC via on_before_browse navigation
    try {
      window.location.href = 'http://ui-action/set-theme?mode=' + encodeURIComponent(mode);
    } catch (e) {}
  };

  // Immediate execution on load
  initTheme();
})();
