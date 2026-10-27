/**
 * Comprehensive Unit Tests for YouTube Extractor Pipeline
 * Verifies:
 * 1. SSOT Memory Object Priority for Description and Chapters (Real y8vLZdiwWtA structure)
 * 2. 0-byte BotGuard timedtext defense & DOM transcript panel trigger (Tier 1 -> Tier 2)
 * 3. Accurate Tier 3 status briefing with registered tracks list (No false CC-absence claims)
 * 4. Empty-element wrapper trap fallback to meta tags
 * 5. Structured DOM chapters & description timestamp regex fallbacks
 */
const assert = require('assert');
const ContentExtractor = require('../ui/content_extractor.js');

console.log('--- [Test: YouTube Extractor Pipeline & Edge Cases] ---');

function createMockDoc({
  url = 'https://www.youtube.com/watch?v=y8vLZdiwWtA',
  meta = {},
  elements = {},
  queryMap = {}
} = {}) {
  return {
    location: { href: url, pathname: '/watch' },
    title: '치앙마이 럭셔리 호텔, 137 필라스 하우스는 어떨까? - YouTube',
    querySelector(selector) {
      if (queryMap[selector] !== undefined) return queryMap[selector];
      if (selector.startsWith('meta[')) {
        for (const [prop, val] of Object.entries(meta)) {
          if (selector.includes(`"${prop}"`)) {
            return { getAttribute: (attr) => (attr === 'content' ? val : null) };
          }
        }
        return null;
      }
      for (const [k, el] of Object.entries(elements)) {
        if (selector.includes(k)) return el;
      }
      return null;
    },
    querySelectorAll(selector) {
      if (queryMap[selector] !== undefined) return queryMap[selector];
      const results = [];
      for (const [k, arr] of Object.entries(elements)) {
        if (Array.isArray(arr) && selector.includes(k)) {
          results.push(...arr);
        }
      }
      return results;
    },
    getElementById(id) {
      return elements[id] || null;
    }
  };
}

// Real data structures from https://www.youtube.com/watch?v=y8vLZdiwWtA
const sampleY8vLZdiwWtAPlayerResponse = {
  videoDetails: {
    title: '치앙마이 럭셔리 호텔, 137 필라스 하우스는 어떨까?',
    author: '화송',
    shortDescription: `137 Pillars House Chiang Mai
137 필라스 하우스 치앙마이

00:00 서론 
01:11 외관 & 로비
03:38 티크 하우스
04:36 수영장 
06:14 피트니스 & 박물관 
07:52 룸 
09:44 레스토랑 & 조식
12:57 스파
14:02 137 필라스 스위트 & 레지던스 방콕
14:39 룸

안녕하세요. 이번에는 
치앙마이에 있는 137 필라스 하우스를 다녀왔습니다.`
  },
  captions: {
    playerCaptionsTracklistRenderer: {
      captionTracks: [
        {
          name: { simpleText: '영어(미국) (자동 생성됨)' },
          languageCode: 'en-US',
          vssId: 'a.en-US',
          baseUrl: 'https://www.youtube.com/api/timedtext?v=y8vLZdiwWtA&kind=asr&lang=en-US'
        },
        {
          name: { simpleText: '한국어 (자동 생성됨)' },
          languageCode: 'ko',
          vssId: 'a.ko',
          baseUrl: 'https://www.youtube.com/api/timedtext?v=y8vLZdiwWtA&kind=asr&lang=ko'
        }
      ]
    }
  }
};

const sampleY8vLZdiwWtAInitialData = {
  engagementPanels: [
    {
      engagementPanelSectionListRenderer: {
        panelIdentifier: 'engagement-panel-macro-markers-description-chapters',
        content: {
          macroMarkersListRenderer: {
            contents: [
              { macroMarkersListItemRenderer: { title: { simpleText: '서론' }, timeDescription: { simpleText: '0:00' } } },
              { macroMarkersListItemRenderer: { title: { simpleText: '외관 & 로비' }, timeDescription: { simpleText: '1:11' } } },
              { macroMarkersListItemRenderer: { title: { simpleText: '티크 하우스' }, timeDescription: { simpleText: '3:38' } } },
              { macroMarkersListItemRenderer: { title: { simpleText: '수영장' }, timeDescription: { simpleText: '4:36' } } },
              { macroMarkersListItemRenderer: { title: { simpleText: '피트니스 & 박물관' }, timeDescription: { simpleText: '6:14' } } },
              { macroMarkersListItemRenderer: { title: { simpleText: '룸' }, timeDescription: { simpleText: '7:52' } } },
              { macroMarkersListItemRenderer: { title: { simpleText: '레스토랑 & 조식' }, timeDescription: { simpleText: '9:44' } } },
              { macroMarkersListItemRenderer: { title: { simpleText: '스파' }, timeDescription: { simpleText: '12:57' } } },
              { macroMarkersListItemRenderer: { title: { simpleText: '137 필라스 스위트 & 레지던스 방콕' }, timeDescription: { simpleText: '14:02' } } },
              { macroMarkersListItemRenderer: { title: { simpleText: '룸' }, timeDescription: { simpleText: '14:39' } } }
            ]
          }
        }
      }
    }
  ]
};

