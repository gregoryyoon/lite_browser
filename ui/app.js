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
    const dropdown = document.getElementById('omnibox-dropdown');
    const isDropdownOpen = dropdown && !dropdown.classList.contains('hide');
    if (isDropdownOpen && omniSelectedIndex >= 0) {
      return;
    }

    let url = event.target.value.trim();
    if (url.length === 0) return;
    
    if (!/^https?:\/\//i.test(url) && !/^about:/i.test(url) && !/^file:\/\//i.test(url) && !/^lite:\/\//i.test(url) && !/^edge:\/\//i.test(url)) {
      if (url.indexOf('.') === -1 || url.indexOf(' ') !== -1) {
        url = 'https://www.google.com/search?q=' + encodeURIComponent(url);
      } else {
        url = 'https://' + url;
      }
    }
    
    if (event.target && typeof event.target.blur === 'function') {
      event.target.blur();
    }
    closeOmniboxDropdown();
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
let historyData = {
  history: []
};
let currentUrl = '';
let currentTitle = '';

function requestLoadBookmarks() {
  window.location.href = 'http://ui-action/load-bookmarks';
  window.location.href = 'http://ui-action/load-history';
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

window.loadHistoryDataB64 = function(b64Str) {
  try {
    const jsonStr = decodeURIComponent(escape(atob(b64Str)));
    const payload = JSON.parse(jsonStr);
    historyData.history = payload.history || [];
  } catch (e) {
    console.error("Failed to parse history data:", e);
  }
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

function saveHistoryToBackend() {
  try {
    const jsonStr = JSON.stringify(historyData);
    const b64Str = btoa(unescape(encodeURIComponent(jsonStr)));
    window.location.href = 'http://ui-action/save-history?data=' + encodeURIComponent(b64Str);
  } catch (e) {
    console.error("Failed to save history:", e);
  }
}

function addHistoryEntry(url, title) {
  if (!url || !/^https?:\/\//i.test(url)) return;
  if (url.includes('ui-action') || url.includes('lite-browser') || url.includes('lite://') || url.includes('ui/')) return;

  const now = Date.now();
  const thirtyDaysAgo = now - (30 * 24 * 60 * 60 * 1000);
  const entryTitle = (title && title !== url) ? title : url;
  const existingIdx = (historyData.history || []).findIndex(h => h.url === url);

  if (existingIdx >= 0) {
    const item = historyData.history[existingIdx];
    item.visitedAt = now;
    if (title && title !== url) {
      item.title = title;
    }
    if (!item.visitTimestamps || !Array.isArray(item.visitTimestamps)) {
      item.visitTimestamps = [item.visitedAt || now];
    }
    item.visitTimestamps = item.visitTimestamps.filter(t => t >= thirtyDaysAgo);
    item.visitTimestamps.push(now);

    const updated = historyData.history.splice(existingIdx, 1)[0];
    historyData.history.unshift(updated);
  } else {
    historyData.history.unshift({
      id: 'hist_' + now,
      url: url,
      title: entryTitle,
      visitedAt: now,
      visitTimestamps: [now]
    });
  }

  if (historyData.history.length > 500) {
    historyData.history = historyData.history.slice(0, 500);
  }

  saveHistoryToBackend();

  // If this URL is a bookmark, update bookmark visit timestamps as well
  if (bookmarksData && bookmarksData.bookmarks) {
    const bm = bookmarksData.bookmarks.find(b => b.url === url);
    if (bm) {
      if (!bm.visitTimestamps || !Array.isArray(bm.visitTimestamps)) {
        bm.visitTimestamps = [];
      }
      bm.visitTimestamps = bm.visitTimestamps.filter(t => t >= thirtyDaysAgo);
      bm.visitTimestamps.push(now);
      saveBookmarksToBackend();
    }
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
    } else if (url.indexOf('ui/manager.html') !== -1 || url.indexOf('lite://favorites') !== -1 || url.indexOf('edge://favorites') !== -1) {
      addressBar.value = 'lite://favorites';
    } else {
      addressBar.value = url;
    }
  }
  updateStarIcon();
  if (url && /^https?:\/\//i.test(url)) {
    addHistoryEntry(url, currentTitle);
  }
};

window.updateTabsList = function(tabs, activeId) {
  const container = document.getElementById('tabs');
  if (!container) return;

  container.innerHTML = '';
  tabs.forEach(tab => {
    const isManager = tab.url && (tab.url.indexOf('ui/manager.html') !== -1 || tab.url.indexOf('lite://favorites') !== -1);
    if (tab.id === activeId) {
      currentTitle = isManager ? '즐겨찾기' : (tab.title || '');
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
    const displayTitle = isManager ? '즐겨찾기' : (tab.title || '새 탭');
    titleEl.innerText = displayTitle;
    titleEl.title = displayTitle;
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

function triggerContextualBookmark(event) {
  if (event) event.stopPropagation();

  const bm = findCurrentBookmark();
  if (bm) {
    // 이미 등록된 북마크인 경우 삭제
    bookmarksData.bookmarks = bookmarksData.bookmarks.filter(b => b.url !== currentUrl);
    saveBookmarksToBackend();
    updateStarIcon();
    return;
  }

  // 등록되지 않은 경우 메타데이터 수집 후 추가
  window.location.href = 'http://ui-action/extract-and-save-bookmark';
}

window.onContextualBookmarkExtractedB64 = function(b64Str) {
  try {
    const jsonStr = decodeURIComponent(escape(atob(b64Str)));
    const payload = JSON.parse(jsonStr);

    const existingIdx = bookmarksData.bookmarks.findIndex(b => b.url === payload.url);
    const newBm = {
      id: existingIdx >= 0 ? bookmarksData.bookmarks[existingIdx].id : ('bm_' + Date.now()),
      url: payload.url,
      title: payload.title || payload.url,
      faviconUrl: payload.faviconUrl || '',
      thumbnailUrl: payload.thumbnailUrl || '',
      themeColor: payload.themeColor || '#0078d4',
      extractedTags: payload.extractedTags || [],
      textSnippet: payload.textSnippet || '',
      context: payload.context || { createdAt: Date.now() },
      folder: '즐겨찾기 바'
    };

    if (existingIdx >= 0) {
      bookmarksData.bookmarks[existingIdx] = newBm;
    } else {
      bookmarksData.bookmarks.unshift(newBm);
    }

    saveBookmarksToBackend();
    updateStarIcon();
  } catch(e) {
    console.error("Failed to process extracted bookmark:", e);
  }
};

function showIngestionToast(bm) {
  const toast = document.getElementById('bookmark-toast-modal');
  if (!toast) return;

  document.getElementById('toast-title').innerText = bm.title || '북마크 추가됨';
  document.getElementById('toast-snippet').innerText = bm.textSnippet || bm.url;

  const thumbBox = document.getElementById('toast-thumb-box');
  if (bm.thumbnailUrl) {
    thumbBox.innerHTML = `<img src="${bm.thumbnailUrl}" onerror="this.outerHTML='<span>⭐</span>'">`;
  } else {
    thumbBox.innerHTML = `<span>⭐</span>`;
  }

  const intentEl = document.getElementById('toast-intent');
  if (bm.context && bm.context.searchIntent) {
    intentEl.innerText = `🔍 ${bm.context.searchIntent}`;
    intentEl.classList.remove('hide');
  } else {
    intentEl.classList.add('hide');
  }

  const tagsEl = document.getElementById('toast-tags');
  tagsEl.innerHTML = (bm.extractedTags || []).map(t => `<span class="tag-chip">#${t}</span>`).join('');

  toast.classList.remove('hide');
  expandUI(220);

  setTimeout(() => {
    closeToastModal();
  }, 2500);
}

function closeToastModal() {
  const toast = document.getElementById('bookmark-toast-modal');
  if (toast) toast.classList.add('hide');
  closeAllPopups();
}

function openBookmarkDashboard() {
  window.location.href = 'http://ui-action/open-bookmark-manager';
}

// ==================== SMART OMNIBOX ENGINE ====================

let omniSelectedIndex = -1;
let omniResults = [];
let omniHistoryResults = [];
let omniRawQuery = '';

document.addEventListener('DOMContentLoaded', () => {
  const addressBar = document.getElementById('address-bar');
  if (addressBar) {
    addressBar.addEventListener('input', handleOmniboxInput);
    addressBar.addEventListener('keydown', handleOmniboxKeydown);
    addressBar.addEventListener('blur', () => {
      setTimeout(() => {
        closeOmniboxDropdown();
      }, 200);
    });
  }
});

function formatTimeAgo(timestamp) {
  if (!timestamp) return '최근 저장';
  const diffMs = Date.now() - timestamp;
  const diffMin = Math.floor(diffMs / (1000 * 60));
  const diffHour = Math.floor(diffMs / (1000 * 60 * 60));
  const diffDay = Math.floor(diffMs / (1000 * 60 * 60 * 24));

  if (diffMin < 1) return '방금 전';
  if (diffMin < 60) return `${diffMin}분 전`;
  if (diffHour < 24) return `${diffHour}시간 전`;
  if (diffDay < 30) return `${diffDay}일 전`;
  const d = new Date(timestamp);
  return `${d.getFullYear()}.${d.getMonth() + 1}.${d.getDate()}`;
}

function handleOmniboxInput(e) {
  const rawValue = e.target.value;
  const query = rawValue.trim().toLowerCase();
  const dropdown = document.getElementById('omnibox-dropdown');
  if (!dropdown) return;

  omniRawQuery = rawValue.trim();

  if (!query) {
    closeOmniboxDropdown();
    return;
  }

  // 1. Multi-dimensional matching for Bookmarks
  if (query.startsWith('#')) {
    const tagName = query.slice(1);
    omniResults = bookmarksData.bookmarks.filter(b => 
      (b.extractedTags || []).some(t => t.toLowerCase().includes(tagName))
    );
    omniHistoryResults = [];
  } else {
    omniResults = bookmarksData.bookmarks.filter(b => {
      const matchTitle = (b.title || '').toLowerCase().includes(query);
      const matchUrl = (b.url || '').toLowerCase().includes(query);
      const matchSnippet = (b.textSnippet || '').toLowerCase().includes(query);
      const matchIntent = (b.context?.searchIntent || '').toLowerCase().includes(query);
      const matchTags = (b.extractedTags || []).some(t => t.toLowerCase().includes(query));
      return matchTitle || matchUrl || matchSnippet || matchIntent || matchTags;
    });

    // 2. Matching for Browsing History
    omniHistoryResults = (historyData.history || []).filter(h => {
      const matchTitle = (h.title || '').toLowerCase().includes(query);
      const matchUrl = (h.url || '').toLowerCase().includes(query);
      return matchTitle || matchUrl;
    });
  }

  // Sort Bookmarks: Priority 1 (30-day visit count desc), Priority 2 (createdAt desc)
  const now = Date.now();
  const thirtyDaysAgo = now - (30 * 24 * 60 * 60 * 1000);

  omniResults.sort((a, b) => {
    const visitsA = (a.visitTimestamps || []).filter(t => t >= thirtyDaysAgo).length;
    const visitsB = (b.visitTimestamps || []).filter(t => t >= thirtyDaysAgo).length;
    if (visitsB !== visitsA) {
      return visitsB - visitsA;
    }
    const timeA = a.context?.createdAt || a.createdAt || 0;
    const timeB = b.context?.createdAt || b.createdAt || 0;
    return timeB - timeA;
  });

  // Sort History: Priority 1 (30-day visit count desc), Priority 2 (visitedAt desc)
  omniHistoryResults.sort((a, b) => {
    const visitsA = (a.visitTimestamps || []).filter(t => t >= thirtyDaysAgo).length;
    const visitsB = (b.visitTimestamps || []).filter(t => t >= thirtyDaysAgo).length;
    if (visitsB !== visitsA) {
      return visitsB - visitsA;
    }
    const timeA = a.visitedAt || 0;
    const timeB = b.visitedAt || 0;
    return timeB - timeA;
  });

  omniResults = omniResults.slice(0, 3);
  omniHistoryResults = omniHistoryResults.slice(0, 3);
  omniSelectedIndex = -1;
  renderOmniboxDropdown();
  dropdown.classList.remove('hide');
  updateOmniboxHeight();
}

function updateOmniboxHeight() {
  const dropdown = document.getElementById('omnibox-dropdown');
  if (!dropdown || dropdown.classList.contains('hide')) return;

  requestAnimationFrame(() => {
    const dropHeight = dropdown.offsetHeight || dropdown.scrollHeight || 0;
    // 툴바 높이(72px) + 오프셋 + 실제 드롭다운 높이 + 여백(16px) -> 스크롤 없이 가변 높이 확장
    const targetHeight = Math.min(650, Math.max(90, 72 + dropHeight + 16));
    expandUI(targetHeight);
  });
}

function navigateToOmniboxUrl(targetUrl) {
  if (!targetUrl) return;
  const dropdown = document.getElementById('omnibox-dropdown');
  if (dropdown) dropdown.classList.add('hide');

  const addressBar = document.getElementById('address-bar');
  if (addressBar && document.activeElement === addressBar) {
    addressBar.blur();
  }

  window.location.href = 'http://ui-action/load?url=' + encodeURIComponent(targetUrl);
}

function renderOmniboxDropdown() {
  const dropdown = document.getElementById('omnibox-dropdown');
  if (!dropdown) return;

  dropdown.innerHTML = '';
  let globalIndex = 0;

  // 1. Render matched Bookmarks
  omniResults.forEach((bm) => {
    const itemIndex = globalIndex++;
    const item = document.createElement('div');
    item.className = 'omni-item' + (itemIndex === omniSelectedIndex ? ' selected' : '');
    
    const handleBookmarkSelect = (e) => {
      if (e) {
        e.preventDefault();
        e.stopPropagation();
      }
      navigateToOmniboxUrl(bm.url);
    };
    item.onmousedown = handleBookmarkSelect;
    item.onclick = handleBookmarkSelect;
    item.onmouseenter = () => {
      omniSelectedIndex = itemIndex;
      const allItems = dropdown.querySelectorAll('.omni-item, .omni-google-item, .omni-history-item');
      allItems.forEach((el, idx) => el.classList.toggle('selected', idx === omniSelectedIndex));
    };

    const tagsHtml = (bm.extractedTags || []).map(t => `<span class="omni-tag-chip">#${t}</span>`).join(' ');
    const timeAgoStr = formatTimeAgo(bm.context?.createdAt);
    const searchIntent = bm.context?.searchIntent;
    const snippetText = bm.textSnippet;

    item.innerHTML = `
      <div class="omni-row-top">
        <div class="omni-title-group">
          <span class="omni-icon">🔖</span>
          <span class="omni-badge">북마크</span>
          <span class="omni-title-text">${bm.title}</span>
        </div>
        <div class="omni-date">📅 ${timeAgoStr} 저장</div>
      </div>
      <div class="omni-row-meta">
        ${tagsHtml ? `<span class="omni-tags-list">🏷️ ${tagsHtml}</span>` : ''}
        ${searchIntent ? `<span class="omni-intent">💡 검색어: "${searchIntent}"</span>` : ''}
      </div>
      ${snippetText ? `<div class="omni-row-snippet">📄 요약: ${snippetText}</div>` : ''}
    `;
    dropdown.appendChild(item);
  });

  // 2. Render Google Search item at middle
  if (omniRawQuery) {
    const googleItemIndex = globalIndex++;
    const googleItem = document.createElement('div');
    googleItem.className = 'omni-google-item' + (omniSelectedIndex === googleItemIndex ? ' selected' : '');
    
    const handleGoogleSelect = (e) => {
      if (e) {
        e.preventDefault();
        e.stopPropagation();
      }
      const searchUrl = 'https://www.google.com/search?q=' + encodeURIComponent(omniRawQuery);
      navigateToOmniboxUrl(searchUrl);
    };
    googleItem.onmousedown = handleGoogleSelect;
    googleItem.onclick = handleGoogleSelect;
    googleItem.onmouseenter = () => {
      omniSelectedIndex = googleItemIndex;
      const allItems = dropdown.querySelectorAll('.omni-item, .omni-google-item, .omni-history-item');
      allItems.forEach((el, idx) => el.classList.toggle('selected', idx === omniSelectedIndex));
    };

    googleItem.innerHTML = `
      <span class="omni-icon">🔍</span>
      <span>구글 검색: "${omniRawQuery}"</span>
    `;
    dropdown.appendChild(googleItem);
  }

  // 3. Render matched Browsing History items (Option B layout)
  omniHistoryResults.forEach((hist) => {
    const histItemIndex = globalIndex++;
    const histItem = document.createElement('div');
    histItem.className = 'omni-history-item' + (histItemIndex === omniSelectedIndex ? ' selected' : '');
    
    const handleHistorySelect = (e) => {
      if (e) {
        e.preventDefault();
        e.stopPropagation();
      }
      navigateToOmniboxUrl(hist.url);
    };
    histItem.onmousedown = handleHistorySelect;
    histItem.onclick = handleHistorySelect;
    histItem.onmouseenter = () => {
      omniSelectedIndex = histItemIndex;
      const allItems = dropdown.querySelectorAll('.omni-item, .omni-google-item, .omni-history-item');
      allItems.forEach((el, idx) => el.classList.toggle('selected', idx === omniSelectedIndex));
    };

    const timeAgoStr = formatTimeAgo(hist.visitedAt);

    histItem.innerHTML = `
      <div class="omni-history-content">
        <span class="omni-icon">🌐</span>
        <span class="omni-history-badge">[방문 기록]</span>
        <span class="omni-history-title">${hist.title || hist.url}</span>
        <span class="omni-history-sep">-</span>
        <span class="omni-history-url">${hist.url}</span>
      </div>
      <div class="omni-history-date">📅 ${timeAgoStr} 방문</div>
    `;
    dropdown.appendChild(histItem);
  });

  updateOmniboxHeight();
}

function handleOmniboxKeydown(e) {
  const dropdown = document.getElementById('omnibox-dropdown');
  if (!dropdown || dropdown.classList.contains('hide')) return;

  const bookmarkCount = omniResults.length;
  const hasGoogle = omniRawQuery ? 1 : 0;
  const totalCount = bookmarkCount + hasGoogle + omniHistoryResults.length;
  if (totalCount === 0) return;

  if (e.key === 'ArrowDown') {
    e.preventDefault();
    omniSelectedIndex = (omniSelectedIndex + 1) % totalCount;
    renderOmniboxDropdown();
  } else if (e.key === 'ArrowUp') {
    e.preventDefault();
    omniSelectedIndex = (omniSelectedIndex - 1 + totalCount) % totalCount;
    renderOmniboxDropdown();
  } else if (e.key === 'Enter' && omniSelectedIndex >= 0) {
    e.preventDefault();
    const googleIndex = bookmarkCount;

    if (omniSelectedIndex < bookmarkCount) {
      const targetUrl = omniResults[omniSelectedIndex].url;
      navigateToOmniboxUrl(targetUrl);
    } else if (hasGoogle && omniSelectedIndex === googleIndex) {
      const searchUrl = 'https://www.google.com/search?q=' + encodeURIComponent(omniRawQuery);
      navigateToOmniboxUrl(searchUrl);
    } else {
      const histIdx = omniSelectedIndex - bookmarkCount - hasGoogle;
      if (histIdx >= 0 && histIdx < omniHistoryResults.length) {
        const targetUrl = omniHistoryResults[histIdx].url;
        navigateToOmniboxUrl(targetUrl);
      }
    }
  } else if (e.key === 'Escape') {
    closeOmniboxDropdown();
  }
}

function closeOmniboxDropdown() {
  const dropdown = document.getElementById('omnibox-dropdown');
  if (dropdown) dropdown.classList.add('hide');
  const addressBar = document.getElementById('address-bar');
  if (addressBar && document.activeElement === addressBar) {
    addressBar.blur();
  }
  closeAllPopups();
}

