let managerBookmarks = [];
let currentFilter = 'all';
let currentTag = '';
let currentSearchQuery = '';
let currentTemporalDays = 0; // 0 = all
let currentLayout = 'card';

document.addEventListener('DOMContentLoaded', () => {
  requestLoadBookmarksV2();
});

function requestLoadBookmarksV2() {
  window.location.href = 'http://ui-action/load-bookmarks-v2';
}

window.loadBookmarksDataB64 = function(b64Str) {
  try {
    const jsonStr = decodeURIComponent(escape(atob(b64Str)));
    const data = JSON.parse(jsonStr);
    managerBookmarks = data.bookmarks || [];
  } catch(e) {
    console.error("Failed to load bookmarks in manager:", e);
    managerBookmarks = [];
  }
  renderDashboard();
};

function renderDashboard() {
  renderSidebar();
  renderMainView();
}

function renderSidebar() {
  const countAll = document.getElementById('count-all');
  const countRecent = document.getElementById('count-recent');
  const countHighlights = document.getElementById('count-highlights');
  const tagsContainer = document.getElementById('smart-tags-list');

  if (countAll) countAll.innerText = managerBookmarks.length;

  const now = Date.now();
  const threeDaysMs = 3 * 24 * 60 * 60 * 1000;
  const recentCount = managerBookmarks.filter(b => (now - (b.context?.createdAt || 0)) <= threeDaysMs).length;
  if (countRecent) countRecent.innerText = recentCount;

  const highlightCount = managerBookmarks.filter(b => b.context?.pageAnchor && b.context.pageAnchor.length > 5).length;
  if (countHighlights) countHighlights.innerText = highlightCount;

  // Build Tag Map & Counts
  const tagCounts = {};
  managerBookmarks.forEach(bm => {
    (bm.extractedTags || []).forEach(tag => {
      tagCounts[tag] = (tagCounts[tag] || 0) + 1;
    });
  });

  if (tagsContainer) {
    tagsContainer.innerHTML = '';
    Object.keys(tagCounts).sort((a,b) => tagCounts[b] - tagCounts[a]).forEach(tag => {
      const btn = document.createElement('button');
      btn.className = 'tag-nav-btn' + (currentTag === tag ? ' active' : '');
      btn.onclick = () => selectTag(tag);
      btn.innerHTML = `<span>#${tag}</span><span class="badge">${tagCounts[tag]}</span>`;
      tagsContainer.appendChild(btn);
    });
  }
}

function setQuickFilter(type) {
  currentFilter = type;
  currentTag = '';
  document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));
  const btn = document.querySelector(`.nav-item[onclick="setQuickFilter('${type}')"]`);
  if (btn) btn.classList.add('active');
  renderDashboard();
}

function selectTag(tag) {
  if (currentTag === tag) {
    currentTag = '';
  } else {
    currentTag = tag;
  }
  renderDashboard();
}

function handleSearchInput() {
  const input = document.getElementById('manager-search');
  if (input) {
    currentSearchQuery = input.value.trim();
    renderMainView();
  }
}

function handleTemporalSlider() {
  const slider = document.getElementById('temporal-slider');
  const text = document.getElementById('temporal-value');
  if (slider && text) {
    currentTemporalDays = parseInt(slider.value, 10);
    text.innerText = currentTemporalDays === 0 ? '전체 기간' : `최근 ${currentTemporalDays}일`;
    renderMainView();
  }
}

function switchLayout(layout) {
  currentLayout = layout;
  document.getElementById('view-card-btn').classList.toggle('active', layout === 'card');
  document.getElementById('view-list-btn').classList.toggle('active', layout === 'list');
  renderMainView();
}

function getFilteredBookmarks() {
  const now = Date.now();
  const thirtyDaysAgo = now - (30 * 24 * 60 * 60 * 1000);

  const filtered = managerBookmarks.filter(bm => {
    // Quick filter
    if (currentFilter === 'recent') {
      if ((now - (bm.context?.createdAt || 0)) > 3 * 24 * 60 * 60 * 1000) return false;
    } else if (currentFilter === 'highlights') {
      if (!bm.context?.pageAnchor || bm.context.pageAnchor.length <= 5) return false;
    }

    // Tag filter
    if (currentTag) {
      if (!bm.extractedTags || !bm.extractedTags.includes(currentTag)) return false;
    }

    // Temporal slider filter
    if (currentTemporalDays > 0) {
      const daysMs = currentTemporalDays * 24 * 60 * 60 * 1000;
      if ((now - (bm.context?.createdAt || 0)) > daysMs) return false;
    }

    // Search query
    if (currentSearchQuery) {
      const q = currentSearchQuery.toLowerCase();
      if (q.startsWith('#')) {
        const tagName = q.slice(1);
        if (!bm.extractedTags || !bm.extractedTags.some(t => t.toLowerCase().includes(tagName))) return false;
      } else {
        const matchTitle = (bm.title || '').toLowerCase().includes(q);
        const matchUrl = (bm.url || '').toLowerCase().includes(q);
        const matchSnippet = (bm.textSnippet || '').toLowerCase().includes(q);
        const matchIntent = (bm.context?.searchIntent || '').toLowerCase().includes(q);
        const matchTags = (bm.extractedTags || []).some(t => t.toLowerCase().includes(q));
        if (!matchTitle && !matchUrl && !matchSnippet && !matchIntent && !matchTags) return false;
      }
    }

    return true;
  });

  // Priority 1: 30-day visit count desc, Priority 2: Creation time desc
  filtered.sort((a, b) => {
    const visitsA = (a.visitTimestamps || []).filter(t => t >= thirtyDaysAgo).length;
    const visitsB = (b.visitTimestamps || []).filter(t => t >= thirtyDaysAgo).length;
    if (visitsB !== visitsA) {
      return visitsB - visitsA;
    }
    const timeA = a.context?.createdAt || a.createdAt || 0;
    const timeB = b.context?.createdAt || b.createdAt || 0;
    return timeB - timeA;
  });

  return filtered;
}

