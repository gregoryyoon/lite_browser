function goBack() {
  window.location.href = 'http://ui-action/back';
}

function goForward() {
  window.location.href = 'http://ui-action/forward';
}

function reloadPage() {
  window.location.href = 'http://ui-action/reload';
}

function newTab() {
  window.location.href = 'http://ui-action/new-tab';
}

function switchTab(id) {
  window.location.href = 'http://ui-action/switch-tab?id=' + id;
}

function closeTab(id, event) {
  event.stopPropagation();
  window.location.href = 'http://ui-action/close-tab?id=' + id;
}

function detachTab(id) {
  window.location.href = 'http://ui-action/detach-tab?id=' + id;
}

function handleKey(event) {
  if (event.key === 'Enter') {
    let url = event.target.value.trim();
    if (url.length === 0) return;
    
    if (!/^https?:\/\//i.test(url) && !/^about:/i.test(url) && !/^file:\/\//i.test(url)) {
      if (url.indexOf('.') === -1 || url.indexOf(' ') !== -1) {
        url = 'https://www.google.com/search?q=' + encodeURIComponent(url);
      } else {
        url = 'https://' + url;
      }
    }
    
    window.location.href = 'http://ui-action/load?url=' + encodeURIComponent(url);
  }
}

// Global functions injected by the C++ backend
window.updateNavState = function(canGoBack, canGoForward, isLoading) {
  document.getElementById('back').disabled = !canGoBack;
  document.getElementById('forward').disabled = !canGoForward;
  
  const loader = document.getElementById('loader');
  if (isLoading) {
    loader.classList.remove('hide');
  } else {
    loader.classList.add('hide');
  }
};

window.updateAddress = function(url) {
  const addressBar = document.getElementById('address-bar');
  if (document.activeElement !== addressBar) {
    if (url.indexOf('ui/index.html') !== -1 || url.indexOf('ui-action') !== -1) {
      addressBar.value = '';
    } else {
      addressBar.value = url;
    }
  }
};

window.updateTabsList = function(tabs, activeId) {
  const container = document.getElementById('tabs');
  if (!container) return;

  container.innerHTML = '';
  tabs.forEach(tab => {
    const tabEl = document.createElement('div');
    tabEl.className = 'tab' + (tab.id === activeId ? ' active' : '');
    tabEl.draggable = false;
    
    tabEl.addEventListener('contextmenu', (e) => {
      e.preventDefault();
      e.stopPropagation();
      const x = Math.round(e.clientX);
      const y = Math.round(e.clientY);
      window.location.href = 'http://ui-action/show-tab-menu?id=' + tab.id + '&x=' + x + '&y=' + y;
    });
    
    const titleEl = document.createElement('span');
    titleEl.className = 'tab-title';
    titleEl.innerText = tab.title || '새 탭';
    titleEl.title = tab.title || '새 탭';
    tabEl.appendChild(titleEl);

    const closeEl = document.createElement('button');
    closeEl.className = 'tab-close';
    closeEl.innerHTML = '&times;';
    closeEl.onclick = (e) => closeTab(tab.id, e);
    closeEl.addEventListener('pointerdown', (e) => {
      e.stopPropagation();
    });
    tabEl.appendChild(closeEl);

    let isDragging = false;
    let startX = 0;
    let startY = 0;

    tabEl.addEventListener('pointerdown', (e) => {
      isDragging = false;
      startX = e.clientX;
      startY = e.clientY;
      tabEl.setPointerCapture(e.pointerId);
      document.body.style.cursor = 'grabbing';
      tabEl.style.cursor = 'grabbing';
    });

    tabEl.addEventListener('pointermove', (e) => {
      if (tabEl.hasPointerCapture(e.pointerId)) {
        const dx = e.clientX - startX;
        const dy = e.clientY - startY;
        if (Math.abs(dx) > 5 || Math.abs(dy) > 5) {
          isDragging = true;
        }

        if (isDragging) {
          const tabsBar = document.querySelector('.tabs-bar');
          if (tabsBar) {
            const rect = tabsBar.getBoundingClientRect();
            const x = e.clientX;
            const y = e.clientY;

            if (x < rect.left || x > rect.right || y < rect.top || y > rect.bottom) {
              document.body.style.cursor = 'copy';
              tabEl.style.cursor = 'copy';
            } else {
              document.body.style.cursor = 'grabbing';
              tabEl.style.cursor = 'grabbing';
            }
          }
        }
      }
    });

    tabEl.addEventListener('pointerup', (e) => {
      if (tabEl.hasPointerCapture(e.pointerId)) {
        tabEl.releasePointerCapture(e.pointerId);
        document.body.style.cursor = '';
        tabEl.style.cursor = '';

        if (isDragging) {
          window.location.href = 'http://ui-action/drag-end?id=' + tab.id;
        } else {
          switchTab(tab.id);
        }
      }
    });

    container.appendChild(tabEl);
  });
};

