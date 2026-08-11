// Download Manager Dashboard JS

let g_downloadsList = [];
let g_currentFilter = 'all';
let g_searchKeyword = '';

document.addEventListener('DOMContentLoaded', () => {
  requestDownloadsList();
  // Poll for active progress updates every 1 second as fallback
  setInterval(requestDownloadsList, 1000);
});

function requestDownloadsList() {
  window.location.href = 'http://ui-action/get-downloads';
}

// Global callback invoked by C backend via ExecuteJsOnBrowser
window.renderDownloads = function(items) {
  if (Array.isArray(items)) {
    g_downloadsList = items;
    updateCounts();
    renderList();
  }
};

function setFilter(filter) {
  g_currentFilter = filter;
  document.querySelectorAll('.dl-tab-btn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.filter === filter);
  });
  renderList();
}

function onSearchInput() {
  const input = document.getElementById('dl-search-input');
  g_searchKeyword = input ? input.value.toLowerCase().trim() : '';
  renderList();
}

function formatBytes(bytes) {
  if (!bytes || bytes <= 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

function formatSpeed(speed) {
  if (!speed || speed <= 0) return '0 B/s';
  return formatBytes(speed) + '/s';
}

function formatDate(timestamp) {
  if (!timestamp || timestamp <= 0) return '-';
  const d = new Date(timestamp * 1000);
  return d.toLocaleString('ko-KR');
}

function getFileTypeIconClass(filename) {
  if (!filename) return 'icon-default';
  const ext = filename.split('.').pop().toLowerCase();

  if (['exe', 'msi', 'bat', 'cmd'].includes(ext)) return 'icon-exe';
  if (['zip', 'rar', '7z', 'tar', 'gz', 'iso'].includes(ext)) return 'icon-zip';
  if (['pdf'].includes(ext)) return 'icon-pdf';
  if (['png', 'jpg', 'jpeg', 'gif', 'webp', 'svg', 'bmp', 'ico'].includes(ext)) return 'icon-img';
  if (['mp4', 'mkv', 'avi', 'mp3', 'wav', 'flac', 'aac', 'webm'].includes(ext)) return 'icon-media';
  if (['html', 'js', 'css', 'json', 'py', 'c', 'cpp', 'h', 'cs', 'java'].includes(ext)) return 'icon-code';
  if (['doc', 'docx', 'xls', 'xlsx', 'ppt', 'pptx', 'txt', 'hwp'].includes(ext)) return 'icon-doc';

  return 'icon-default';
}

function getFileExtensionBadge(filename) {
  if (!filename) return 'FILE';
  const ext = filename.split('.').pop().toUpperCase();
  return ext.length > 5 ? 'FILE' : ext;
}

function updateCounts() {
  let activeCount = 0;
  let completedCount = 0;
  let missingCount = 0;

  g_downloadsList.forEach(item => {
    if (item.is_in_progress) activeCount++;
    else if (item.is_complete && item.file_exists) completedCount++;
    if (!item.file_exists && (item.is_complete || !item.is_in_progress)) missingCount++;
  });

  document.getElementById('count-all').textContent = g_downloadsList.length;
  document.getElementById('count-active').textContent = activeCount;
  document.getElementById('count-completed').textContent = completedCount;
  document.getElementById('count-missing').textContent = missingCount;

  // Active summary banner
  const banner = document.getElementById('dl-summary-banner');
  const summaryText = document.getElementById('summary-text');

  if (activeCount > 0) {
    banner.classList.remove('hide');
    summaryText.textContent = `${activeCount}개 항목 다운로드 진행 중...`;
  } else {
    banner.classList.add('hide');
  }
}

function renderList() {
  const container = document.getElementById('dl-list-container');
  const emptyState = document.getElementById('dl-empty-state');

  const filtered = g_downloadsList.filter(item => {
    // Filter by tab
    if (g_currentFilter === 'active' && !item.is_in_progress) return false;
    if (g_currentFilter === 'completed' && (!item.is_complete || !item.file_exists)) return false;
    if (g_currentFilter === 'missing' && item.file_exists) return false;

    // Filter by search keyword
    if (g_searchKeyword) {
      const name = (item.file_name || '').toLowerCase();
      const url = (item.url || '').toLowerCase();
      if (!name.includes(g_searchKeyword) && !url.includes(g_searchKeyword)) return false;
    }
    return true;
  });

  if (filtered.length === 0) {
    container.innerHTML = '';
    emptyState.classList.remove('hide');
    return;
  }

  emptyState.classList.add('hide');
  let html = '';

  filtered.forEach(item => {
    const iconClass = getFileTypeIconClass(item.file_name);
    const extBadge = getFileExtensionBadge(item.file_name);
    const isMissing = !item.file_exists && !item.is_in_progress;

    let statusBadgeHtml = '';
    if (item.is_in_progress) {
      statusBadgeHtml = `<span class="badge badge-downloading">다운로드 중</span>`;
    } else if (isMissing) {
      statusBadgeHtml = `<span class="badge badge-missing">파일 없음 (삭제/이동됨)</span>`;
    } else if (item.is_complete) {
      statusBadgeHtml = `<span class="badge badge-completed">완료됨</span>`;
    } else if (item.is_canceled) {
      statusBadgeHtml = `<span class="badge badge-canceled">취소됨</span>`;
    } else {
      statusBadgeHtml = `<span class="badge badge-paused">일시 중지</span>`;
    }

    let progressHtml = '';
    if (item.is_in_progress) {
      const pct = item.percent_complete >= 0 ? item.percent_complete : 0;
      const speedStr = formatSpeed(item.current_speed);
      const recStr = formatBytes(item.received_bytes);
      const totStr = item.total_bytes > 0 ? formatBytes(item.total_bytes) : '알 수 없음';

      progressHtml = `
        <div class="dl-progress-box">
          <div class="dl-progress-bar-bg">
            <div class="dl-progress-bar-fill" style="width: ${pct}%;"></div>
          </div>
          <div class="dl-progress-text">
            <span>${recStr} / ${totStr} (${pct}%)</span>
            <span>속도: ${speedStr}</span>
          </div>
        </div>
      `;
    }

    let actionsHtml = '';
    if (item.is_in_progress) {
      actionsHtml += `<button class="btn-action" onclick="pauseDownload(${item.id})">일시중지</button>`;
      actionsHtml += `<button class="btn-action btn-danger" onclick="cancelDownload(${item.id})">취소</button>`;
    } else {
      if (item.is_complete && item.file_exists) {
        actionsHtml += `<button class="btn-action btn-primary" onclick="openFile('${escapeJsString(item.full_path)}')">열기 (실행)</button>`;
        actionsHtml += `<button class="btn-action" onclick="showInFolder('${escapeJsString(item.full_path)}')">폴더에서 보기</button>`;
        actionsHtml += `<button class="btn-action btn-danger" onclick="deleteFile(${item.id}, '${escapeJsString(item.full_path)}')">파일 삭제</button>`;
      } else if (isMissing) {
        actionsHtml += `<button class="btn-action" disabled title="파일이 디스크에 존재하지 않습니다">실행 불가</button>`;
      }
      actionsHtml += `<button class="btn-action" onclick="removeHistory(${item.id})">기록 제거</button>`;
    }
    actionsHtml += `<button class="btn-action" onclick="showDetailModal(${item.id})">정보</button>`;

    html += `
      <div class="dl-card ${isMissing ? 'missing-file' : ''}">
        <div class="dl-icon-box ${iconClass}">${extBadge}</div>
        <div class="dl-info-section">
          <div class="dl-title-row">
            <span class="dl-file-name" title="${escapeHtml(item.file_name)}">${escapeHtml(item.file_name)}</span>
            ${statusBadgeHtml}
          </div>
          <div class="dl-meta-row">
            <span>${formatBytes(item.total_bytes || item.received_bytes)}</span>
            <span>•</span>
            <span class="dl-url" title="${escapeHtml(item.url)}">${escapeHtml(item.url)}</span>
            <span>•</span>
            <span>${formatDate(item.start_time)}</span>
          </div>
          ${progressHtml}
        </div>
        <div class="dl-actions">
          ${actionsHtml}
        </div>
      </div>
    `;
  });

  container.innerHTML = html;
}

function escapeHtml(str) {
  if (!str) return '';
  return str.replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&#039;");
}

function escapeJsString(str) {
  if (!str) return '';
  return str.replace(/\\/g, "\\\\").replace(/'/g, "\\'");
}

// Action Callbacks (Using window.location.href = http://ui-action/...)

function openFile(path) {
  if (!path) return;
  window.location.href = `http://ui-action/download-open-file?path=${encodeURIComponent(path)}`;
}

function showInFolder(path) {
  if (!path) return;
  window.location.href = `http://ui-action/download-show-in-folder?path=${encodeURIComponent(path)}`;
}

function deleteFile(id, path) {
  if (confirm(`실제 파일을 삭제하시겠습니까?\n${path}`)) {
    window.location.href = `http://ui-action/download-delete-file?id=${id}&path=${encodeURIComponent(path)}`;
  }
}

function removeHistory(id) {
  window.location.href = `http://ui-action/download-remove-history?id=${id}`;
}

function clearDownloadsHistory() {
  if (confirm('완료 및 취소된 모든 다운로드 기록을 정리하시겠습니까? (실제 파일은 삭제되지 않습니다)')) {
    window.location.href = 'http://ui-action/download-clear-history';
  }
}

function openDownloadsFolder() {
  window.location.href = 'http://ui-action/download-show-in-folder?path=';
}

function pauseDownload(id) {
  window.location.href = `http://ui-action/download-pause?id=${id}`;
}

function resumeDownload(id) {
  window.location.href = `http://ui-action/download-resume?id=${id}`;
}

function cancelDownload(id) {
  window.location.href = `http://ui-action/download-cancel?id=${id}`;
}

// Modal Detail Information
function showDetailModal(id) {
  const item = g_downloadsList.find(x => x.id === id);
  if (!item) return;

  document.getElementById('info-file-name').textContent = item.file_name || '-';
  document.getElementById('info-file-path').textContent = item.full_path || '-';
  document.getElementById('info-url').textContent = item.url || '-';
  document.getElementById('info-file-size').textContent = formatBytes(item.total_bytes || item.received_bytes);
  document.getElementById('info-status').textContent = item.file_exists ? '정상 존재' : '파일 미존재 (삭제 또는 이동됨)';
  document.getElementById('info-start-time').textContent = formatDate(item.start_time);
  document.getElementById('info-mime').textContent = item.mime_type || '알 수 없음';

  document.getElementById('dl-detail-modal').classList.remove('hide');
}

function closeDetailModal() {
  document.getElementById('dl-detail-modal').classList.add('hide');
}
