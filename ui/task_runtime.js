/**
 * Lite Browser - Task Runtime & State Machine Automator
 * Manages task breakdown, step-by-step execution, DOM highlighting,
 * error recovery (STUCK -> Retry -> WAITING), and user interventions.
 */

const TaskState = {
  IDLE: 'IDLE',
  RUNNING: 'RUNNING',
  PAUSED: 'PAUSED',
  STUCK: 'STUCK',
  WAITING: 'WAITING',
  DONE: 'DONE'
};

function sendNativeLog(msg) {
  console.log(msg);
  try {
    let bridge = document.getElementById('debug-log-bridge');
    if (!bridge) {
      bridge = document.createElement('iframe');
      bridge.id = 'debug-log-bridge';
      bridge.style.display = 'none';
      bridge.tabIndex = -1;
      bridge.setAttribute('tabindex', '-1');
      bridge.setAttribute('aria-hidden', 'true');
      document.body.appendChild(bridge);
    }
    bridge.src = 'http://ui-action/log?msg=' + encodeURIComponent(msg);
  } catch (e) {}
}
window.sendNativeLog = sendNativeLog;

function sendUiAction(url) {
  let bridge = document.getElementById('ui-action-bridge');
  if (!bridge) {
    bridge = document.createElement('iframe');
    bridge.id = 'ui-action-bridge';
    bridge.style.display = 'none';
    bridge.tabIndex = -1;
    bridge.setAttribute('tabindex', '-1');
    bridge.setAttribute('aria-hidden', 'true');
    document.body.appendChild(bridge);
  }
  bridge.src = url;
}
window.sendUiAction = sendUiAction;

class TaskRuntime {
  constructor() {
    this.state = TaskState.IDLE;
    this.currentTask = null;
    this.logs = [];
    this.steps = [];
    this.currentStepIndex = 0;
    this.isPaused = false;
    this.isInterrupted = false;
    this.onStateChange = null;
    this.onLog = null;
    this.onStepUpdate = null;
    this.pendingDomCallback = null;
    this.pendingViewportCallback = null;
    this.lastUserQuery = '';
  }

  setState(newState, detail = '') {
    this.state = newState;
    this.addLog(`상태 변경: [${newState}] ${detail}`);
    if (this.onStateChange) this.onStateChange(newState, detail);
  }

  addLog(message, type = 'info') {
    const logItem = {
      timestamp: new Date().toLocaleTimeString(),
      message,
      type
    };
    this.logs.push(logItem);
    if (this.onLog) this.onLog(logItem);
  }

