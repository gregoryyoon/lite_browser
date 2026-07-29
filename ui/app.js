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

let bookmarksData = {
  folders: ["즐겨찾기 바", "기타 즐겨찾기"],
  bookmarks: []
};
let currentUrl = '';
let currentTitle = '';

function requestLoadBookmarks() {
  window.location.href = 'http://ui-action/load-bookmarks';
}

window.loadBookmarksDataB64 = function(b64Str) {
  try {
    const jsonStr = decodeURIComponent(escape(atob(b64Str)));
    bookmarksData = JSON.parse(jsonStr);
    if (!bookmarksData.folders) bookmarksData.folders = ["즐겨찾기 바", "기타 즐겨찾기"];
    if (!bookmarksData.bookmarks) bookmarksData.bookmarks = [];
  } catch (e) {
    console.error("Failed to parse bookmarks data:", e);
  }
  updateStarIcon();
};

function saveBookmarksToBackend() {
  try {
    const jsonStr = JSON.stringify(bookmarksData);
    const b64Str = btoa(unescape(encodeURIComponent(jsonStr)));
    window.location.href = 'http://ui-action/save-bookmarks?data=' + encodeURIComponent(b64Str);
  } catch (e) {
    console.error("Failed to save bookmarks:", e);
  }
}

function findCurrentBookmark() {
  if (!currentUrl) return null;
  return bookmarksData.bookmarks.find(b => b.url === currentUrl);
}

function updateStarIcon() {
  const starBtn = document.getElementById('bookmark-star-btn');
  if (!starBtn) return;
  const bm = findCurrentBookmark();
  if (bm) {
    starBtn.classList.add('bookmarked');
    starBtn.title = "즐겨찾기 편집";
  } else {
    starBtn.classList.remove('bookmarked');
    starBtn.title = "이 페이지를 즐겨찾기에 추가";
  }
}

window.updateAddress = function(url) {
  currentUrl = url;
  const addressBar = document.getElementById('address-bar');
  if (document.activeElement !== addressBar) {
    if (url.indexOf('ui/index.html') !== -1 || url.indexOf('ui-action') !== -1) {
      addressBar.value = '';
    } else {
      addressBar.value = url;
    }
  }
  updateStarIcon();
};