function toggleMenu(event) {
  event.stopPropagation();
  const btn = document.getElementById('menu-btn');
  if (btn) {
    const rect = btn.getBoundingClientRect();
    const x = Math.round(rect.left);
    const y = Math.round(rect.bottom);
    window.location.href = 'http://ui-action/show-menu?x=' + x + '&y=' + y;
  }
}

// 창 드래그 이동을 위한 마우스 리스너
document.addEventListener('DOMContentLoaded', () => {
  const tabsBar = document.querySelector('.tabs-bar');
  if (tabsBar) {
    tabsBar.addEventListener('mousedown', (e) => {
      const target = e.target;
      // .tab, .tab-btn, .win-control-btn 자식 요소를 클릭한 경우 드래그하지 않음
      if (target.closest('.tab') || target.closest('.tab-btn') || target.closest('.win-control-btn')) {
        return;
      }
      window.location.href = 'http://ui-action/drag-window';
    });
  }

  // 주소창 미포커스 시 클릭했을 때 전체 선택 동작 구현
  const addressBar = document.getElementById('address-bar');
  if (addressBar) {
    let isSelectAllOnFocus = false;
    addressBar.addEventListener('focus', () => {
      addressBar.select();
      isSelectAllOnFocus = true;
    });
    addressBar.addEventListener('mouseup', (e) => {
      if (isSelectAllOnFocus) {
        e.preventDefault();
        isSelectAllOnFocus = false;
      }
    });
    addressBar.addEventListener('blur', () => {
      isSelectAllOnFocus = false;
    });
  }
});

// 최소화, 최대화, 닫기 액션 디스패치 함수
function minimizeWindow() {
  window.location.href = 'http://ui-action/window-minimize';
}

function maximizeWindow() {
  window.location.href = 'http://ui-action/window-maximize';
}

function closeWindow() {
  window.location.href = 'http://ui-action/window-close';
}

// 최대화 상태에 따른 아이콘 갱신 함수 (백엔드 C 코드에서 호출)
window.updateMaximizeState = function(isMaximized) {
  const maxBtn = document.getElementById('win-max');
  if (maxBtn) {
    if (isMaximized) {
      // 겹쳐진 두 개의 사각형 (이전 크기로 복원 - 일반 메모장 스타일)
      maxBtn.innerHTML = '<svg viewBox="0 0 10 10"><path d="M2,0v2H0v8h8V8h2V0H2z M7,9H1V3h6V9z M9,7H8V2H3V1h6V7z"/></svg>';
      maxBtn.title = "이전 크기로 복원";
    } else {
      // 하나의 사각형 (최대화)
      maxBtn.innerHTML = '<svg viewBox="0 0 10 10"><path d="M0,0v10h10V0H0z M9,9H1V1h8V9z"/></svg>';
      maxBtn.title = "최대화";
    }
  }
};

function toggleEditor() {
  window.location.href = 'http://ui-action/toggle-editor';
}
