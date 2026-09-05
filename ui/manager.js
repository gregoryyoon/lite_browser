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

function getHighlightText(bm) {
  if (bm.context?.selectedText && bm.context.selectedText.trim().length > 0) {
    return bm.context.selectedText.trim();
  }
  if (bm.context?.pageAnchor) {
    const anchor = bm.context.pageAnchor;
    if (!anchor.startsWith('(y:') && anchor.length > 5) {
      return anchor.replace(/\s*\(y:\d+\)$/, '').trim();
    }
  }
  return '';
}

function hasHighlightAnchor(bm) {
  return getHighlightText(bm).length > 0;
}

function renderSidebar() {
  const countAll = document.getElementById('count-all');
  const countRecent = document.getElementById('count-recent');
  const countHighlights = document.getElementById('count-highlights');
  const tagsContainer = document.getElementById('smart-tags-list');

  // Bento Top Stat Widgets
  const bentoTotal = document.getElementById('bento-stat-total');
  const bentoHighlights = document.getElementById('bento-stat-highlights');
  const bentoTagsCloud = document.getElementById('bento-top-tags');

  if (countAll) countAll.innerText = managerBookmarks.length;
  if (bentoTotal) bentoTotal.innerText = managerBookmarks.length;

  const now = Date.now();
  const threeDaysMs = 3 * 24 * 60 * 60 * 1000;
  const recentCount = managerBookmarks.filter(b => (now - (b.context?.createdAt || 0)) <= threeDaysMs).length;
  if (countRecent) countRecent.innerText = recentCount;

  const highlightCount = managerBookmarks.filter(hasHighlightAnchor).length;
  if (countHighlights) countHighlights.innerText = highlightCount;
  if (bentoHighlights) bentoHighlights.innerText = highlightCount;

  // Build Tag Map & Counts
  const tagCounts = {};
  managerBookmarks.forEach(bm => {
    (bm.extractedTags || []).forEach(tag => {
      tagCounts[tag] = (tagCounts[tag] || 0) + 1;
    });
  });

  const sortedTags = Object.keys(tagCounts).sort((a,b) => tagCounts[b] - tagCounts[a]);

  // Render Top 6 Tags in Bento Stat Cloud
  if (bentoTagsCloud) {
    bentoTagsCloud.innerHTML = '';
    if (sortedTags.length === 0) {
      bentoTagsCloud.innerHTML = '<span class="stat-sub">태그 없음</span>';
    } else {
      sortedTags.slice(0, 6).forEach(tag => {
        const pill = document.createElement('button');
        pill.className = 'bento-tag-pill' + (currentTag === tag ? ' active' : '');
        pill.onclick = () => selectTag(tag);
        pill.innerHTML = `<span>#${tag}</span><span class="badge">${tagCounts[tag]}</span>`;
        bentoTagsCloud.appendChild(pill);
      });
    }
  }

  if (tagsContainer) {
    tagsContainer.innerHTML = '';
    sortedTags.forEach(tag => {
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
      if (!hasHighlightAnchor(bm)) return false;
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
        const matchHighlight = getHighlightText(bm).toLowerCase().includes(q);
        if (!matchTitle && !matchUrl && !matchSnippet && !matchIntent && !matchTags && !matchHighlight) return false;
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

function getItemVisitCount(item) {
  if (!item) return 1;
  if (typeof item.visitCount === 'number' && item.visitCount > 0) {
    return item.visitCount;
  }
  if (Array.isArray(item.visitTimestamps) && item.visitTimestamps.length > 0) {
    return item.visitTimestamps.length;
  }
  return 1;
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
        const isHighlight = hasHighlightAnchor(bm);
        const card = document.createElement('div');
        card.className = 'bm-card' + (isHighlight ? ' bento-wide' : '');
        card.onclick = (e) => openBookmarkUrl(bm.url, e.ctrlKey);
        card.onauxclick = (e) => {
          if (e.button === 1) {
            e.preventDefault();
            openBookmarkUrl(bm.url, true);
          }
        };

        const thumbHtml = bm.thumbnailUrl 
          ? `<img src="${bm.thumbnailUrl}" class="card-thumb" onerror="this.outerHTML='<div class=\\'card-thumb-placeholder\\'>${(bm.title || 'B')[0]}</div>'">`
          : `<div class="card-thumb-placeholder">${(bm.title || 'B')[0]}</div>`;

        const tagsHtml = (bm.extractedTags || []).map(t => `<span class="card-tag">#${t}</span>`).join('');
        const intentHtml = bm.context?.searchIntent ? `<span class="card-intent">🔍 ${bm.context.searchIntent}</span>` : '';
        const timeAgo = formatTimeAgo(bm.context?.createdAt);
        const visitCount = getItemVisitCount(bm);
        
        const highlightText = getHighlightText(bm);
        const highlightHtml = highlightText ? `<div class="card-highlight-box" title="${highlightText}">✨ "${highlightText}"</div>` : '';

        card.innerHTML = `
          ${thumbHtml}
          <div class="card-content">
            <div class="card-header">
              <img src="${bm.faviconUrl || ''}" class="card-favicon" onerror="this.style.display='none'">
              <span class="card-title" title="${bm.title}">${bm.title}</span>
            </div>
            ${highlightHtml}
            <div class="card-snippet" title="${bm.textSnippet || ''}">${bm.textSnippet || '본문 요약이 없습니다.'}</div>
            <div class="card-meta-row">
              ${intentHtml}
              ${tagsHtml}
            </div>
          </div>
          <div class="card-footer">
            <span>${timeAgo} · 👁️ ${visitCount}회 방문</span>
            <div class="card-actions">
              <button class="card-action-btn btn-del" title="삭제" onclick="deleteBookmarkManager('${bm.id}', event)">
                <svg viewBox="0 0 24 24"><path d="M3 6h18"/><path d="M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6"/><path d="M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2"/><line x1="10" x2="10" y1="11" y2="17"/><line x1="14" x2="14" y1="11" y2="17"/></svg>
              </button>
            </div>
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
        row.onclick = (e) => openBookmarkUrl(bm.url, e.ctrlKey);
        row.onauxclick = (e) => {
          if (e.button === 1) {
            e.preventDefault();
            openBookmarkUrl(bm.url, true);
          }
        };
        const timeAgo = formatTimeAgo(bm.context?.createdAt);
        const visitCount = getItemVisitCount(bm);

        const highlightText = getHighlightText(bm);
        const highlightHtml = highlightText ? `<span class="list-row-highlight" title="${highlightText}">✨ "${highlightText}"</span>` : '';

        row.innerHTML = `
          <img src="${bm.faviconUrl || ''}" class="card-favicon" onerror="this.style.display='none'">
          <span class="list-row-title" title="${bm.title}">${bm.title}</span>
          ${highlightHtml}
          <span class="list-row-snippet" title="${bm.textSnippet || bm.url}">${bm.textSnippet || bm.url}</span>
          <span class="list-row-date" title="${timeAgo} · ${visitCount}회 방문">${timeAgo} · <svg viewBox="0 0 24 24" style="width:12px;height:12px;display:inline-block;stroke:currentColor;fill:none;stroke-width:2;vertical-align:-1px;margin-right:2px;"><path d="M2 12s3-7 10-7 10 7 10 7-3 7-10 7-10-7-10-7Z"/><circle cx="12" cy="12" r="3"/></svg>${visitCount}회</span>
          <button class="card-action-btn btn-del" title="삭제" onclick="deleteBookmarkManager('${bm.id}', event)">
            <svg viewBox="0 0 24 24"><path d="M18 6 6 18"/><path d="m6 6 12 12"/></svg>
          </button>
        `;
        listTable.appendChild(row);
      });
    }
  }
}

function openBookmarkUrl(url, newTab = false) {
  if (url) {
    if (newTab) {
      window.location.href = 'http://ui-action/new-tab?url=' + encodeURIComponent(url);
    } else {
      window.location.href = 'http://ui-action/load?url=' + encodeURIComponent(url);
    }
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