  // Get standardized MCP browser tool definitions
  getAvailableTools() {
    return [
      {
        name: 'browser_navigate',
        description: '웹 브라우저의 현재 활성 탭을 지정한 URL로 이동시킵니다.',
        parameters: {
          type: 'object',
          properties: {
            url: { type: 'string', description: '이동할 URL (예: https://www.naver.com)' }
          },
          required: ['url']
        }
      },
      {
        name: 'browser_get_page_content',
        description: '현재 브라우저 활성 탭의 최신 실시간 URL, 웹페이지 제목, 상세 본문 텍스트(bodySnippet)를 읽어옵니다. 사용자가 대화 도중 페이지를 새로 이동했을 수 있으므로, 과거 대화에서 이미 확인했더라도 사용자가 다시 페이지/URL/요약을 요청할 때마다 반드시 새로 호출해야 합니다.',
        parameters: {
          type: 'object',
          properties: {
            format: { type: 'string', enum: ['summary', 'interactive_elements', 'full_text'] }
          }
        }
      },
      {
        name: 'browser_take_screenshot',
        description: '현재 웹 브라우저 활성 탭의 뷰포트 화면 전체를 스크린샷으로 실시간 캡처하여 관찰합니다. 차트, 캔버스 그래프, 대시보드 및 복합 시각 자료를 분석할 때 호출합니다.',
        parameters: {
          type: 'object',
          properties: {}
        }
      },
      {
        name: 'youtube_get_comments',
        description: '현재 YouTube 동영상 페이지에서 시청자들의 실시간 댓글 목록(작성자, 내용, 좋아요 수, 고정 여부)을 수집합니다. 사용자가 "댓글 분위기 알려줘", "시청자 반응 어때?", "인기 댓글 뭐야?" 등 댓글 및 여론 분석을 요청했을 때만 호출합니다. 일반 동영상 내용 요약 시에는 호출하지 마세요.',
        parameters: {
          type: 'object',
          properties: {
            limit: { type: 'number', description: '가져올 댓글 수 (기본값: 30, 최대: 50)' }
          }
        }
      },
      {
        name: 'browser_click_element',
        description: '사용자가 특정 버튼이나 링크를 클릭하라고 명시적으로 요청했을 때만 해당 요소를 마우스로 클릭합니다. 단순 URL/내용 조회 질문일 때는 절대 임의로 호출하지 마세요.',
        parameters: {
          type: 'object',
          properties: {
            selector: { type: 'string', description: 'CSS 셀렉터 (예: #search_btn, .btn_login)' },
            text: { type: 'string', description: '버튼에 적힌 텍스트' }
          }
        }
      },
      {
        name: 'browser_type_text',
        description: '사용자가 검색어나 텍스트를 입력하라고 명시적으로 요청했을 때만 입력 폼에 텍스트를 입력합니다. 단순 URL/내용 조회 질문일 때는 절대 임의로 호출하지 마세요.',
        parameters: {
          type: 'object',
          properties: {
            selector: { type: 'string', description: '입력 요소의 CSS 셀렉터 (예: #query, input[name="search"])' },
            text: { type: 'string', description: '입력할 텍스트' },
            press_enter: { type: 'boolean', description: '입력 후 Enter 키를 누를지 여부 (기본값: false)' }
          },
          required: ['selector', 'text']
        }
      },
      {
        name: 'browser_scroll',
        description: '웹 페이지를 위/아래로 스크롤합니다.',
        parameters: {
          type: 'object',
          properties: {
            direction: { type: 'string', enum: ['up', 'down', 'top', 'bottom'] },
            amount: { type: 'number', description: '스크롤 픽셀 양 (기본값: 500)' }
          }
        }
      },
      {
        name: 'browser_autofill_login',
        description: '로컬 보안 볼트(Vault)를 통해 로그인 폼에 대리 로그인을 수행합니다. 비밀번호 평문은 LLM에 노출되지 않습니다.',
        parameters: {
          type: 'object',
          properties: {
            domain: { type: 'string', description: '로그인 대상 도메인' }
          }
        }
      },
      {
        name: 'browser_get_tabs',
        description: '현재 브라우저 창에 열려 있는 모든 탭의 실시간 목록(tab_id, title, url, is_active, is_split)을 조회합니다. 탭 찾기, 탭 그룹핑, 탭 간 비교, 탭 닫기 전 대상 탭을 파악하기 위해 반드시 먼저 호출해야 합니다.',
        parameters: {
          type: 'object',
          properties: {}
        }
      },
      {
        name: 'browser_switch_tab',
        description: '지정한 tab_id의 탭으로 브라우저 활성 화면을 즉시 전환합니다. 검색어나 자연어 질의로 단 하나의 대상 탭이 명확히 특정되었을 때 화면 전환을 위해 호출합니다.',
        parameters: {
          type: 'object',
          properties: {
            tab_id: { type: 'number', description: '전환할 대상 탭의 ID (예: 1)' }
          },
          required: ['tab_id']
        }
      },
      {
        name: 'browser_reorder_tabs',
        description: '열려 있는 탭들의 표시 순서를 주어진 tab_ids 배열 순서대로 일괄 재배치합니다. 탭 그룹핑 시 도메인 및 주제가 유사한 탭들이 탭 바에서 나란히 인접하도록 모을 때 사용합니다.',
        parameters: {
          type: 'object',
          properties: {
            tab_ids: {
              type: 'array',
              items: { type: 'number' },
              description: '원하는 순서대로 정렬된 탭 ID 배열 (예: [3, 1, 4, 2])'
            }
          },
          required: ['tab_ids']
        }
      },
      {
        name: 'browser_get_tab_content',
        description: '특정 tab_id 탭의 웹페이지 제목, URL 및 상세 본문 텍스트(DOM)를 백그라운드에서 비동기로 추출합니다. 탭 간 내용/스펙/가격 비교를 수행할 때 각 대상 탭의 정보를 가져오기 위해 호출합니다.',
        parameters: {
          type: 'object',
          properties: {
            tab_id: { type: 'number', description: '본문 내용을 추출할 대상 탭의 ID' }
          },
          required: ['tab_id']
        }
      },
      {
        name: 'browser_split_tabs',
        description: '두 개의 탭(left_tab_id, right_tab_id)을 브라우저의 분할 화면(Dual Split View)으로 좌/우에 나란히 배치하여 시각적 대조를 지원합니다. 사용자가 2개 탭의 비교를 요청했을 때 유용합니다.',
        parameters: {
          type: 'object',
          properties: {
            left_tab_id: { type: 'number', description: '좌측 창에 표시할 기본 탭의 ID' },
            right_tab_id: { type: 'number', description: '우측 창에 분할로 띄울 탭의 ID' }
          },
          required: ['left_tab_id', 'right_tab_id']
        }
      },
      {
        name: 'browser_close_tabs',
        description: '지정한 tab_ids에 해당하는 탭들을 닫습니다. 데이터 유실 방지를 위해 다중 탭 닫기 시 기본적으로 사이드패널에 사용자 확인 카드를 표시하며, 사용자가 "확인 없이 닫아줘"라고 명시한 경우에만 require_confirmation을 false로 지정합니다.',
        parameters: {
          type: 'object',
          properties: {
            tab_ids: {
              type: 'array',
              items: { type: 'number' },
              description: '닫을 대상 탭 ID 목록'
            },
            require_confirmation: {
              type: 'boolean',
              description: '닫기 전 사용자 확인 카드 표시 여부 (다중 탭 닫기 시 기본값 true)'
            }
          },
          required: ['tab_ids']
        }
      },
      {
        name: 'browser_open_tab',
        description: '웹 브라우저에 새로운 탭을 추가로 생성하고 지정한 웹 페이지 또는 검색 결과 페이지를 로드합니다. 사용자가 "새 탭에 ~ 열어줘", "새 탭에서 검색해줘" 등을 요청했을 때 호출합니다.',
        parameters: {
          type: 'object',
          properties: {
            url: { type: 'string', description: '새 탭에 로드할 웹 URL 또는 검색 URL (예: https://github.com, https://www.google.com/search?q=... 미지정 시 기본 새 탭 lite://favorites)' },
            activate: { type: 'boolean', description: '생성 후 새 탭으로 즉시 화면을 전환할지 여부 (기본값: true, 백그라운드 생성 요청 시: false)' }
          }
        }
      },
      {
        name: 'browser_translate_page',
        description: '현재 웹페이지의 본문 텍스트를 대상 언어(target_lang)로 인플레이스(In-place) 실시간 번역하여 화면에 직접 치환하고, 사이드패널에 원문/번역문 토글 카드 및 핵심 요약 브리핑을 제공합니다. 한국어뿐만 아니라 영어, 일본어, 중국어 등 사용자가 요청한 모든 대상 언어로 번역할 수 있습니다. (예: "한국어로 번역해줘", "영어로 번역해줘", "일본어로 번역해줘")',
        parameters: {
          type: 'object',
          properties: {
            target_lang: { type: 'string', description: '번역 대상 언어 (예: "한국어", "영어", "일본어" 등, 미지정 시 기본값: "한국어")' },
            scope: { type: 'string', enum: ['main', 'full'], description: '번역 대상 영역 (기본값: "main" 메인 콘텐츠 우선, "full" 전체 DOM)' }
          }
        }
      },
      {
        name: 'browser_get_selected_text',
        description: '사용자가 현재 웹페이지에서 마우스로 드래그하여 선택한 텍스트를 추출합니다. 사용자가 "선택한 텍스트 번역해줘", "드래그한 내용 설명해줘" 등을 요청했을 때 호출합니다.',
        parameters: {
          type: 'object',
          properties: {}
        }
      }
    ];
  }

