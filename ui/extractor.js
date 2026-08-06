(function() {
  function getMeta(property) {
    const el = document.querySelector(`meta[property="${property}"], meta[name="${property}"]`);
    return el ? (el.getAttribute('content') || '').trim() : '';
  }

  function getFavicon() {
    const link = document.querySelector('link[rel~="icon"], link[rel~="shortcut icon"]');
    if (link && link.href) return link.href;
    return window.location.origin + '/favicon.ico';
  }

  function extractSearchIntent() {
    try {
      const ref = document.referrer;
      if (!ref) return '';
      const url = new URL(ref);
      const params = new URLSearchParams(url.search);
      return params.get('q') || params.get('query') || params.get('search_query') || params.get('wd') || '';
    } catch(e) {
      return '';
    }
  }

  function extractTopNouns() {
    const text = document.body ? document.body.innerText || '' : '';
    const words = text.match(/[가-힣a-zA-Z0-9]{2,}/g) || [];
    const stopWords = new Set(['http', 'https', 'www', 'com', 'org', 'net', 'that', 'this', 'with', 'from', 'have', 'more', 'about', 'there', 'their', 'which', 'would', 'could', 'should', '그리고', '하지만', '또한', '통해', '위해', '경우', '대한', '관한', '있는', '없는', '모든']);
    
    const freq = {};
    words.forEach(w => {
      const lower = w.toLowerCase();
      if (!stopWords.has(lower) && isNaN(lower) && lower.length >= 2) {
        freq[w] = (freq[w] || 0) + 1;
      }
    });

    return Object.keys(freq)
      .sort((a, b) => freq[b] - freq[a])
      .slice(0, 5);
  }

  const ogImage = getMeta('og:image') || getMeta('twitter:image');
  const ogDesc = getMeta('og:description') || getMeta('description');
  const keywords = getMeta('keywords').split(',').map(k => k.trim()).filter(Boolean);
  const themeColor = getMeta('theme-color') || '#0078d4';
  const topNouns = extractTopNouns();

  let bodySnippet = '';
  if (document.body) {
    bodySnippet = (document.body.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 200);
  }

  const selectionText = window.getSelection() ? window.getSelection().toString().trim() : '';

  const allTags = Array.from(new Set([...keywords, ...topNouns])).slice(0, 5);

  const payload = {
    url: window.location.href,
    title: document.title || window.location.href,
    faviconUrl: getFavicon(),
    thumbnailUrl: ogImage,
    themeColor: themeColor,
    extractedTags: allTags,
    textSnippet: ogDesc || bodySnippet,
    context: {
      searchIntent: extractSearchIntent(),
      createdAt: Date.now(),
      pageAnchor: selectionText ? `${selectionText} (y:${Math.round(window.scrollY)})` : `(y:${Math.round(window.scrollY)})`
    }
  };

  const jsonStr = JSON.stringify(payload);
  const b64 = btoa(unescape(encodeURIComponent(jsonStr)));
  window.location.href = 'http://ui-action/save-contextual-bookmark?data=' + encodeURIComponent(b64);
})();
