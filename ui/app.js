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

function newWindow() {
  window.location.href = 'http://ui-action/new-window';
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
    
    const titleEl = document.createElement('span');
    titleEl.className = 'tab-title';
    titleEl.innerText = tab.title || '새 탭';
    titleEl.title = tab.title || '새 탭';
    tabEl.appendChild(titleEl);

    const closeEl = document.createElement('button');
    closeEl.className = 'tab-close';
    closeEl.innerHTML = '&times;';
    closeEl.onclick = (e) => closeTab(tab.id, e);
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