  // Execute a single browser action
  async executeToolAction(name, args) {
    sendNativeLog(`[TaskRuntime] executeToolAction: ${name} ${JSON.stringify(args)}`);
    this.addLog(`도구 실행: ${name} (${JSON.stringify(args)})`, 'action');

    // Check pause/interrupt guards
    while (this.isPaused && !this.isInterrupted) {
      this.setState(TaskState.PAUSED, '사용자가 작업을 일시 정지했습니다.');
      await new Promise(r => setTimeout(r, 300));
    }
    if (this.isInterrupted) {
      throw new Error('Task was interrupted by user');
    }

    let retryCount = 0;
    const maxRetries = 2;

    while (retryCount <= maxRetries) {
      try {
        switch (name) {
          case 'browser_navigate': {
            const url = args.url || '';
            this.setState(TaskState.RUNNING, `${url} 웹페이지로 이동 중...`);
            sendUiAction(`http://ui-action/load?url=${encodeURIComponent(url)}`);
            await new Promise(r => setTimeout(r, 1500)); // wait for navigation
            this.setState(TaskState.RUNNING, `${url} 페이지 로딩 완료`);
            return { status: 'success', message: `Navigated to ${url}` };
          }

          case 'browser_open_tab': {
            const rawUrl = (args.url || '').trim();
            const activate = (args.activate !== false);
            let targetUrl = rawUrl;
            if (!targetUrl) {
              targetUrl = 'lite://favorites';
            } else if (!targetUrl.startsWith('http://') && !targetUrl.startsWith('https://') && !targetUrl.startsWith('lite://') && !targetUrl.startsWith('file://')) {
              if (targetUrl.includes('.') && !targetUrl.includes(' ')) {
                targetUrl = 'https://' + targetUrl;
              } else {
                targetUrl = 'https://www.google.com/search?q=' + encodeURIComponent(targetUrl);
              }
            }

            this.setState(TaskState.RUNNING, `새 탭 생성 및 ${targetUrl} 탐색 중...`);

            const openResult = await new Promise((resolve) => {
              this.pendingOpenTabCallback = resolve;
              window.onAgentTabOpened = (res) => {
                if (this.pendingOpenTabCallback) {
                  const cb = this.pendingOpenTabCallback;
                  this.pendingOpenTabCallback = null;
                  cb(res);
                }
              };
              sendUiAction(`http://ui-action/ai-open-tab?url=${encodeURIComponent(targetUrl)}&activate=${activate ? '1' : '0'}`);
              setTimeout(() => {
                if (this.pendingOpenTabCallback) {
                  this.pendingOpenTabCallback = null;
                  resolve({ success: false, message: '새 탭 생성 응답 시간 초과' });
                }
              }, 4000);
            });

            if (openResult && openResult.success) {
              this.setState(TaskState.RUNNING, `새 탭(ID: ${openResult.tab_id}) 열기 완료`);
              return {
                status: 'success',
                tab_id: openResult.tab_id,
                url: openResult.url || targetUrl,
                title: openResult.title || '새 탭',
                activated: activate,
                message: `새 탭(ID: ${openResult.tab_id})에 ${openResult.url || targetUrl}을 열었습니다.`
              };
            } else {
              this.setState(TaskState.RUNNING, '새 탭 생성 실패');
              return {
                status: 'failed',
                message: openResult?.message || '새 탭을 생성하지 못했습니다 (최대 탭 수 초과 가능).'
              };
            }
          }

          case 'browser_get_selected_text': {
            this.setState(TaskState.RUNNING, '사용자 드래그 선택 영역 텍스트 추출 중...');
            const selData = await new Promise((resolve) => {
              this.pendingSelectionCallback = resolve;
              window.onAgentSelectionExtracted = (res) => {
                if (this.pendingSelectionCallback) {
                  const cb = this.pendingSelectionCallback;
                  this.pendingSelectionCallback = null;
                  cb(res);
                }
              };
              sendUiAction('http://ui-action/ai-get-selection');
              setTimeout(() => {
                if (this.pendingSelectionCallback) {
                  this.pendingSelectionCallback = null;
                  resolve({ selection: '', title: '', url: '' });
                }
              }, 3000);
            });
            const sel = (selData?.selection || '').trim();
            this.setState(TaskState.RUNNING, sel ? `선택 텍스트 추출 완료 (${sel.length}자)` : '선택된 텍스트가 없습니다.');
            const selText = selData?.selection || '';
            if (selText) {
              return {
                status: 'success',
                selected_text: selText,
                url: selData.url || '',
                title: selData.title || '',
                message: `선택된 텍스트 (${selText.length}자)를 성공적으로 추출했습니다.`
              };
            } else {
              return {
                status: 'empty',
                message: '현재 웹페이지에서 마우스로 드래그 선택된 텍스트가 없습니다. 전체 페이지 번역을 원하시면 browser_translate_page를 사용하세요.'
              };
            }
          }

          case 'browser_translate_page': {
            const targetLang = args.target_lang || '한국어';
            const scope = args.scope || 'main';

            this.setState(TaskState.RUNNING, '웹페이지 텍스트 블록 추출 중...');

            const extractData = await new Promise((resolve) => {
              this.pendingPageTextsCallback = resolve;
              window.onAgentPageTextsExtracted = (res) => {
                if (this.pendingPageTextsCallback) {
                  const cb = this.pendingPageTextsCallback;
                  this.pendingPageTextsCallback = null;
                  cb(res);
                }
              };
              sendUiAction(`http://ui-action/ai-get-page-texts?scope=${encodeURIComponent(scope)}`);
              setTimeout(() => {
                if (this.pendingPageTextsCallback) {
                  this.pendingPageTextsCallback = null;
                  resolve({ items: [], title: '', url: '' });
                }
              }, 6000);
            });

            const items = extractData?.items || [];
            if (!items || items.length === 0) {
              return {
                status: 'failed',
                message: '번역할 본문 텍스트 요소를 찾을 수 없습니다.'
              };
            }

            this.setState(TaskState.RUNNING, `${items.length}개 텍스트 블록 ${targetLang} 번역 중...`);

            const settings = window.AIProviderFactory.getSettings();
            const currentAuthMode = settings[`${settings.provider}AuthMode`] || 'apikey';
            settings.authType = currentAuthMode;
            if (currentAuthMode === 'subscription' && window.getSubscriptionToken) {
              const subToken = await window.getSubscriptionToken(settings.provider);
              if (subToken) {
                settings.subscriptionToken = subToken;
              }
            }
            const provider = window.AIProviderFactory.createProvider(settings);

            const translationPrompt =
              `당신은 전문 웹 브라우저 번역기입니다. 아래 제공된 웹페이지 본문 텍스트 블록 배열을 문맥을 살려 자연스러운 ${targetLang}(으)로 번역해주세요.\n` +
              `원문의 서식이나 고유명사는 적절히 보존하고, 반드시 아래와 같은 순수 JSON 형식으로만 응답하세요(코드블록이나 설명 없이 JSON만 출력):\n` +
              `{\n  "translations": [\n    { "id": 1, "text": "번역된 내용" }\n  ]\n}\n\n` +
              `번역할 데이터:\n` + JSON.stringify(items);

            let rawResponse = '';
            try {
              let fullResp = '';
              await provider.chatStream({
                messages: [{ role: 'user', content: translationPrompt }],
                systemPrompt: 'You are a precise, JSON-only translator for web page text nodes. Return ONLY valid JSON.',
                onChunk: (chunk, full) => { fullResp = full; }
              });
              rawResponse = fullResp;
            } catch (err) {
              console.error('Translation LLM call error:', err);
              return { status: 'error', message: `AI 번역 중 오류가 발생했습니다: ${err.message}` };
            }

            let translations = [];
            try {
              let cleaned = rawResponse.trim();
              if (cleaned.startsWith('```json')) cleaned = cleaned.slice(7);
              if (cleaned.startsWith('```')) cleaned = cleaned.slice(3);
              if (cleaned.endsWith('```')) cleaned = cleaned.slice(0, -3);
              const parsed = JSON.parse(cleaned.trim());
              translations = parsed.translations || [];
            } catch (e) {
              const match = rawResponse.match(/\{[\s\S]*"translations"[\s\S]*\}/);
              if (match) {
                try {
                  const p = JSON.parse(match[0]);
                  translations = p.translations || [];
                } catch(e2) {}
              }
            }

            if (!translations || translations.length === 0) {
              return { status: 'failed', message: 'AI 번역 결과를 JSON으로 파싱하지 못했습니다.' };
            }

            this.setState(TaskState.RUNNING, '웹페이지에 번역문 적용 중...');
            const transPayload = JSON.stringify(translations);
            const b64 = btoa(unescape(encodeURIComponent(transPayload)));
            sendUiAction(`http://ui-action/ai-apply-translation?data=${encodeURIComponent(b64)}`);
            await new Promise(r => setTimeout(r, 400));

            return {
              status: 'success',
              translated_count: translations.length,
              target_lang: targetLang,
              title: extractData.title || '',
              url: extractData.url || '',
              message: `총 ${translations.length}개 텍스트 블록을 ${targetLang}(으)로 성공적으로 번역 치환했습니다.`
            };
          }

          case 'browser_get_page_content': {
            this.setState(TaskState.RUNNING, '웹페이지 DOM 구조 및 본문 데이터 추출 중...');
            const domData = await new Promise((resolve) => {
              this.pendingDomCallback = resolve;
              sendUiAction('http://ui-action/ai-get-dom-summary');
              setTimeout(() => {
                if (this.pendingDomCallback) {
                  this.pendingDomCallback = null;
                  resolve({ title: '현재 페이지', url: '', bodySnippet: '페이지 본문 내용을 읽을 수 없습니다.', markdown: '', buttons: [], inputs: [], visualContext: { isVisualHeavy: false, reason: [] } });
                }
              }, 8000);
            });
            const textLen = (domData?.bodySnippet || domData?.markdown || '').length;
            this.setState(TaskState.RUNNING, `본문 데이터 수신 완료 (${textLen}자)`);

            const isVisualHeavy = Boolean(domData?.visualContext?.isVisualHeavy);
            const userQuery = (this.lastUserQuery || (typeof window !== 'undefined' && window.lastUserPrompt) || '').toLowerCase();
            const visualKeywordRegex = /(차트|그래프|화면|캔버스|시각|이미지|캔들|dashboard|대시보드|chart|graph|visual)/i;
            const needsVisualFallback = isVisualHeavy || visualKeywordRegex.test(userQuery);

            let screenshotData = null;
            if (needsVisualFallback) {
              this.setState(TaskState.RUNNING, '시각 집약(차트/대시보드) 페이지 감지 - 뷰포트 시각 캡처 수집 중...');
              const captureRes = await this.captureViewport();
              if (captureRes?.status === 'success' && captureRes.data) {
                screenshotData = captureRes.data;
                this.setState(TaskState.RUNNING, `뷰포트 시각 캡처 완료 (${captureRes.width}x${captureRes.height})`);
              }
            }

            return {
              status: 'success',
              url: domData.url || '',
              title: domData.title || '',
              bodySnippet: domData.bodySnippet || domData.markdown || '',
              buttons: domData.buttons || [],
              inputs: domData.inputs || [],
              visualContext: domData.visualContext || { isVisualHeavy: false, reason: [] },
              ...(screenshotData ? { screenshot: screenshotData } : {})
            };
          }

          case 'browser_take_screenshot': {
            this.setState(TaskState.RUNNING, '웹페이지 뷰포트 화면 스크린샷 캡처 중...');
            const captureRes = await this.captureViewport();
            if (captureRes?.status === 'success' && captureRes.data) {
              this.setState(TaskState.RUNNING, `뷰포트 스크린샷 캡처 완료 (${captureRes.width}x${captureRes.height})`);
              return {
                status: 'success',
                width: captureRes.width,
                height: captureRes.height,
                screenshot: captureRes.data
              };
            } else {
              this.setState(TaskState.RUNNING, '뷰포트 화면 캡처 실패');
              return {
                status: 'error',
                message: captureRes?.message || '뷰포트 화면 캡처에 실패했습니다.'
              };
            }
          }

          case 'youtube_get_comments': {
            const limit = Math.min(Math.max(parseInt(args?.limit) || 30, 5), 50);
            this.setState(TaskState.RUNNING, `YouTube 시청자 댓글 및 여론 데이터 수집 중 (최대 ${limit}개)...`);
            const commentData = await new Promise((resolve) => {
              this.pendingYoutubeCommentsCallback = resolve;
              sendUiAction(`http://ui-action/ai-get-youtube-comments?limit=${limit}`);
              setTimeout(() => {
                if (this.pendingYoutubeCommentsCallback) {
                  this.pendingYoutubeCommentsCallback = null;
                  resolve({
                    title: 'YouTube 동영상',
                    url: '',
                    count: 0,
                    comments: [],
                    message: '댓글 수집 응답 대기 시간이 초과되었습니다.'
                  });
                }
              }, 10000);
            });
            const count = commentData.comments ? commentData.comments.length : 0;
            this.setState(TaskState.RUNNING, `댓글 ${count}개 수집 완료`);
            return {
              status: count > 0 ? 'success' : 'no_comments',
              url: commentData.url || '',
              title: commentData.title || '',
              count: count,
              comments: commentData.comments || [],
              message: commentData.message || (count > 0 ? `총 ${count}개의 댓글을 성공적으로 수집했습니다.` : '수집된 댓글이 없습니다. 동영상의 댓글이 사용 중지되었거나 아직 로드되지 않았을 수 있습니다.')
            };
          }

          case 'browser_click_element': {
            const selector = args.selector || '';
            const text = args.text || '';
            this.setState(TaskState.RUNNING, `화면 요소 위치 탐색 중: ${text || selector}`);
            if (selector) {
              sendUiAction(`http://ui-action/ai-highlight-element?selector=${encodeURIComponent(selector)}`);
              await new Promise(r => setTimeout(r, 200));
            }
            this.setState(TaskState.RUNNING, `요소 클릭 이벤트 전달 중: ${text || selector}`);
            sendUiAction(`http://ui-action/ai-click-element?selector=${encodeURIComponent(selector)}&text=${encodeURIComponent(text)}`);
            await new Promise(r => setTimeout(r, 600));
            this.setState(TaskState.RUNNING, '요소 클릭 완료');
            return { status: 'success', message: `Clicked element: ${selector || text}` };
          }

          case 'browser_type_text': {
            const selector = args.selector || '';
            const text = args.text || '';
            const enter = args.press_enter ? '1' : '0';
            this.setState(TaskState.RUNNING, `입력 필드 탐색 및 포커스 설정: ${selector}`);
            if (selector) {
              sendUiAction(`http://ui-action/ai-highlight-element?selector=${encodeURIComponent(selector)}`);
              await new Promise(r => setTimeout(r, 200));
            }
            this.setState(TaskState.RUNNING, `텍스트 "${text}" 입력 이벤트 전달 중...`);
            sendUiAction(`http://ui-action/ai-type-element?selector=${encodeURIComponent(selector)}&text=${encodeURIComponent(text)}&enter=${enter}`);
            await new Promise(r => setTimeout(r, 600));
            this.setState(TaskState.RUNNING, '텍스트 입력 완료');
            return { status: 'success', message: `Typed into ${selector}` };
          }

          case 'browser_scroll': {
            const dir = args.direction || 'down';
            const amount = args.amount || 500;
            this.setState(TaskState.RUNNING, `화면 ${dir} 방향으로 ${amount}px 스크롤 중...`);
            sendUiAction(`http://ui-action/ai-scroll?direction=${encodeURIComponent(dir)}&amount=${amount}`);
            await new Promise(r => setTimeout(r, 400));
            this.setState(TaskState.RUNNING, '화면 스크롤 완료');
            return { status: 'success', message: `Scrolled ${dir}` };
          }

          case 'browser_autofill_login': {
            const domain = args.domain || '';
            this.setState(TaskState.RUNNING, `Vault 마스터 보안 금고에서 ${domain} 계정 검색 중...`);
            const result = await new Promise((resolve) => {
              window.onVaultAutofillResult = (res) => resolve(res);
              sendUiAction(`http://ui-action/vault-autofill?domain=${encodeURIComponent(domain)}`);
              setTimeout(() => resolve(false), 2500);
            });
            this.setState(TaskState.RUNNING, result ? '보안 계정 자동 완성 완료' : '일치하는 계정 정보 없음');
            if (result) {
              return { status: 'success', message: 'Vault autofill executed safely (Plaintext password was not exposed to LLM context)' };
            } else {
              return { status: 'failed', message: 'No matching credentials found in Vault for this domain' };
            }
          }

          case 'browser_get_tabs': {
            this.setState(TaskState.RUNNING, '열려 있는 탭 목록 및 활성 상태 조회 중...');
            const tabs = await new Promise((resolve) => {
              this.pendingTabsCallback = resolve;
              sendUiAction('http://ui-action/ai-get-tabs');
              setTimeout(() => {
                if (this.pendingTabsCallback) {
                  this.pendingTabsCallback = null;
                  resolve([]);
                }
              }, 4000);
            });
            this.cachedTabs = tabs;
            this.setState(TaskState.RUNNING, `총 ${tabs.length}개 탭 정보 조회 완료`);
            return { status: 'success', tabs };
          }

          case 'browser_switch_tab': {
            const tabId = Number(args.tab_id);
            this.setState(TaskState.RUNNING, `탭 #${tabId} 활성 화면으로 전환 중...`);
            sendUiAction(`http://ui-action/switch-tab?id=${tabId}`);
            await new Promise(r => setTimeout(r, 400));
            this.setState(TaskState.RUNNING, `탭 #${tabId} 화면 전환 완료`);
            return { status: 'success', message: `Switched to tab ${tabId}`, tab_id: tabId };
          }

          case 'browser_reorder_tabs': {
            const tabIds = args.tab_ids || [];
            if (!Array.isArray(tabIds) || tabIds.length === 0) {
              return { status: 'failed', message: 'No tab_ids provided for reorder' };
            }
            this.setState(TaskState.RUNNING, `탭 ${tabIds.length}개 순서 재배치 및 그룹핑 중...`);
            const idsParam = tabIds.join(',');
            await new Promise((resolve) => {
              this.pendingReorderCallback = resolve;
              sendUiAction(`http://ui-action/ai-reorder-tabs?ids=${encodeURIComponent(idsParam)}`);
              setTimeout(() => {
                if (this.pendingReorderCallback) {
                  this.pendingReorderCallback = null;
                  resolve(true);
                }
              }, 2000);
            });
            this.setState(TaskState.RUNNING, `탭 ${tabIds.length}개 탭 바 재배치 완료`);
            return { status: 'success', message: 'Tabs reordered successfully', tab_ids: tabIds };
          }

          case 'browser_get_tab_content': {
            const tabId = Number(args.tab_id);
            this.setState(TaskState.RUNNING, `탭 #${tabId} 백그라운드 본문 데이터 추출 중...`);
            if (!this.pendingTabDomCallbacks) this.pendingTabDomCallbacks = {};
            const domData = await new Promise((resolve) => {
              this.pendingTabDomCallbacks[tabId] = resolve;
              sendUiAction(`http://ui-action/ai-get-tab-dom?id=${tabId}`);
              setTimeout(() => {
                if (this.pendingTabDomCallbacks && this.pendingTabDomCallbacks[tabId]) {
                  delete this.pendingTabDomCallbacks[tabId];
                  resolve({ tab_id: tabId, title: '', url: '', bodySnippet: '타임아웃: 본문을 추출할 수 없습니다.' });
                }
              }, 6000);
            });
            this.setState(TaskState.RUNNING, `탭 #${tabId} 본문 데이터 추출 완료`);
            return {
              status: 'success',
              tab_id: domData?.tab_id || tabId,
              title: domData?.title || '',
              url: domData?.url || '',
              bodySnippet: domData?.bodySnippet || domData?.markdown || ''
            };
          }

          case 'browser_split_tabs': {
            const leftId = Number(args.left_tab_id);
            const rightId = Number(args.right_tab_id);
            this.setState(TaskState.RUNNING, `탭 #${leftId}과 탭 #${rightId} 듀얼 분할 화면 구성 중...`);
            await new Promise((resolve) => {
              this.pendingSplitCallback = resolve;
              sendUiAction(`http://ui-action/ai-split-tabs?left=${leftId}&right=${rightId}`);
              setTimeout(() => {
                if (this.pendingSplitCallback) {
                  this.pendingSplitCallback = null;
                  resolve(true);
                }
              }, 2000);
            });
            this.setState(TaskState.RUNNING, '분할 화면 배치 완료');
            return { status: 'success', message: `Split view enabled for tabs ${leftId} and ${rightId}`, left_tab_id: leftId, right_tab_id: rightId };
          }

          case 'browser_close_tabs': {
            const tabIds = (args.tab_ids || []).map(Number).filter(n => !isNaN(n));
            if (tabIds.length === 0) {
              return { status: 'success', closed_tab_ids: [] };
            }
            this.setState(TaskState.RUNNING, `${tabIds.length}개 탭 종료 처리 중...`);
            if (!this.cachedTabs || this.cachedTabs.length === 0) {
              try {
                const res = await this.executeToolAction('browser_get_tabs', {});
                if (res && res.tabs) this.cachedTabs = res.tabs;
              } catch (e) {}
            }
            const requireConfirmation = args.require_confirmation !== undefined
              ? Boolean(args.require_confirmation)
              : (tabIds.length > 1);
            if (requireConfirmation && window.confirmCloseTabsUI) {
              const confirmed = await window.confirmCloseTabsUI(tabIds, this.cachedTabs);
              if (!confirmed) {
                this.setState(TaskState.RUNNING, '사용자가 탭 닫기를 취소했습니다.');
                return { status: 'cancelled', message: '사용자가 탭 닫기 확인 창에서 작업을 취소했습니다.', closed_tab_ids: [] };
              }
            }
            this.setState(TaskState.RUNNING, `${tabIds.length}개 탭 닫기 실행 중...`);
            const idsParam = tabIds.join(',');
            await new Promise((resolve) => {
              this.pendingCloseCallback = resolve;
              sendUiAction(`http://ui-action/ai-close-tabs?ids=${encodeURIComponent(idsParam)}`);
              setTimeout(() => {
                if (this.pendingCloseCallback) {
                  this.pendingCloseCallback = null;
                  resolve(true);
                }
              }, 2000);
            });
            this.setState(TaskState.RUNNING, `${tabIds.length}개 탭 닫기 완료`);
            return { status: 'success', message: `${tabIds.length}개 탭을 성공적으로 닫았습니다.`, closed_tab_ids: tabIds };
          }

          default:
            return { status: 'unknown_tool', message: `Tool ${name} is not supported` };
        }
      } catch (err) {
        retryCount++;
        this.setState(TaskState.STUCK, `오류 발생: ${err.message} (자율 복구 재시도 ${retryCount}/${maxRetries})`);
        // Self-recovery: try scrolling slightly and waiting
        sendUiAction('http://ui-action/ai-scroll?direction=down&amount=200');
        await new Promise(r => setTimeout(r, 800));

        if (retryCount > maxRetries) {
          this.setState(TaskState.WAITING, '요소를 찾을 수 없거나 작업이 막혔습니다. 사용자의 지침을 기다립니다.');
          throw err;
        }
      }
    }
  }

