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
      }
    ];
  }

  // Execute a single browser action
  async executeToolAction(name, args) {
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
            window.location.href = `http://ui-action/load?url=${encodeURIComponent(url)}`;
            await new Promise(r => setTimeout(r, 1500)); // wait for navigation
            return { status: 'success', message: `Navigated to ${url}` };
          }

          case 'browser_get_page_content': {
            const domData = await new Promise((resolve) => {
              this.pendingDomCallback = resolve;
              window.location.href = 'http://ui-action/ai-get-dom-summary';
              setTimeout(() => {
                if (this.pendingDomCallback) {
                  this.pendingDomCallback = null;
                  resolve({ title: '현재 페이지', url: '', bodySnippet: '페이지 본문 내용을 읽을 수 없습니다.', markdown: '', buttons: [], inputs: [] });
                }
              }, 5000);
            });
            return {
              status: 'success',
              url: domData.url || '',
              title: domData.title || '',
              bodySnippet: domData.bodySnippet || domData.markdown || '',
              buttons: domData.buttons || [],
              inputs: domData.inputs || []
            };
          }

          case 'browser_click_element': {
            const selector = args.selector || '';
            const text = args.text || '';
            if (selector) {
              window.location.href = `http://ui-action/ai-highlight-element?selector=${encodeURIComponent(selector)}`;
              await new Promise(r => setTimeout(r, 200));
            }
            window.location.href = `http://ui-action/ai-click-element?selector=${encodeURIComponent(selector)}&text=${encodeURIComponent(text)}`;
            await new Promise(r => setTimeout(r, 600));
            return { status: 'success', message: `Clicked element: ${selector || text}` };
          }

          case 'browser_type_text': {
            const selector = args.selector || '';
            const text = args.text || '';
            const enter = args.press_enter ? '1' : '0';
            if (selector) {
              window.location.href = `http://ui-action/ai-highlight-element?selector=${encodeURIComponent(selector)}`;
              await new Promise(r => setTimeout(r, 200));
            }
            window.location.href = `http://ui-action/ai-type-element?selector=${encodeURIComponent(selector)}&text=${encodeURIComponent(text)}&enter=${enter}`;
            await new Promise(r => setTimeout(r, 600));
            return { status: 'success', message: `Typed into ${selector}` };
          }

          case 'browser_scroll': {
            const dir = args.direction || 'down';
            const amount = args.amount || 500;
            window.location.href = `http://ui-action/ai-scroll?direction=${encodeURIComponent(dir)}&amount=${amount}`;
            await new Promise(r => setTimeout(r, 400));
            return { status: 'success', message: `Scrolled ${dir}` };
          }

          case 'browser_autofill_login': {
            const domain = args.domain || '';
            const result = await new Promise((resolve) => {
              window.onVaultAutofillResult = (res) => resolve(res);
              window.location.href = `http://ui-action/vault-autofill?domain=${encodeURIComponent(domain)}`;
              setTimeout(() => resolve(false), 2500);
            });
            if (result) {
              return { status: 'success', message: 'Vault autofill executed safely (Plaintext password was not exposed to LLM context)' };
            } else {
              return { status: 'failed', message: 'No matching credentials found in Vault for this domain' };
            }
          }

          default:
            return { status: 'unknown_tool', message: `Tool ${name} is not supported` };
        }
      } catch (err) {
        retryCount++;
        this.setState(TaskState.STUCK, `오류 발생: ${err.message} (자율 복구 재시도 ${retryCount}/${maxRetries})`);
        // Self-recovery: try scrolling slightly and waiting
        window.location.href = 'http://ui-action/ai-scroll?direction=down&amount=200';
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

// C backend callback bridge for DOM extraction
window.onAgentDomExtracted = function(data) {
  if (window.taskRuntime) {
    window.taskRuntime.handleDomExtracted(data);
  }
};