function renderMainView() {
  const filtered = getFilteredBookmarks();
  const cardGrid = document.getElementById('card-grid');
  const listTable = document.getElementById('list-table');
  const emptyState = document.getElementById('empty-state');

  if (filtered.length === 0) {
    if (cardGrid) cardGrid.classList.add('hide');
    if (listTable) listTable.classList.add('hide');
    if (emptyState) emptyState.classList.remove('hide');
    return;
  }

  if (emptyState) emptyState.classList.add('hide');

  if (currentLayout === 'card') {
    if (listTable) listTable.classList.add('hide');
    if (cardGrid) {
      cardGrid.classList.remove('hide');
      cardGrid.innerHTML = '';
      filtered.forEach(bm => {
        const card = document.createElement('div');
        card.className = 'bm-card';
        card.onclick = () => openBookmarkUrl(bm.url);

        const thumbHtml = bm.thumbnailUrl 
          ? `<img src="${bm.thumbnailUrl}" class="card-thumb" onerror="this.outerHTML='<div class=\\'card-thumb-placeholder\\'>${(bm.title || 'B')[0]}</div>'">`
          : `<div class="card-thumb-placeholder">${(bm.title || 'B')[0]}</div>`;

        const tagsHtml = (bm.extractedTags || []).map(t => `<span class="tag-chip">#${t}</span>`).join('');
        const intentHtml = bm.context?.searchIntent ? `<span class="intent-chip">🔍 ${bm.context.searchIntent}</span>` : '';
        const timeAgo = formatTimeAgo(bm.context?.createdAt);

        card.innerHTML = `
          ${thumbHtml}
          <div class="card-content">
            <div class="card-header">
              <img src="${bm.faviconUrl || ''}" class="card-favicon" onerror="this.style.display='none'">
              <span class="card-title" title="${bm.title}">${bm.title}</span>
            </div>
            <div class="card-snippet" title="${bm.textSnippet || ''}">${bm.textSnippet || '본문 요약이 없습니다.'}</div>
            <div class="card-meta-box">
              ${intentHtml}
              ${tagsHtml}
            </div>
          </div>
          <div class="card-footer">
            <span>${timeAgo}</span>
            <button class="card-delete-btn" title="삭제" onclick="deleteBookmarkManager('${bm.id}', event)">✕</button>
          </div>
        `;
        cardGrid.appendChild(card);
      });
    }
  } else {
    if (cardGrid) cardGrid.classList.add('hide');
    if (listTable) {
      listTable.classList.remove('hide');
      listTable.innerHTML = '';
      filtered.forEach(bm => {
        const row = document.createElement('div');
        row.className = 'list-row';
        row.onclick = () => openBookmarkUrl(bm.url);
        const timeAgo = formatTimeAgo(bm.context?.createdAt);

        row.innerHTML = `
          <img src="${bm.faviconUrl || ''}" class="card-favicon" onerror="this.style.display='none'">
          <span class="list-row-title">${bm.title}</span>
          <span class="list-row-snippet">${bm.textSnippet || bm.url}</span>
          <span class="list-row-date">${timeAgo}</span>
          <button class="card-delete-btn" title="삭제" onclick="deleteBookmarkManager('${bm.id}', event)">✕</button>
        `;
        listTable.appendChild(row);
      });
    }
  }
}

function openBookmarkUrl(url) {
  if (url) {
    window.location.href = 'http://ui-action/load?url=' + encodeURIComponent(url);
  }
}

function deleteBookmarkManager(id, event) {
  if (event) event.stopPropagation();
  managerBookmarks = managerBookmarks.filter(b => b.id !== id);
  saveBookmarksV2();
  renderDashboard();
}

function saveBookmarksV2() {
  const jsonStr = JSON.stringify({ bookmarks: managerBookmarks });
  const b64 = btoa(unescape(encodeURIComponent(jsonStr)));
  window.location.href = 'http://ui-action/save-bookmarks?data=' + encodeURIComponent(b64);
}

function formatTimeAgo(timestamp) {
  if (!timestamp) return '방금 전';
  const diff = Date.now() - timestamp;
  const minutes = Math.floor(diff / 60000);
  if (minutes < 60) return `${Math.max(1, minutes)}분 전`;
  const hours = Math.floor(minutes / 60);
  if (hours < 24) return `${hours}시간 전`;
  const days = Math.floor(hours / 24);
  return `${days}일 전`;
}
