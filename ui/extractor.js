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

  // Helper to extract clean content text from a document or element (stripping UI noise like nav/footer/pagination)
  function getCleanBodyTextFromDocument(doc) {
    if (!doc || !doc.body) return '';

    // Clone body to avoid mutating active DOM
    const clone = doc.body.cloneNode(true);

    // 1. Remove UI Noise Elements
    const noiseSelectors = [
      'nav', 'header', 'footer', 'aside', 'script', 'style', 'noscript', 'button',
      '.pagination', '.paging', '.menu', '.nav', '.navbar', '.footer', '.header',
      '.sidebar', '.comment', '.comments', '.reply', '.reply-list', '.btn', '.btn-area',
      '#footer', '#header', '#sidebar', '#comments', '#menu', '.prev-post', '.next-post'
    ];
    noiseSelectors.forEach(sel => {
      clone.querySelectorAll(sel).forEach(el => el.remove());
    });

    // 2. Target main content container if available
    const mainContainer = clone.querySelector('article, main, .post-content, .article-body, .entry-content, #content, .se-viewer, .se-main-container');
    const targetEl = mainContainer || clone;

    return (targetEl.innerText || '').replace(/\s+/g, ' ').trim();
  }

  // Deep body text extraction (supports top window + child iframes like Naver Blog)
  function getDeepBodyText() {
    let texts = [];
    const mainText = getCleanBodyTextFromDocument(document);
    if (mainText) texts.push(mainText);

    const iframes = document.querySelectorAll('iframe');
    for (let i = 0; i < iframes.length; i++) {
      try {
        const iDoc = iframes[i].contentDocument || iframes[i].contentWindow.document;
        if (iDoc) {
          const iText = getCleanBodyTextFromDocument(iDoc);
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

  // Extract headings (h1, h2, h3, title, strong) for extra weighting
  function getHeadingText() {
    let headings = [document.title || ''];

    const selector = 'h1, h2, h3, .entry-title, .post-title, .se-title-text, strong, b';
    document.querySelectorAll(selector).forEach(el => {
      const txt = (el.innerText || '').trim();
      if (txt.length >= 2 && txt.length <= 100) {
        headings.push(txt);
      }
    });

    const iframes = document.querySelectorAll('iframe');
    for (let i = 0; i < iframes.length; i++) {
      try {
        const iDoc = iframes[i].contentDocument || iframes[i].contentWindow.document;
        if (iDoc) {
          iDoc.querySelectorAll(selector).forEach(el => {
            const txt = (el.innerText || '').trim();
            if (txt.length >= 2 && txt.length <= 100) {
              headings.push(txt);
            }
          });
        }
      } catch(e) {}
    }

    return headings.join(' ');
  }

  // Comprehensive Stopwords Set
  const stopWords = new Set([
    // Web UI / Navigation Stopwords
    '이전페이지', '다음페이지', '바로가기', '목록보기', '자세히보기', '전체보기', '페이지', '로그인',
    '회원가입', '검색어', '카테고리', '댓글', '공유하기', '저장', '수정', '삭제', '목록', '보기',
    '메뉴', '홈', '블로그', '네이버', '다음', '구글', '페이스북', '인스타그램', '유튜브', '트위터',
    '기사', '뉴스', '스마트', '스토어', '쇼핑', '아이콘', '이미지', '사진', '동영상', '첨부파일',
    '다운로드', '취소', '확인', '닫기', '열기', '이전', '다음', '검색', '글쓰기', '답글', '추천',
    
    // Generic Korean Functional / Grammar Words
    'http', 'https', 'www', 'com', 'org', 'net', '그리고', '하지만', '또한', '통해', '위해',
    '경우', '대한', '관한', '있는', '없는', '모든', '관련', '이유', '방법', '이후', '전체', '현재',
    '최근', '이것', '저것', '때문', '하나', '두개', '정도', '사용', '작성', '등록', '확인', '진행',
    '가능', '필요', '대해', '하여', '따라', '통해', '가지', '경우', '위한', '따른', '의해', '매우',

    // Generic English Stopwords
    'the', 'and', 'for', 'with', 'that', 'this', 'from', 'have', 'more', 'about', 'there',
    'their', 'which', 'would', 'could', 'should', 'your', 'what', 'some', 'other', 'into',
    'then', 'than', 'them', 'these', 'only', 'will', 'just', 'been', 'each', 'make', 'like'
  ]);

  function extractSmartTags(fullBodyText, headingText) {
    const freq = {};

    function addWords(text, weight) {
      if (!text) return;
      const words = text.match(/[가-힣a-zA-Z0-9]{2,}/g) || [];
      words.forEach(w => {
        const lower = w.toLowerCase();
        if (!stopWords.has(lower) && isNaN(lower) && lower.length >= 2) {
          freq[w] = (freq[w] || 0) + weight;
        }
      });
    }

    addWords(fullBodyText, 1);
    addWords(headingText, 4);

    return Object.keys(freq)
      .sort((a, b) => freq[b] - freq[a])
      .slice(0, 5);
  }

  // Smart Extractive Summarization (Sentence scoring based on importance)
  function extractSmartSummary(fullText, headingText, topKeywords) {
    if (!fullText) return '';

    // Split text into complete sentences
    const rawSentences = fullText.split(/(?<=[.!?\n])\s+/).map(s => s.trim()).filter(s => s.length >= 15 && s.length <= 250);
    if (rawSentences.length === 0) return fullText.slice(0, 200);

    const keywordSet = new Set(topKeywords.map(k => k.toLowerCase()));
    const headingWords = (headingText || '').toLowerCase().match(/[가-힣a-zA-Z0-9]{2,}/g) || [];
    const titleSet = new Set(headingWords);

    // Score sentences
    const scored = rawSentences.map((sent, index) => {
      let score = 0;

      // Position score (earlier sentences in main paragraphs score higher)
      score += Math.max(0, 10 - index * 1.2);

      // Sentence word importance
      const words = sent.toLowerCase().match(/[가-힣a-zA-Z0-9]{2,}/g) || [];
      words.forEach(w => {
        if (titleSet.has(w)) score += 3;
        if (keywordSet.has(w)) score += 2;
      });

      return { sent, score, index };
    });

    // Sort by score descending
    scored.sort((a, b) => b.score - a.score);

    // Select top scoring sentences and maintain original chronological flow
    let selected = scored.slice(0, 3);
    selected.sort((a, b) => a.index - b.index);

    let summary = '';
    for (let item of selected) {
      if ((summary + ' ' + item.sent).length > 220) break;
      summary += (summary ? ' ' : '') + item.sent;
    }

    return summary || rawSentences[0].slice(0, 200);
  }

  const ogImage = getMeta('og:image') || getMeta('twitter:image');
  const ogDesc = getMeta('og:description') || getMeta('description');
  const keywords = getMeta('keywords').split(',').map(k => k.trim()).filter(Boolean);
  const themeColor = getMeta('theme-color') || '#0078d4';
  
  const fullBodyText = getDeepBodyText();
  const headingText = getHeadingText();
  const smartNouns = extractSmartTags(fullBodyText, headingText);
  const smartSummary = extractSmartSummary(fullBodyText, headingText, smartNouns);

  const selectionText = getDeepSelectionText();
  const allTags = Array.from(new Set([...keywords, ...smartNouns])).slice(0, 5);

  const payload = {
    url: window.location.href,
    title: document.title || window.location.href,
    faviconUrl: getFavicon(),
    thumbnailUrl: ogImage,
    themeColor: themeColor,
    extractedTags: allTags,
    textSnippet: ogDesc || smartSummary,
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