  handleDomExtracted(domData) {
    if (this.pendingDomCallback) {
      this.pendingDomCallback(domData);
      this.pendingDomCallback = null;
    }
  }

  handleYoutubeCommentsReceived(commentData) {
    if (this.pendingYoutubeCommentsCallback) {
      this.pendingYoutubeCommentsCallback(commentData);
      this.pendingYoutubeCommentsCallback = null;
    }
  }

  handleTabsListReceived(tabs) {
    if (this.pendingTabsCallback) {
      this.pendingTabsCallback(tabs);
      this.pendingTabsCallback = null;
    }
  }

  handleTabReordered(success) {
    if (this.pendingReorderCallback) {
      this.pendingReorderCallback(success);
      this.pendingReorderCallback = null;
    }
  }

  handleTabDomExtracted(tabId, data) {
    if (this.pendingTabDomCallbacks && this.pendingTabDomCallbacks[tabId]) {
      this.pendingTabDomCallbacks[tabId](data);
      delete this.pendingTabDomCallbacks[tabId];
    }
  }

  handleTabSplitCompleted(success) {
    if (this.pendingSplitCallback) {
      this.pendingSplitCallback(success);
      this.pendingSplitCallback = null;
    }
  }

  handleTabsClosed(success) {
    if (this.pendingCloseCallback) {
      this.pendingCloseCallback(success);
      this.pendingCloseCallback = null;
    }
  }