async function runTests() {
  // Test Case 1: Real y8vLZdiwWtA structure with 10 chapters & 0-byte timedtext BotGuard defense
  {
    // Mock global fetch returning 0-byte body for timedtext (BotGuard xpe)
    const originalFetch = global.fetch;
    global.fetch = async () => ({
      ok: true,
      status: 200,
      text: async () => ''
    });

    const mockWin = {
      location: { href: 'https://www.youtube.com/watch?v=y8vLZdiwWtA', pathname: '/watch' },
      ytInitialPlayerResponse: sampleY8vLZdiwWtAPlayerResponse,
      ytInitialData: sampleY8vLZdiwWtAInitialData
    };

    const mockDoc = createMockDoc({
      url: 'https://www.youtube.com/watch?v=y8vLZdiwWtA'
    });

    const result = await ContentExtractor.extractYouTube(mockDoc, mockWin);

    assert.ok(result, 'Extraction result must not be null');
    assert.strictEqual(result.title, '치앙마이 럭셔리 호텔, 137 필라스 하우스는 어떨까?');
    assert.strictEqual(result.author, '화송');

    // Verify 10 chapters parsed from engagementPanels
    assert.ok(result.markdown.includes('### ⏱️ 동영상 챕터 및 주요 타임라인'), 'Markdown must have chapters section');
    assert.ok(result.markdown.includes('- `0:00` 서론'));
    assert.ok(result.markdown.includes('- `1:11` 외관 & 로비'));
    assert.ok(result.markdown.includes('- `14:39` 룸'));
    const chapterMatches = result.markdown.match(/- `\d{1,2}:\d{2}`/g) || [];
    assert.strictEqual(chapterMatches.length, 10, 'Must extract exactly 10 chapters');

    // Verify shortDescription preserved with line breaks
    assert.ok(result.markdown.includes('137 Pillars House Chiang Mai'));
    assert.ok(result.markdown.includes('치앙마이에 있는 137 필라스 하우스를 다녀왔습니다.'));

    // Verify Tier 3 briefing: must NOT claim "자막 트랙(CC)이 없다"
    assert.strictEqual(result.markdown.includes('자막 트랙(CC)이 없어'), false, 'Must NOT falsely claim no CC track');
    assert.ok(result.markdown.includes('등록된 자막 트랙'), 'Must acknowledge registered caption tracks');
    assert.ok(result.markdown.includes('한국어 (자동 생성됨)'), 'Must mention registered Korean track');

    console.log('✓ Test 1 Passed: y8vLZdiwWtA 10 chapters + SSOT shortDescription + 0-byte timedtext BotGuard Tier 3 briefing verified');
    global.fetch = originalFetch;
  }

  // Test Case 2: Tier 2 DOM Searchable Transcript Panel Trigger & Collection
  {
    const originalFetch = global.fetch;
    // Tier 1 returns 0 bytes
    global.fetch = async () => ({
      ok: true,
      status: 200,
      text: async () => ''
    });

    let expandClicked = false;
    let transcriptBtnClicked = false;

    const mockSegments = [
      {
        querySelector(s) {
          if (s.includes('timestamp')) return { innerText: '0:01' };
          if (s.includes('text')) return { innerText: '안녕하세요 화송입니다.' };
          return null;
        }
      },
      {
        querySelector(s) {
          if (s.includes('timestamp')) return { innerText: '0:15' };
          if (s.includes('text')) return { innerText: '오늘은 137 필라스 하우스를 소개합니다.' };
          return null;
        }
      }
    ];

    let segmentsAvailable = false;

    const mockDoc = createMockDoc({
      url: 'https://www.youtube.com/watch?v=y8vLZdiwWtA',
      queryMap: {
        '#description-inline-expander #expand, #expand, tp-yt-paper-button#expand, ytd-text-inline-expander #expand': {
          click() { expandClicked = true; }
        },
        'ytd-video-description-transcript-section-renderer button, ytd-video-description-transcript-section-renderer ytd-button-renderer, button[aria-label*="스크립트"], button[aria-label*="대본"], button[aria-label*="Transcript" i]': {
          click() {
            transcriptBtnClicked = true;
            setTimeout(() => { segmentsAvailable = true; }, 50);
          }
        }
      }
    });

    // querySelectorAll dynamically returns mockSegments after click
    mockDoc.querySelectorAll = function(selector) {
      if (selector === 'ytd-transcript-segment-renderer') {
        return segmentsAvailable ? mockSegments : [];
      }
      return [];
    };

    const mockWin = {
      location: { href: 'https://www.youtube.com/watch?v=y8vLZdiwWtA', pathname: '/watch' },
      ytInitialPlayerResponse: sampleY8vLZdiwWtAPlayerResponse,
      ytInitialData: sampleY8vLZdiwWtAInitialData
    };

    const result = await ContentExtractor.extractYouTube(mockDoc, mockWin);

    assert.ok(expandClicked, 'Description expand button should be clicked');
    assert.ok(transcriptBtnClicked, 'Transcript button should be clicked');
    assert.ok(result.markdown.includes('### 🎙️ 동영상 자막 대본'), 'Transcript section header must be present');
    assert.ok(result.markdown.includes('[0:01] 안녕하세요 화송입니다.'));
    assert.ok(result.markdown.includes('[0:15] 오늘은 137 필라스 하우스를 소개합니다.'));

    console.log('✓ Test 2 Passed: Tier 2 DOM transcript panel trigger & segment extraction verified');
    global.fetch = originalFetch;
  }

  // Test Case 3: Empty-wrapper trap defense: empty #description-inner falls back to og:description
  {
    const mockDoc = createMockDoc({
      url: 'https://www.youtube.com/watch?v=test_empty_dom',
      meta: {
        'og:title': 'Sample Video Title',
        'og:description': 'Fallback description from og:description meta tag'
      },
      elements: {
        // Empty DOM wrapper element trap
        'description-inner': { innerText: '   \n\t   ' }
      }
    });

    const mockWin = {
      location: { href: 'https://www.youtube.com/watch?v=test_empty_dom', pathname: '/watch' }
    };

    const result = await ContentExtractor.extractYouTube(mockDoc, mockWin);
    assert.ok(result.markdown.includes('Fallback description from og:description meta tag'), 'Empty DOM element must fall back to meta description');

    console.log('✓ Test 3 Passed: Empty DOM description wrapper trap cleanly falls back to meta tag');
  }

  // Test Case 4: True absence of CC (tracks = null)
  {
    const mockDoc = createMockDoc({
      url: 'https://www.youtube.com/watch?v=no_cc_video',
      meta: {
        'og:title': 'No CC Video',
        'og:description': 'Video description without captions'
      }
    });

    const mockWin = {
      location: { href: 'https://www.youtube.com/watch?v=no_cc_video', pathname: '/watch' },
      ytInitialPlayerResponse: {
        videoDetails: { title: 'No CC Video', shortDescription: 'Video without captions' },
        captions: null
      }
    };

    const result = await ContentExtractor.extractYouTube(mockDoc, mockWin);
    assert.ok(result.markdown.includes('제공되는 자막 트랙(CC)이 없어'), 'When no CC tracks exist at all, note must be shown');

    console.log('✓ Test 4 Passed: Truly absent CC correctly informs user about lack of CC track');
  }

  // Test Case 5: Chapters 2nd Priority (DOM ytd-macro-markers-list-item-renderer)
  {
    const domChapter1 = {
      querySelector(s) {
        if (s.includes('time')) return { innerText: '00:00' };
        if (s.includes('title')) return { innerText: 'DOM Intro' };
        return null;
      }
    };
    const domChapter2 = {
      querySelector(s) {
        if (s.includes('time')) return { innerText: '02:30' };
        if (s.includes('title')) return { innerText: 'DOM Main' };
        return null;
      }
    };

    const mockDoc = createMockDoc({
      url: 'https://www.youtube.com/watch?v=dom_chapters',
      meta: { 'og:title': 'DOM Chapters Video' },
      elements: {
        'ytd-macro-markers-list-item-renderer': [domChapter1, domChapter2]
      }
    });

    const mockWin = {
      location: { href: 'https://www.youtube.com/watch?v=dom_chapters', pathname: '/watch' }
    };

    const result = await ContentExtractor.extractYouTube(mockDoc, mockWin);
    assert.ok(result.markdown.includes('- `00:00` DOM Intro'));
    assert.ok(result.markdown.includes('- `02:30` DOM Main'));

    console.log('✓ Test 5 Passed: DOM macroMarkersListItemRenderer chapters parsed correctly');
  }

  // Test Case 6: Chapters 3rd Priority (Description timestampRegex fallback)
  {
    const mockDoc = createMockDoc({
      url: 'https://www.youtube.com/watch?v=regex_chapters',
      meta: { 'og:title': 'Regex Chapters Video' }
    });

    const mockWin = {
      location: { href: 'https://www.youtube.com/watch?v=regex_chapters', pathname: '/watch' },
      ytInitialPlayerResponse: {
        videoDetails: {
          title: 'Regex Chapters Video',
          shortDescription: `Timeline of this video:
00:00 Introduction to the topic
03:45 Deep dive into features
11:20 Conclusion and recap`
        }
      }
    };

    const result = await ContentExtractor.extractYouTube(mockDoc, mockWin);
    assert.ok(result.markdown.includes('- `00:00` Introduction to the topic'));
    assert.ok(result.markdown.includes('- `03:45` Deep dive into features'));
    assert.ok(result.markdown.includes('- `11:20` Conclusion and recap'));

    console.log('✓ Test 6 Passed: Description regex chapters fallback parsed correctly');
  }

  // Test Case 7: Real-world Async Transcript Button Appearance (rendered 50ms after expandBtn click)
  {
    const originalFetch = global.fetch;
    global.fetch = async () => ({ ok: true, status: 200, text: async () => '' });

    let expandClicked = false;
    let transcriptBtnAvailable = false;
    let transcriptBtnClicked = false;
    let segmentsAvailable = false;

    const mockSegments = [
      {
        querySelector(s) {
          if (s.includes('timestamp')) return { innerText: '1:11' };
          if (s.includes('text')) return { innerText: '비율이 1:11인 점을 확인하세요.' };
          return null;
        }
      }
    ];

    const mockDoc = createMockDoc({
      url: 'https://www.youtube.com/watch?v=async_transcript_btn',
      queryMap: {
        '#description-inline-expander #expand, #expand, tp-yt-paper-button#expand, ytd-text-inline-expander #expand': {
          click() {
            expandClicked = true;
            // Transcript button is NOT in DOM immediately; it appears 50ms after expand click
            setTimeout(() => { transcriptBtnAvailable = true; }, 50);
          }
        }
      }
    });

    // querySelector dynamically returns transcript button only after expandBtn was clicked
    const origQuerySelector = mockDoc.querySelector.bind(mockDoc);
    mockDoc.querySelector = function(selector) {
      if (selector.includes('ytd-video-description-transcript-section-renderer') || selector.includes('스크립트')) {
        if (!transcriptBtnAvailable) return null;
        return {
          click() {
            transcriptBtnClicked = true;
            setTimeout(() => { segmentsAvailable = true; }, 50);
          }
        };
      }
      return origQuerySelector(selector);
    };

    mockDoc.querySelectorAll = function(selector) {
      if (selector === 'ytd-transcript-segment-renderer') {
        return segmentsAvailable ? mockSegments : [];
      }
      return [];
    };

    const mockWin = {
      location: { href: 'https://www.youtube.com/watch?v=async_transcript_btn', pathname: '/watch' },
      ytInitialPlayerResponse: sampleY8vLZdiwWtAPlayerResponse
    };

    const result = await ContentExtractor.extractYouTube(mockDoc, mockWin);

    assert.ok(expandClicked, 'Expand button must be clicked');
    assert.ok(transcriptBtnClicked, 'Async transcript button must be found and clicked');
    assert.ok(result.markdown.includes('비율이 1:11인 점을 확인하세요.'), 'Must extract transcript segments after async button click');
    // Also verify Flaw 3 fix: 1:11 was NOT erased from speech!
    assert.ok(result.markdown.includes('[1:11] 비율이 1:11인 점을 확인하세요.'), 'Spoken time within text must not be corrupted by naive replace');

    console.log('✓ Test 7 Passed: Async transcript button rendering & speech text integrity verified');
    global.fetch = originalFetch;
  }

  // Test Case 8: 100% Description Preservation without arbitrary truncation (> 3500 chars)
  {
    const longDesc = 'A'.repeat(1800) + '\n\nMIDDLE_SECTION\n\n' + 'B'.repeat(1800) + '\nEND_MARKER';
    const mockDoc = createMockDoc({
      url: 'https://www.youtube.com/watch?v=long_desc_video'
    });

    const mockWin = {
      location: { href: 'https://www.youtube.com/watch?v=long_desc_video', pathname: '/watch' },
      ytInitialPlayerResponse: {
        videoDetails: {
          title: 'Long Description Video',
          shortDescription: longDesc
        }
      }
    };

    const result = await ContentExtractor.extractYouTube(mockDoc, mockWin);
    assert.ok(result.markdown.includes('END_MARKER'), 'Description must not be truncated at 3000 chars');
    assert.strictEqual(result.description, longDesc, 'result.description must preserve 100% of the SSOT text');

    console.log('✓ Test 8 Passed: 100% Description text preservation (>3500 chars) verified');
  }

  // Test Case 9: Caption track name with runs format
  {
    const originalFetch = global.fetch;
    global.fetch = async () => ({ ok: true, status: 200, text: async () => '' });

    const mockDoc = createMockDoc({ url: 'https://www.youtube.com/watch?v=caption_runs' });
    const mockWin = {
      location: { href: 'https://www.youtube.com/watch?v=caption_runs', pathname: '/watch' },
      ytInitialPlayerResponse: {
        videoDetails: { title: 'Caption Runs Video', shortDescription: 'Desc' },
        captions: {
          playerCaptionsTracklistRenderer: {
            captionTracks: [
              {
                name: { runs: [{ text: '한국어 (특수 자막)' }] },
                languageCode: 'ko',
                baseUrl: 'https://www.youtube.com/api/timedtext?v=test'
              }
            ]
          }
        }
      }
    };

    const result = await ContentExtractor.extractYouTube(mockDoc, mockWin);
    assert.ok(result.markdown.includes('한국어 (특수 자막)'), 'Caption track name in runs format must be extracted in Tier 3');

    console.log('✓ Test 9 Passed: Caption track runs format successfully extracted');
    global.fetch = originalFetch;
  }

  // Test Case 10: DOM Chapter with parent <a id="endpoint"> wrapper
  {
    const domChapter = {
      querySelector(s) {
        // Mock standard YouTube DOM: #endpoint is a parent container, #time is inside it
        if (s.includes('time')) return { innerText: '05:40' };
        if (s.includes('title')) return { innerText: 'Chapter Under Endpoint' };
        return null;
      }
    };

    const mockDoc = createMockDoc({
      url: 'https://www.youtube.com/watch?v=endpoint_chapter',
      elements: {
        'ytd-macro-markers-list-item-renderer': [domChapter]
      }
    });
    const mockWin = {
      location: { href: 'https://www.youtube.com/watch?v=endpoint_chapter', pathname: '/watch' }
    };

    const result = await ContentExtractor.extractYouTube(mockDoc, mockWin);
    assert.ok(result.markdown.includes('- `05:40` Chapter Under Endpoint'), 'DOM chapter must cleanly extract time and title without parent container contamination');

    console.log('✓ Test 10 Passed: DOM chapter extraction with parent wrapper verified');
  }

  // Test Case 11: youtu.be short URL support
  {
    const mockDoc = createMockDoc({
      url: 'https://youtu.be/y8vLZdiwWtA'
    });
    mockDoc.location = { href: 'https://youtu.be/y8vLZdiwWtA', pathname: '/y8vLZdiwWtA' };
    const mockWin = {
      location: { href: 'https://youtu.be/y8vLZdiwWtA', pathname: '/y8vLZdiwWtA' },
      ytInitialPlayerResponse: sampleY8vLZdiwWtAPlayerResponse
    };

    const result = await ContentExtractor.extractYouTube(mockDoc, mockWin);
    assert.ok(result !== null, 'youtu.be URL must be recognized by extractYouTube');
    assert.strictEqual(result.title, '치앙마이 럭셔리 호텔, 137 필라스 하우스는 어떨까?');

    console.log('✓ Test 11 Passed: youtu.be short URL support verified');
  }

  console.log('\nAll YouTube Extractor pipeline tests passed successfully!');
}

runTests().catch(err => {
  console.error('Test Failed:', err);
  process.exit(1);
});
