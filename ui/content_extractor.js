/**
 * LiteBrowser - Chrome Gemini Style 5-Stage Content Extraction Pipeline
 * (Readability + Viewport Visual Scoring + DOM Noise Filtering + Markdown Serializer)
 */
(function(global) {
  'use strict';

  function ContentExtractor() {}

  // 1. Helper to extract meta tag contents
  ContentExtractor.getMeta = function(doc, property) {
    if (!doc) return '';
    const el = doc.querySelector(`meta[property="${property}"], meta[name="${property}"], meta[itemprop="${property}"]`);
    return el ? (el.getAttribute('content') || '').trim() : '';
  };

  // 2. Helper to check if element is visually rendered / visible
  ContentExtractor.isElementVisible = function(el, win) {
    if (!el || el.nodeType !== Node.ELEMENT_NODE) return true;
    try {
      const style = win ? win.getComputedStyle(el) : window.getComputedStyle(el);
      if (style.display === 'none') return false;
      if (style.visibility === 'hidden') return false;
      if (parseFloat(style.opacity || '1') < 0.05) return false;
      
      const rect = el.getBoundingClientRect();
      if (rect.width === 0 && rect.height === 0 && el.children.length === 0) {
        return false;
      }
      return true;
    } catch(e) {
      return true;
    }
  };

  // 3. Stage 1: Smart Frame & Container Candidate Evaluator
  ContentExtractor.findBestContentDoc = function() {
    const candidates = [];

    function evaluateDoc(doc, win, isIframe) {
      if (!doc) return;
      try {
        let score = 0;
        const textLen = (doc.body ? (doc.body.innerText || '').trim().length : 0);
        score += Math.min(textLen, 5000);

        // Platform-specific content containers
        const articleSelectors = [
          '.se-main-container', '.se_component_wrap', '#postViewArea', '.post-view',
          '#dic_area', '#articleBody', '.article_view', '.news_body',
          '.entry-content', '.post-content', '.article-body', '.article_body',
          'article', 'main', '[role="main"]', '#content', '.content-body', '#article'
        ];

        let matchedSelector = null;
        for (const sel of articleSelectors) {
          const el = doc.querySelector(sel);
          if (el && el.innerText && el.innerText.trim().length > 100) {
            score += 3000;
            matchedSelector = sel;
            break;
          }
        }

        // If inside Naver Blog / Cafe iframe, give massive priority
        if (isIframe && (matchedSelector || textLen > 200)) {
          score += 2000;
        }

        candidates.push({ doc, win, score, textLen, matchedSelector });

        // Recursively inspect child iframes
        const iframes = doc.querySelectorAll('iframe, frame');
        for (let i = 0; i < iframes.length; i++) {
          try {
            const ifr = iframes[i];
            const iDoc = ifr.contentDocument || (ifr.contentWindow && ifr.contentWindow.document);
            const iWin = ifr.contentWindow;
            if (iDoc) {
              evaluateDoc(iDoc, iWin, true);
            }
          } catch(err) {
            // Cross-origin iframe
          }
        }
      } catch(e) {}
    }

    evaluateDoc(document, window, false);

    candidates.sort((a, b) => b.score - a.score);
    return candidates.length > 0 ? candidates[0] : { doc: document, win: window, score: 0 };
  };

  // 4. Stage 2: Semantic & Non-Visual Noise Filter
  ContentExtractor.cleanDOMTree = function(rootEl, win) {
    if (!rootEl) return document.createElement('div');
    const clone = rootEl.cloneNode(true);

    // Tags to remove immediately
    const noiseTags = [
      'script', 'style', 'noscript', 'template', 'svg', 'canvas', 'iframe', 'frame',
      'nav', 'header', 'footer', 'aside', 'form', 'select', 'dialog', 'menu'
    ];
    noiseTags.forEach(tag => {
      clone.querySelectorAll(tag).forEach(el => el.remove());
    });

    // Class/ID noise patterns (ads, banners, social, widgets)
    const noiseClassPatterns = [
      '.ad', '.ads', '.advertisement', '.banner', '.sponsor', '.sponsored',
      '.social-share', '.sns-share', '.share-box', '.share-area',
      '.pagination', '.paging', '.page-nav',
      '.sidebar', '.side-bar', '.widget', '.popup', '.modal',
      '.footer', '.footer-area', '.header-nav', '.top-nav', '.btn-area'
    ];

    noiseClassPatterns.forEach(selector => {
      try {
        clone.querySelectorAll(selector).forEach(el => {
          // Protect main content container if selector accidentally matches a substring
          if (!el.matches('article, main, .se-main-container, #postViewArea, #dic_area, .entry-content, .article-body')) {
            el.remove();
          }
        });
      } catch(e) {}
    });

    return clone;
  };

  // 5. Stage 3: Readability Scoring & Best Node Isolator
  ContentExtractor.isolateArticleNode = function(doc, win) {
    if (!doc || !doc.body) return document.body;

    // Direct match for known high-quality content containers
    const prioritySelectors = [
      '.se-main-container', '#postViewArea', '.se_component_wrap',
      '#dic_area', '#articleBody', '.article_view', '.news_body',
      '.entry-content', '.post-content', '.article-body', '.article_body',
      'article', 'main', '[role="main"]', '#content'
    ];

    for (const sel of prioritySelectors) {
      const el = doc.querySelector(sel);
      if (el && el.innerText && el.innerText.trim().length > 100) {
        return el;
      }
    }

    // Heuristic Paragraph/Container scoring
    const candidates = Array.from(doc.querySelectorAll('div, section, article, main'));
    let bestNode = doc.body;
    let maxScore = -1;

    const vHeight = win ? win.innerHeight : window.innerHeight;
    const vWidth = win ? win.innerWidth : window.innerWidth;

    candidates.forEach(node => {
      if (!ContentExtractor.isElementVisible(node, win)) return;

      const text = (node.innerText || '').trim();
      if (text.length < 50) return;

      // Base score on text length and commas
      let score = text.length / 20.0;
      const commas = (text.match(/,/g) || []).length;
      score += commas * 2;

      // Link Density Penalty
      const links = node.querySelectorAll('a');
      let linkTextLen = 0;
      links.forEach(a => linkTextLen += (a.innerText || '').trim().length);
      const linkDensity = text.length > 0 ? (linkTextLen / text.length) : 0;
      if (linkDensity > 0.5) {
        score *= (1.0 - linkDensity);
      }

      // Viewport Center Heuristic
      try {
        const rect = node.getBoundingClientRect();
        if (rect.top >= 0 && rect.top <= vHeight * 1.5 && rect.width >= vWidth * 0.3) {
          score *= 1.3; // In-viewport center boost
        }
      } catch(e) {}

      // Paragraph density boost
      const pCount = node.querySelectorAll('p').length;
      score += pCount * 5;

      if (score > maxScore) {
        maxScore = score;
        bestNode = node;
      }
    });

    return bestNode;
  };

  // 6. Stage 4: Clean Markdown Serializer
  ContentExtractor.nodeToMarkdown = function(node) {
    if (!node) return '';

    let md = '';

    function serialize(el, depth) {
      if (!el) return;

      // Text node
      if (el.nodeType === Node.TEXT_NODE) {
        const text = el.textContent.replace(/\s+/g, ' ');
        md += text;
        return;
      }

      if (el.nodeType !== Node.ELEMENT_NODE) return;

      const tag = el.tagName.toLowerCase();

      // Heading tags
      if (/^h[1-6]$/.test(tag)) {
        const level = parseInt(tag[1], 10);
        const prefix = '#'.repeat(level) + ' ';
        const headingText = (el.innerText || el.textContent || '').trim();
        if (headingText) {
          md += '\n\n' + prefix + headingText + '\n\n';
        }
        return;
      }

      // Paragraph
      if (tag === 'p') {
        const pText = (el.innerText || el.textContent || '').trim();
        if (pText) {
          md += '\n\n';
          for (const child of el.childNodes) {
            serialize(child, depth + 1);
          }
          md += '\n\n';
        }
        return;
      }

      // Blockquote
      if (tag === 'blockquote') {
        const qText = (el.innerText || el.textContent || '').trim();
        if (qText) {
          md += '\n\n> ' + qText.replace(/\n+/g, '\n> ') + '\n\n';
        }
        return;
      }

      // Lists
      if (tag === 'ul' || tag === 'ol') {
        md += '\n\n';
        let idx = 1;
        for (const child of el.children) {
          if (child.tagName && child.tagName.toLowerCase() === 'li') {
            const itemText = (child.innerText || child.textContent || '').trim();
            if (itemText) {
              const marker = (tag === 'ol') ? `${idx++}. ` : '- ';
              md += marker + itemText + '\n';
            }
          }
        }
        md += '\n';
        return;
      }

      // Table support
      if (tag === 'table') {
        md += '\n\n';
        const rows = el.querySelectorAll('tr');
        if (rows.length > 0) {
          rows.forEach((tr, rIdx) => {
            const cells = tr.querySelectorAll('th, td');
            if (cells.length === 0) return;
            const rowText = Array.from(cells).map(c => (c.innerText || '').trim().replace(/\|/g, '\\|')).join(' | ');
            md += '| ' + rowText + ' |\n';
            if (rIdx === 0) {
              const sep = Array.from(cells).map(() => '---').join(' | ');
              md += '| ' + sep + ' |\n';
            }
          });
        }
        md += '\n';
        return;
      }

      // Preformatted / Code
      if (tag === 'pre' || tag === 'code') {
        const codeText = (el.innerText || el.textContent || '').trim();
        if (codeText) {
          md += '\n\n```\n' + codeText + '\n```\n\n';
        }
        return;
      }

      // Line break
      if (tag === 'br') {
        md += '\n';
        return;
      }

      // Horizontal rule
      if (tag === 'hr') {
        md += '\n\n---\n\n';
        return;
      }

      // Formatting
      if (tag === 'strong' || tag === 'b') {
        const bText = (el.innerText || el.textContent || '').trim();
        if (bText) md += ` **${bText}** `;
        return;
      }

      // Images
      if (tag === 'img') {
        const alt = el.alt || el.title || '';
        const src = el.src || el.getAttribute('data-src') || '';
        if (src && !src.startsWith('data:') && alt) {
          md += `\n![${alt}](${src})\n`;
        }
        return;
      }

      // Generic container recursion
      for (const child of el.childNodes) {
        serialize(child, depth + 1);
      }

      if (['div', 'section', 'article'].includes(tag)) {
        md += '\n';
      }
    }

    serialize(node, 0);

    // Clean up excessive newlines & spaces
    return md
      .replace(/[ \t]+/g, ' ')
      .replace(/\n{3,}/g, '\n\n')
      .trim();
  };

  // 7. Specialized Extractor for YouTube Video Watch Pages
  ContentExtractor.extractYouTube = function(doc, win) {
    try {
      const url = win ? win.location.href : (doc.location ? doc.location.href : window.location.href);
      if (!/(?:youtube\.com|youtu\.be)/i.test(url)) return null;

      // Extract Title
      const titleEl = doc.querySelector('h1.ytd-watch-metadata yt-formatted-string, #title h1, h1.title, ytd-video-primary-info-renderer #title, #title.ytd-watch-metadata');
      const title = (titleEl ? titleEl.innerText : '') || 
                    ContentExtractor.getMeta(doc, 'og:title') || 
                    doc.title.replace(/\s*-\s*YouTube$/i, '') || 'YouTube 동영상';

      // Extract Channel / Author
      const channelEl = doc.querySelector('#channel-name yt-formatted-string, #owner #channel-name a, ytd-channel-name a, #upload-info a, #owner-name a');
      const author = (channelEl ? channelEl.innerText : '') || 
                     ContentExtractor.getMeta(doc, 'author') || 'YouTube Channel';

      // Extract View Count & Date
      const infoEl = doc.querySelector('#info-container #info, ytd-watch-info-text #info, #info-text');
      const publishedTime = (infoEl ? infoEl.innerText.replace(/\n+/g, ' | ') : '') || 
                            ContentExtractor.getMeta(doc, 'article:published_time') || '';

      // Extract Thumbnail Image
      const image = ContentExtractor.getMeta(doc, 'og:image') || '';

      // Extract Description
      let descText = '';
      const descEl = doc.querySelector('#description-inner, #description-inline-expander, ytd-expandable-video-description-body-renderer #description-body, #description');
      if (descEl) {
        descText = (descEl.innerText || '').trim();
      } else {
        descText = ContentExtractor.getMeta(doc, 'og:description') || ContentExtractor.getMeta(doc, 'description') || '';
      }

      // Extract Chapters / Timestamps
      const chapters = [];
      const chapterEls = doc.querySelectorAll('ytd-macro-markers-list-item-renderer, ytd-chapter-renderer');
      chapterEls.forEach(el => {
        const timeEl = el.querySelector('#time, .macro-markers-time, #endpoint');
        const titleEl = el.querySelector('#title, .macro-markers-title');
        if (timeEl && titleEl) {
          const t = timeEl.innerText.trim();
          const h = titleEl.innerText.trim();
          if (t && h) chapters.push(`- \`${t}\` ${h}`);
        }
      });

      if (chapters.length === 0 && descText) {
        const timestampRegex = /(?:^|\n)\s*(\d{1,2}:\d{2}(?::\d{2})?)\s*[-–—:]?\s*([^\n\r]+)/g;
        let match;
        while ((match = timestampRegex.exec(descText)) !== null) {
          const t = match[1].trim();
          const h = match[2].trim().slice(0, 80);
          chapters.push(`- \`${t}\` ${h}`);
        }
      }

      // Format clean, high-density Markdown
      let md = `# 🎬 ${title}\n\n`;
      md += `- **채널**: ${author}\n`;
      if (publishedTime) md += `- **정보**: ${publishedTime}\n`;
      md += `- **URL**: ${url}\n\n`;

      if (chapters.length > 0) {
        md += `### ⏱️ 동영상 챕터 및 주요 타임라인\n`;
        md += chapters.slice(0, 30).join('\n') + `\n\n`;
      }

      if (descText) {
        const cleanDesc = descText.slice(0, 2500);
        md += `### 📝 동영상 상세 설명\n${cleanDesc}\n\n`;
      }

      // Extract Comments (if loaded in DOM)
      const commentHeader = doc.querySelector('ytd-comments-header-renderer #count, ytd-comments-header-renderer #title, yt-formatted-string.count-text')?.innerText?.trim() || '';
      const commentNodes = Array.from(doc.querySelectorAll('ytd-comment-thread-renderer, ytd-comment-view-model'));
      const comments = [];

      commentNodes.slice(0, 50).forEach(cNode => {
        const authorEl = cNode.querySelector('#author-text, #header-author, .ytd-comment-view-model #author-text');
        const textEl = cNode.querySelector('#content-text, yt-attributed-string#content-text, .yt-core-attributed-string');
        const voteEl = cNode.querySelector('#vote-count-middle, .ytd-comment-action-buttons-renderer span.yt-core-attributed-string');
        const timeEl = cNode.querySelector('#published-time-text a, .ytd-comment-view-model a.yt-core-attributed-string');

        const author = (authorEl ? authorEl.innerText : '').trim().replace(/^@/, '');
        const text = (textEl ? textEl.innerText : '').trim();
        const votes = (voteEl ? voteEl.innerText : '').trim();
        const time = (timeEl ? timeEl.innerText : '').trim();

        if (text) {
          let commentLine = `- **${author || '시청자'}**`;
          if (time) commentLine += ` (${time})`;
          if (votes) commentLine += ` [👍 ${votes}]`;
          commentLine += `: ${text}`;
          comments.push(commentLine);
        }
      });

      if (comments.length > 0) {
        md += `### 💬 시청자 댓글 목록 (${commentHeader || comments.length + '개 로딩됨'})\n`;
        md += comments.join('\n\n') + `\n\n`;
      } else if (commentHeader) {
        md += `### 💬 시청자 댓글\n${commentHeader} (댓글을 확인하려면 스크롤을 아래로 조금 더 내려주세요)\n\n`;
      }

      // Extract Interactive Buttons
      const buttons = Array.from(doc.querySelectorAll('button[aria-label], ytd-button-renderer button, #search-icon-legacy, .ytp-play-button'))
        .filter(el => ContentExtractor.isElementVisible(el, win))
        .slice(0, 20)
        .map(el => ({
          tag: 'button',
          text: (el.getAttribute('aria-label') || el.innerText || el.title || '').trim().slice(0, 40),
          id: el.id || '',
          selector: el.id ? ('#' + el.id) : (el.className ? ('.' + el.className.trim().split(/\s+/)[0]) : 'button')
        }))
        .filter(x => x.text.length > 0);

      // Search input
      const inputs = Array.from(doc.querySelectorAll('input#search, input[name="search_query"], textarea'))
        .filter(el => ContentExtractor.isElementVisible(el, win))
        .slice(0, 5)
        .map(el => ({
          tag: 'input',
          type: el.type || 'text',
          name: el.name || 'search',
          placeholder: el.placeholder || '검색',
          selector: el.id ? ('#' + el.id) : 'input#search'
        }));

      return {
        title,
        url,
        author,
        publishedTime,
        image,
        bodySnippet: md,
        markdown: md,
        buttons,
        inputs
      };
    } catch(e) {
      console.warn('[YouTube Extractor Warning]', e);
      return null;
    }
  };

  // 8. Stage 5: Full Pipeline Orchestrator
  ContentExtractor.extract = function() {
    try {
      // 0. Specialized YouTube Watch Page extraction
      const ytResult = ContentExtractor.extractYouTube(document, window);
      if (ytResult) {
        return ytResult;
      }

      // 1. Find best document & window (handling nested frames)
      const best = ContentExtractor.findBestContentDoc();
      const targetDoc = best.doc || document;
      const targetWin = best.win || window;

      // 2. Isolate core article node
      const articleNode = ContentExtractor.isolateArticleNode(targetDoc, targetWin);

      // 3. Clean noise from DOM clone
      const cleanedDOM = ContentExtractor.cleanDOMTree(articleNode, targetWin);

      // 4. Serialize to Markdown
      let markdownBody = ContentExtractor.nodeToMarkdown(cleanedDOM);
      if (!markdownBody || markdownBody.length < 50) {
        markdownBody = (cleanedDOM.innerText || (targetDoc.body ? targetDoc.body.innerText : '') || '').trim();
      }

      // Limit to 8,000 chars for optimal LLM token budget
      if (markdownBody.length > 8000) {
        markdownBody = markdownBody.slice(0, 8000) + '\n\n...(이하 생략)';
      }

      // 5. Metadata extraction
      const title = ContentExtractor.getMeta(targetDoc, 'og:title') || 
                    ContentExtractor.getMeta(document, 'og:title') || 
                    targetDoc.title || document.title || '';

      const author = ContentExtractor.getMeta(targetDoc, 'author') || 
                     ContentExtractor.getMeta(targetDoc, 'article:author') || 
                     ContentExtractor.getMeta(document, 'author') || '';

      const publishedTime = ContentExtractor.getMeta(targetDoc, 'article:published_time') || 
                            ContentExtractor.getMeta(document, 'article:published_time') || '';

      const image = ContentExtractor.getMeta(targetDoc, 'og:image') || 
                    ContentExtractor.getMeta(document, 'og:image') || '';

      // 6. Interactive buttons & inputs for AI Agent control
      const buttons = Array.from(targetDoc.querySelectorAll('button, a, input[type="submit"], [role="button"]'))
        .filter(el => ContentExtractor.isElementVisible(el, targetWin))
        .slice(0, 30)
        .map(el => ({
          tag: el.tagName.toLowerCase(),
          text: (el.innerText || el.value || '').trim().slice(0, 50),
          id: el.id || '',
          selector: el.id ? ('#' + el.id) : (el.className ? ('.' + el.className.trim().split(/\s+/)[0]) : el.tagName.toLowerCase())
        }))
        .filter(x => x.text.length > 0);

      const inputs = Array.from(targetDoc.querySelectorAll('input:not([type="hidden"]), textarea, select'))
        .filter(el => ContentExtractor.isElementVisible(el, targetWin))
        .slice(0, 20)
        .map(el => ({
          tag: el.tagName.toLowerCase(),
          type: el.type || '',
          name: el.name || '',
          placeholder: el.placeholder || '',
          selector: el.id ? ('#' + el.id) : (el.name ? ('[name="' + el.name + '"]') : (el.type ? ('input[type="' + el.type + '"]') : el.tagName.toLowerCase()))
        }));

      return {
        title,
        url: window.location.href,
        author,
        publishedTime,
        image,
        bodySnippet: markdownBody,
        markdown: markdownBody,
        buttons,
        inputs
      };
    } catch(e) {
      console.error('[ContentExtractor Error]', e);
      return {
        title: document.title || '',
        url: window.location.href,
        bodySnippet: (document.body ? document.body.innerText.slice(0, 4000) : ''),
        markdown: (document.body ? document.body.innerText.slice(0, 4000) : ''),
        buttons: [],
        inputs: []
      };
    }
  };

  // Expose to global
  global.ContentExtractor = ContentExtractor;

})(typeof window !== 'undefined' ? window : this);
