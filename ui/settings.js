// Settings Page Logic

document.addEventListener('DOMContentLoaded', () => {
  initThemeUI();
  requestDefaultBrowserStatus();
  requestOptimizationMode();
});

// Auto refresh status when user returns focus to Lite Browser
window.addEventListener('focus', () => {
  requestDefaultBrowserStatus();
  requestOptimizationMode();
});

document.addEventListener('visibilitychange', () => {
  if (!document.hidden) {
    requestDefaultBrowserStatus();
    requestOptimizationMode();
  }
});

// ==========================================================================
// Theme Selection Logic
// ==========================================================================
function initThemeUI() {
  const currentMode = window.getCurrentThemeMode ? window.getCurrentThemeMode() : (localStorage.getItem('lite_theme') || 'system');
  renderThemeUI(currentMode);
}

function chooseTheme(mode) {
  if (mode !== 'light' && mode !== 'dark' && mode !== 'system') return;
  if (window.setLiteTheme) {
    window.setLiteTheme(mode);
  } else if (window.applyTheme) {
    window.applyTheme(mode);
  }
  renderThemeUI(mode);
}

function renderThemeUI(mode) {
  ['light', 'dark', 'system'].forEach(m => {
    const card = document.getElementById(`theme-card-${m}`);
    const radio = document.getElementById(`theme-radio-${m}`);
    const isActive = (m === mode);
    if (card) card.classList.toggle('active', isActive);
    if (radio) radio.checked = isActive;
  });

  const pill = document.getElementById('active-theme-pill');
  if (pill) {
    if (mode === 'light') pill.textContent = '라이트 모드 적용 중';
    else if (mode === 'dark') pill.textContent = '다크 모드 적용 중';
    else pill.textContent = '시스템 설정 동기화 중';
  }
}

window.addEventListener('litethemechange', (e) => {
  if (e.detail && e.detail.mode) {
    renderThemeUI(e.detail.mode);
  }
});

// ==========================================================================
// Default Browser Status Logic
// ==========================================================================
function requestDefaultBrowserStatus() {
  window.location.href = 'http://ui-action/get-default-browser-status';
}

function setAsDefaultBrowser() {
  const btn = document.getElementById('btn-set-default');
  if (btn) {
    btn.disabled = true;
    btn.querySelector('span').textContent = '설정 창 여는 중...';
  }

  window.location.href = 'http://ui-action/set-default-browser';

  setTimeout(() => {
    if (btn) {
      btn.disabled = false;
      btn.querySelector('span').textContent = '기본 브라우저로 설정';
    }
  }, 2000);
}

// Global callback invoked by CEF backend
window.updateDefaultBrowserStatus = function(isDefault) {
  const badge = document.getElementById('status-badge');
  const text = document.getElementById('status-text');
  const btn = document.getElementById('btn-set-default');

  if (!badge || !text) return;

  if (isDefault === 1) {
    badge.className = 'status-badge status-default';
    text.textContent = '현재 Windows의 기본 브라우저입니다';
    if (btn) {
      btn.disabled = true;
      btn.querySelector('span').textContent = '기본 브라우저 설정 완료';
    }
  } else {
    badge.className = 'status-badge status-not-default';
    text.textContent = '현재 기본 브라우저가 아닙니다';
    if (btn) {
      btn.disabled = false;
      btn.querySelector('span').textContent = '기본 브라우저로 설정';
    }
  }
};

// ==========================================================================
// Performance & Memory Optimization Logic
// ==========================================================================
let g_savedOptimizationMode = 'speed';
let g_launchOptimizationMode = 'speed';

function requestOptimizationMode() {
  window.location.href = 'http://ui-action/get-optimization-mode';
}

function selectOptimizationMode(mode) {
  if (mode !== 'speed' && mode !== 'memory') return;
  if (g_savedOptimizationMode === mode) return;
  g_savedOptimizationMode = mode;
  renderOptimizationUI(mode, g_launchOptimizationMode);
  window.location.href = 'http://ui-action/set-optimization-mode?mode=' + encodeURIComponent(mode);
}

function restartBrowser() {
  const btn = document.querySelector('.btn-restart');
  if (btn) {
    btn.disabled = true;
    btn.style.opacity = '0.7';
    btn.style.pointerEvents = 'none';
    const span = btn.querySelector('span');
    if (span) span.textContent = '재시작 중...';
  }
  window.location.href = 'http://ui-action/restart-browser';
}

