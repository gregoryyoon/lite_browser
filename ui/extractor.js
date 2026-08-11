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

  // Deep selection extraction (supports top window + child iframes like Naver Blog mainFrame)
  function getDeepSelectionText() {
    let sel = window.getSelection() ? window.getSelection().toString().trim() : '';
    if (sel) return sel;

    const iframes = document.querySelectorAll('iframe');
    for (let i = 0; i < iframes.length; i++) {
      try {
        const iWin = iframes[i].contentWindow;
        if (iWin) {
          const iSel = iWin.getSelection() ? iWin.getSelection().toString().trim() : '';
          if (iSel) return iSel;
        }
      } catch (e) {
        // Cross-origin iframe catch
      }
    }
    return '';
  }

  // Deep body text extraction (supports top window + child iframes like Naver Blog)
  function getDeepBodyText() {
    let texts = [];
    if (document.body) {
      const mainText = (document.body.innerText || '').replace(/\s+/g, ' ').trim();
      if (mainText) texts.push(mainText);
    }

    const iframes = document.querySelectorAll('iframe');
    for (let i = 0; i < iframes.length; i++) {
      try {
        const iDoc = iframes[i].contentDocument || iframes[i].contentWindow.document;
        if (iDoc && iDoc.body) {
          const iText = (iDoc.body.innerText || '').replace(/\s+/g, ' ').trim();
          if (iText && iText.length > 20) {
            texts.push(iText);
          }
        }
      } catch (e) {
        // Cross-origin iframe catch
      }
    }

    // Sort by text length descending to prioritize main content body
    texts.sort((a, b) => b.length - a.length);
    return texts[0] || '';
  }

  function extractTopNouns(fullBodyText) {
    const text = fullBodyText || (document.body ? document.body.innerText || '' : '');
    const words = text.match(/[가-힣a-zA-Z0-9]{2,}/g) || [];
    const stopWords = new Set(['http', 'https', 'www', 'com', 'org', 'net', 'that', 'this', 'with', 'from', 'have', 'more', 'about', 'there', 'their', 'which', 'would', 'could', 'should', '그리고', '하지만', '또한', '통해', '위해', '경우', '대한', '관한', '있는', '없는', '모든', '블로그', '네이버']);
    
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
  
  const fullBodyText = getDeepBodyText();
  const topNouns = extractTopNouns(fullBodyText);
  const bodySnippet = fullBodyText ? fullBodyText.slice(0, 200) : '';

  const selectionText = getDeepSelectionText();
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
      selectedText: selectionText,
      pageAnchor: selectionText ? `${selectionText} (y:${Math.round(window.scrollY)})` : `(y:${Math.round(window.scrollY)})`
    }
  };

  const jsonStr = JSON.stringify(payload);
  const b64 = btoa(unescape(encodeURIComponent(jsonStr)));
  window.location.href = 'http://ui-action/save-contextual-bookmark?data=' + encodeURIComponent(b64);
})();
