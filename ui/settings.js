// Settings Page Logic

document.addEventListener('DOMContentLoaded', () => {
  requestDefaultBrowserStatus();
});

// Auto refresh status when user returns focus to Lite Browser
window.addEventListener('focus', () => {
  requestDefaultBrowserStatus();
});

document.addEventListener('visibilitychange', () => {
  if (!document.hidden) {
    requestDefaultBrowserStatus();
  }
});

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