// Global callback invoked by CEF backend for Optimization Mode
window.updateOptimizationMode = function(savedMode, launchMode) {
  if (!savedMode) savedMode = 'speed';
  if (!launchMode) launchMode = savedMode;

  g_savedOptimizationMode = savedMode;
  g_launchOptimizationMode = launchMode;

  renderOptimizationUI(savedMode, launchMode);
};

function renderOptimizationUI(savedMode, launchMode) {
  const isSpeed = (savedMode === 'speed');
  const speedRadio = document.getElementById('radio-speed');
  const memoryRadio = document.getElementById('radio-memory');
  const speedCard = document.getElementById('opt-card-speed');
  const memoryCard = document.getElementById('opt-card-memory');
  const speedStatus = document.getElementById('opt-speed-status');
  const memoryStatus = document.getElementById('opt-memory-status');
  const restartNotice = document.getElementById('opt-restart-notice');
  const infoMode = document.getElementById('info-opt-mode');

  if (speedRadio) speedRadio.checked = isSpeed;
  if (memoryRadio) memoryRadio.checked = !isSpeed;

  if (speedCard) {
    speedCard.classList.toggle('selected', isSpeed);
  }
  if (memoryCard) {
    memoryCard.classList.toggle('selected', !isSpeed);
  }

  // Update session status indicators on cards
  if (speedStatus) {
    if (launchMode === 'speed') {
      if (isSpeed) {
        speedStatus.className = 'opt-status active';
        speedStatus.innerHTML = '<span class="status-dot"></span><span class="opt-status-label">현재 세션 적용 중 (활성)</span>';
      } else {
        speedStatus.className = 'opt-status';
        speedStatus.innerHTML = '<span class="status-dot"></span><span class="opt-status-label">현재 세션 동작 중 (재시작 시 해제)</span>';
      }
    } else {
      if (isSpeed) {
        speedStatus.className = 'opt-status pending';
        speedStatus.innerHTML = '<span class="status-dot"></span><span class="opt-status-label">다음 재시작 시 적용 예정</span>';
      } else {
        speedStatus.className = 'opt-status';
        speedStatus.innerHTML = '<span class="status-dot"></span><span class="opt-status-label">미적용</span>';
      }
    }
  }

  if (memoryStatus) {
    if (launchMode === 'memory') {
      if (!isSpeed) {
        memoryStatus.className = 'opt-status active';
        memoryStatus.innerHTML = '<span class="status-dot"></span><span class="opt-status-label">현재 세션 적용 중 (활성)</span>';
      } else {
        memoryStatus.className = 'opt-status';
        memoryStatus.innerHTML = '<span class="status-dot"></span><span class="opt-status-label">현재 세션 동작 중 (재시작 시 해제)</span>';
      }
    } else {
      if (!isSpeed) {
        memoryStatus.className = 'opt-status pending';
        memoryStatus.innerHTML = '<span class="status-dot"></span><span class="opt-status-label">다음 재시작 시 적용 예정</span>';
      } else {
        memoryStatus.className = 'opt-status';
        memoryStatus.innerHTML = '<span class="status-dot"></span><span class="opt-status-label">미적용</span>';
      }
    }
  }

  // Show or hide restart notice banner
  if (restartNotice) {
    if (savedMode !== launchMode) {
      restartNotice.style.display = 'flex';
      const targetModeName = isSpeed ? '실행 속도 우선' : '메모리 절감 우선';
      const noticeText = restartNotice.querySelector('.restart-notice-text');
      if (noticeText) {
        noticeText.textContent = `'${targetModeName}' 모드로 변경되었습니다. 브라우저를 완전히 닫고 다시 실행하면 적용됩니다.`;
      }
    } else {
      restartNotice.style.display = 'none';
    }
  }

  // Update browser info grid
  if (infoMode) {
    const activeName = launchMode === 'speed' ? '실행 속도 우선' : '메모리 절감 우선';
    if (savedMode !== launchMode) {
      const nextName = isSpeed ? '실행 속도' : '메모리 절감';
      infoMode.textContent = `${activeName} (재시작 시: ${nextName} 적용)`;
    } else {
      infoMode.textContent = `${activeName} (세션 활성)`;
    }
  }
}