window.updateTabsList = function(tabs, activeId) {
  const container = document.getElementById('tabs');
  if (!container) return;

  container.innerHTML = '';
  tabs.forEach(tab => {
    if (tab.id === activeId) {
      currentTitle = tab.title || '';
    }
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
    const x = Math.round(rect.right);
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

  // Request initial bookmarks load from C backend
  requestLoadBookmarks();
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

// ==================== BOOKMARK FUNCTIONS ====================

function closeAllPopups() {
  const addPopup = document.getElementById('bookmark-add-popup');
  const listPopup = document.getElementById('bookmark-list-popup');
  const backdrop = document.getElementById('popup-backdrop');

  if (addPopup) addPopup.classList.add('hide');
  if (listPopup) listPopup.classList.add('hide');
  if (backdrop) backdrop.classList.add('hide');

  window.location.href = 'http://ui-action/collapse-ui';
}

function expandUI(height) {
  window.location.href = 'http://ui-action/expand-ui?height=' + height;
  const backdrop = document.getElementById('popup-backdrop');
  if (backdrop) backdrop.classList.remove('hide');
}

function toggleBookmarkAddPopup(event) {
  if (event) event.stopPropagation();

  const addPopup = document.getElementById('bookmark-add-popup');
  const listPopup = document.getElementById('bookmark-list-popup');
  if (!addPopup) return;

  if (!addPopup.classList.contains('hide')) {
    closeAllPopups();
    return;
  }

  if (listPopup) listPopup.classList.add('hide');

  const titleEl = document.getElementById('bm-popup-title');
  const nameInput = document.getElementById('bm-name-input');
  const folderSelect = document.getElementById('bm-folder-select');
  const removeBtn = document.getElementById('bm-remove-btn');

  folderSelect.innerHTML = '';
  bookmarksData.folders.forEach(f => {
    const opt = document.createElement('option');
    opt.value = f;
    opt.innerText = f;
    folderSelect.appendChild(opt);
  });

  const bm = findCurrentBookmark();
  if (bm) {
    if (titleEl) titleEl.innerText = '즐겨찾기 편집';
    if (nameInput) nameInput.value = bm.title;
    if (folderSelect) folderSelect.value = bm.folder || '즐겨찾기 바';
    if (removeBtn) removeBtn.style.display = 'inline-block';
  } else {
    const addressBar = document.getElementById('address-bar');
    const autoTitle = currentTitle || (addressBar ? addressBar.value : '페이지');
    const autoUrl = currentUrl || (addressBar ? addressBar.value : '');

    if (!autoUrl) return;

    const newBm = {
      id: 'bm_' + Date.now(),
      title: autoTitle,
      url: autoUrl,
      folder: '즐겨찾기 바',
      createdAt: Date.now()
    };
    bookmarksData.bookmarks.push(newBm);
    saveBookmarksToBackend();
    updateStarIcon();

    if (titleEl) titleEl.innerText = '즐겨찾기 추가됨';
    if (nameInput) nameInput.value = autoTitle;
    if (folderSelect) folderSelect.value = '즐겨찾기 바';
    if (removeBtn) removeBtn.style.display = 'inline-block';
  }

  addPopup.classList.remove('hide');
  expandUI(320);
}

function saveCurrentBookmark() {
  const nameInput = document.getElementById('bm-name-input');
  const folderSelect = document.getElementById('bm-folder-select');
  const bm = findCurrentBookmark();

  if (bm && nameInput && folderSelect) {
    bm.title = nameInput.value.trim() || bm.title;
    bm.folder = folderSelect.value;
    saveBookmarksToBackend();
    updateStarIcon();
  }
  closeAllPopups();
}

function removeCurrentBookmark() {
  const bm = findCurrentBookmark();
  if (bm) {
    bookmarksData.bookmarks = bookmarksData.bookmarks.filter(b => b.url !== currentUrl);
    saveBookmarksToBackend();
    updateStarIcon();
  }
  closeAllPopups();
}

function toggleBookmarkListPopup(event) {
  if (event) event.stopPropagation();

  const addPopup = document.getElementById('bookmark-add-popup');
  const listPopup = document.getElementById('bookmark-list-popup');
  if (!listPopup) return;

  if (!listPopup.classList.contains('hide')) {
    closeAllPopups();
    return;
  }

  if (addPopup) addPopup.classList.add('hide');

  renderBookmarksTree();
  listPopup.classList.remove('hide');
  expandUI(520);
}

function renderBookmarksTree(filterQuery = '') {
  const container = document.getElementById('bm-tree-container');
  if (!container) return;

  container.innerHTML = '';
  const query = filterQuery.toLowerCase().trim();

  let hasAnyItems = false;

  bookmarksData.folders.forEach(folderName => {
    const itemsInFolder = bookmarksData.bookmarks.filter(b => {
      if (b.folder !== folderName) return false;
      if (!query) return true;
      return (b.title && b.title.toLowerCase().includes(query)) || (b.url && b.url.toLowerCase().includes(query));
    });

    if (query && itemsInFolder.length === 0) return;

    hasAnyItems = true;
    const folderGroup = document.createElement('div');
    folderGroup.className = 'folder-group';

    const folderHeader = document.createElement('div');
    folderHeader.className = 'folder-header';
    folderHeader.innerHTML = `
      <svg class="folder-icon" viewBox="0 0 24 24"><path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.89 2 1.99 2H20c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"/></svg>
      <span>${folderName}</span>
    `;
    folderGroup.appendChild(folderHeader);

    if (itemsInFolder.length === 0) {
      const emptyDiv = document.createElement('div');
      emptyDiv.className = 'bm-empty-msg';
      emptyDiv.innerText = '즐겨찾기가 없습니다';
      folderGroup.appendChild(emptyDiv);
    } else {
      itemsInFolder.forEach(bm => {
        const itemEl = document.createElement('div');
        itemEl.className = 'bm-item';
        itemEl.onclick = (e) => {
          e.stopPropagation();
          window.location.href = 'http://ui-action/load?url=' + encodeURIComponent(bm.url);
          closeAllPopups();
        };

        itemEl.innerHTML = `
          <svg class="bm-favicon" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-1 17.93c-3.95-.49-7-3.85-7-7.93 0-.62.08-1.21.21-1.79L9 15v1c0 1.1.9 2 2 2v1.93zm6.9-2.54c-.26-.81-1-1.39-1.9-1.39h-1v-3c0-.55-.45-1-1-1H8v-2h2c.55 0 1-.45 1-1V7h2c1.1 0 2-.9 2-2v-.41c2.93 1.19 5 4.06 5 7.41 0 2.08-.8 3.97-2.1 5.39z"/></svg>
          <div class="bm-details">
            <div class="bm-title">${bm.title || '북마크'}</div>
            <div class="bm-url">${bm.url || ''}</div>
          </div>
          <div class="bm-item-actions">
            <button class="icon-btn" title="삭제" onclick="deleteBookmarkItem('${bm.id}', event)">
              <svg viewBox="0 0 24 24"><path d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"/></svg>
            </button>
          </div>
        `;
        folderGroup.appendChild(itemEl);
      });
    }

    container.appendChild(folderGroup);
  });

  if (!hasAnyItems) {
    const emptyDiv = document.createElement('div');
    emptyDiv.className = 'bm-empty-msg';
    emptyDiv.innerText = query ? '검색 결과가 없습니다' : '즐겨찾기가 없습니다';
    container.appendChild(emptyDiv);
  }
}

function filterBookmarksList() {
  const input = document.getElementById('bm-search-input');
  if (input) {
    renderBookmarksTree(input.value);
  }
}

function addNewFolderPrompt() {
  const folderName = prompt('새 즐겨찾기 폴더 이름을 입력하세요:', '새 폴더');
  if (folderName && folderName.trim()) {
    const name = folderName.trim();
    if (!bookmarksData.folders.includes(name)) {
      bookmarksData.folders.push(name);
      saveBookmarksToBackend();
      renderBookmarksTree();
    }
  }
}

function deleteBookmarkItem(bmId, event) {
  if (event) event.stopPropagation();
  bookmarksData.bookmarks = bookmarksData.bookmarks.filter(b => b.id !== bmId);
  saveBookmarksToBackend();
  updateStarIcon();
  filterBookmarksList();
}