  async captureViewport() {
    return new Promise((resolve) => {
      this.pendingViewportCallback = resolve;
      sendUiAction('http://ui-action/ai-capture-viewport');
      setTimeout(() => {
        if (this.pendingViewportCallback) {
          this.pendingViewportCallback = null;
          resolve({ status: 'error', message: '뷰포트 화면 캡처 응답 대기 시간이 초과되었습니다.' });
        }
      }, 6000);
    });
  }

  handleViewportCaptured(data) {
    if (this.pendingViewportCallback) {
      this.pendingViewportCallback(data);
      this.pendingViewportCallback = null;
    }
  }

  pause() {
    this.isPaused = true;
    this.setState(TaskState.PAUSED, '일시정지되었습니다.');
  }

  resume() {
    this.isPaused = false;
    this.setState(TaskState.RUNNING, '작업을 재개합니다.');
  }

  stop() {
    this.isInterrupted = true;
    this.isPaused = false;
    this.setState(TaskState.IDLE, '작업이 중단되었습니다.');
  }

  manualTakeover() {
    this.isPaused = true;
    this.setState(TaskState.WAITING, '사용자 직접 개입 모드: 원하는 작업을 수행한 후 [에이전트 계속]을 누르세요.');
  }
}

window.TaskRuntime = TaskRuntime;
window.TaskState = TaskState;
window.taskRuntime = new TaskRuntime();

// C backend callback bridges
window.onAgentViewportCaptured = function(data) {
  if (window.taskRuntime) {
    window.taskRuntime.handleViewportCaptured(data);
  }
  if (typeof window.onViewportCapturedCallback === 'function') {
    window.onViewportCapturedCallback(data);
  }
};

window.onAgentDomExtracted = function(data) {
  if (window.taskRuntime) {
    window.taskRuntime.handleDomExtracted(data);
  }
};

window.onAgentTabsListReceived = function(tabs) {
  if (window.taskRuntime) {
    window.taskRuntime.handleTabsListReceived(tabs);
  }
};

window.onAgentTabReordered = function(res) {
  if (window.taskRuntime) {
    window.taskRuntime.handleTabReordered(res);
  }
};

window.onAgentTabDomExtracted = function(tabId, data) {
  if (window.taskRuntime) {
    window.taskRuntime.handleTabDomExtracted(tabId, data);
  }
};

window.onAgentTabSplitCompleted = function(res) {
  if (window.taskRuntime) {
    window.taskRuntime.handleTabSplitCompleted(res);
  }
};

window.onAgentTabsClosed = function(res) {
  if (window.taskRuntime) {
    window.taskRuntime.handleTabsClosed(res);
  }
};

window.onAgentYoutubeCommentsReceived = function(data) {
  if (window.taskRuntime) {
    window.taskRuntime.handleYoutubeCommentsReceived(data);
  }
};
