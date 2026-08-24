/**
 * Lite Browser - AI Side Panel Orchestrator
 */

document.addEventListener('DOMContentLoaded', () => {
  // DOM Elements
  const statusBadge = document.getElementById('agent-status-badge');
  const chatContainer = document.getElementById('chat-container');
  const welcomeCard = document.getElementById('welcome-card');
  const messagesList = document.getElementById('messages-list');
  const promptInput = document.getElementById('prompt-input');
  const sendBtn = document.getElementById('send-btn');
  const stopBtn = document.getElementById('stop-btn');
  const clearChatBtn = document.getElementById('clear-chat-btn');
  const settingsBtn = document.getElementById('settings-btn');
  const closePanelBtn = document.getElementById('close-panel-btn');

  // Settings DOM Elements
  const settingsPanel = document.getElementById('settings-panel');
  const closeSettingsBtn = document.getElementById('close-settings-btn');
  const saveSettingsBtn = document.getElementById('save-settings-btn');
  const providerSelect = document.getElementById('provider-select');
  const geminiFields = document.getElementById('gemini-fields');
  const openaiFields = document.getElementById('openai-fields');
  const anthropicFields = document.getElementById('anthropic-fields');
  const ollamaFields = document.getElementById('ollama-fields');
  const geminiKeyInput = document.getElementById('gemini-key');
  const geminiModelSelect = document.getElementById('gemini-model');
  const openaiKeyInput = document.getElementById('openai-key');
  const openaiModelInput = document.getElementById('openai-model');
  const anthropicKeyInput = document.getElementById('anthropic-key');
  const anthropicModelInput = document.getElementById('anthropic-model');
  const ollamaUrlInput = document.getElementById('ollama-url');
  const ollamaModelInput = document.getElementById('ollama-model');

  // Vault DOM Elements
  const vaultDomainInput = document.getElementById('vault-domain');
  const vaultUserInput = document.getElementById('vault-user');
  const vaultPassInput = document.getElementById('vault-pass');
  const vaultAddBtn = document.getElementById('vault-add-btn');
  const vaultList = document.getElementById('vault-list');

  // Memory DOM Elements
  const memoryIndexToggle = document.getElementById('memory-index-toggle');
  const memoryCountLabel = document.getElementById('memory-count-label');
  const clearMemoryBtn = document.getElementById('clear-memory-btn');

  // Intervention DOM Elements
  const interventionCard = document.getElementById('intervention-card');
  const interventionMessage = document.getElementById('intervention-message');
  const interventionResumeBtn = document.getElementById('intervention-resume-btn');
  const interventionStopBtn = document.getElementById('intervention-stop-btn');

  // Chat State
  let conversationHistory = [];
  let abortController = null;
  let isGenerating = false;

  // 1. Initialize State & Settings
  function loadSettingsIntoUI() {
    const s = AIProviderFactory.getSettings();
    providerSelect.value = s.provider || 'gemini';
    geminiKeyInput.value = s.geminiKey || '';
    geminiModelSelect.value = s.geminiModel || 'gemini-3.7-flash';
    openaiKeyInput.value = s.openaiKey || '';
    openaiModelInput.value = s.openaiModel || 'gpt-4o';
    anthropicKeyInput.value = s.anthropicKey || '';
    anthropicModelInput.value = s.anthropicModel || 'claude-3-7-sonnet-20250219';
    ollamaUrlInput.value = s.ollamaUrl || 'http://localhost:11434';
    ollamaModelInput.value = s.ollamaModel || 'llama3.2';

    updateProviderFields(s.provider || 'gemini');
    updateMemoryStats();
    requestVaultList();
  }

  function updateProviderFields(provider) {
    geminiFields.classList.toggle('hidden', provider !== 'gemini');
    openaiFields.classList.toggle('hidden', provider !== 'openai');
    anthropicFields.classList.toggle('hidden', provider !== 'anthropic');
    ollamaFields.classList.toggle('hidden', provider !== 'ollama');
  }

  providerSelect.addEventListener('change', (e) => {
    updateProviderFields(e.target.value);
  });

  saveSettingsBtn.addEventListener('click', () => {
    const updated = {
      provider: providerSelect.value,
      geminiKey: geminiKeyInput.value.trim(),
      geminiModel: geminiModelSelect.value,
      openaiKey: openaiKeyInput.value.trim(),
      openaiModel: openaiModelInput.value.trim(),
      anthropicKey: anthropicKeyInput.value.trim(),
      anthropicModel: anthropicModelInput.value.trim(),
      ollamaUrl: ollamaUrlInput.value.trim(),
      ollamaModel: ollamaModelInput.value.trim()
    };
    AIProviderFactory.saveSettings(updated);
    settingsPanel.classList.add('hidden');
    appendAssistantMessage('설정이 안전하게 저장되었습니다.');
  });

  settingsBtn.addEventListener('click', () => {
    loadSettingsIntoUI();
    settingsPanel.classList.remove('hidden');
  });

  closeSettingsBtn.addEventListener('click', () => {
    settingsPanel.classList.add('hidden');
  });

  closePanelBtn.addEventListener('click', () => {
    window.location.href = 'http://ui-action/toggle-ai-sidepanel';
  });

  // 2. Task Runtime Status & Callbacks
  window.taskRuntime.onStateChange = (newState, detail) => {
    statusBadge.textContent = newState;
    statusBadge.className = `status-badge status-${newState.toLowerCase()}`;

    if (newState === TaskState.STUCK || newState === TaskState.WAITING) {
      interventionMessage.textContent = detail || '작업 수행 중 사용자 개입이 필요합니다.';
      interventionCard.classList.remove('hidden');
    } else {
      interventionCard.classList.add('hidden');
    }
  };

  interventionResumeBtn.addEventListener('click', () => {
    interventionCard.classList.add('hidden');
    window.taskRuntime.resume();
  });

  interventionStopBtn.addEventListener('click', () => {
    interventionCard.classList.add('hidden');
    window.taskRuntime.stop();
  });

  // 3. Vault Management
  function requestVaultList() {
    window.location.href = 'http://ui-action/vault-get-list';
  }

  window.renderVaultList = function(entries) {
    if (!entries || entries.length === 0) {
      vaultList.innerHTML = '<div class="empty-hint">저장된 계정이 없습니다.</div>';
      return;
    }
    vaultList.innerHTML = '';
    entries.forEach(e => {
      const item = document.createElement('div');
      item.className = 'vault-item';
      item.innerHTML = `
        <div>
          <strong>${escapeHtml(e.domain)}</strong>
          <span style="color: var(--text-secondary); margin-left: 6px;">(${escapeHtml(e.username || '비공개')})</span>
        </div>
        <button class="icon-btn" title="삭제" data-domain="${escapeHtml(e.domain)}">🗑️</button>
      `;
      item.querySelector('button').addEventListener('click', () => {
        window.location.href = `http://ui-action/vault-delete?domain=${encodeURIComponent(e.domain)}`;
        setTimeout(requestVaultList, 200);
      });
      vaultList.appendChild(item);
    });
  };

  vaultAddBtn.addEventListener('click', () => {
    const domain = vaultDomainInput.value.trim();
    const user = vaultUserInput.value.trim();
    const pass = vaultPassInput.value;

    if (!domain || !pass) {
      alert('도메인과 비밀번호를 입력해주세요.');
      return;
    }

    window.location.href = `http://ui-action/vault-save?domain=${encodeURIComponent(domain)}&user=${encodeURIComponent(user)}&pass=${encodeURIComponent(pass)}`;
    vaultDomainInput.value = '';
    vaultUserInput.value = '';
    vaultPassInput.value = '';
    setTimeout(requestVaultList, 200);
  });

  // 4. Memory Management
  async function updateMemoryStats() {
    const count = await window.agentMemory.getMemoryCount();
    memoryCountLabel.textContent = `${count}개`;
  }

  memoryIndexToggle.addEventListener('change', (e) => {
    window.agentMemory.setIndexingEnabled(e.target.checked);
  });

  clearMemoryBtn.addEventListener('click', async () => {
    if (confirm('인덱싱된 모든 브라우징 기억 데이터를 삭제하시겠습니까?')) {
      await window.agentMemory.clearAllMemory();
      await updateMemoryStats();
      alert('기억 데이터가 완전히 삭제되었습니다.');
    }
  });

  // 5. Chat & Prompt Execution
  function appendUserMessage(text) {
    if (welcomeCard) welcomeCard.style.display = 'none';
    const div = document.createElement('div');
    div.className = 'message-bubble message-user';
    div.textContent = text;
    messagesList.appendChild(div);
    chatContainer.scrollTop = chatContainer.scrollHeight;
  }

  function appendAssistantMessage(text) {
    if (welcomeCard) welcomeCard.style.display = 'none';
    const div = document.createElement('div');
    div.className = 'message-bubble message-assistant';
    div.innerHTML = formatMarkdown(text);
    messagesList.appendChild(div);
    chatContainer.scrollTop = chatContainer.scrollHeight;
    return div;
  }

  function createStreamingBubble() {
    if (welcomeCard) welcomeCard.style.display = 'none';
    const div = document.createElement('div');
    div.className = 'message-bubble message-assistant';

    const statusNotice = document.createElement('div');
    statusNotice.className = 'status-notice hidden';

    const thinkingBox = document.createElement('details');
    thinkingBox.className = 'thinking-box hidden';
    thinkingBox.innerHTML = '<summary>🧠 사고 과정 (Thinking)...</summary><div class="thinking-content"></div>';

    const contentDiv = document.createElement('div');
    contentDiv.className = 'bubble-content';

    div.appendChild(statusNotice);
    div.appendChild(thinkingBox);
    div.appendChild(contentDiv);
    messagesList.appendChild(div);
    chatContainer.scrollTop = chatContainer.scrollHeight;

    return {
      container: div,
      statusNotice,
      thinkingBox,
      thinkingContent: thinkingBox.querySelector('.thinking-content'),
      contentDiv
    };
  }

  function appendToolCard(toolName, args, result) {
    const card = document.createElement('div');
    card.className = 'action-card';
    card.innerHTML = `
      <div class="action-header">
        <span>🛠️ <strong>${escapeHtml(toolName)}</strong></span>
        <span style="color: ${result?.isError ? 'var(--danger-color)' : 'var(--success-color)'}; font-size: 11px;">
          ${result?.isError ? '실패' : '완료'}
        </span>
      </div>
      <div class="action-body">${escapeHtml(JSON.stringify(args))}</div>
    `;
    messagesList.appendChild(card);
    chatContainer.scrollTop = chatContainer.scrollHeight;
  }

  async function handleSendPrompt(customText = null) {
    const prompt = (customText || promptInput.value).trim();
    if (!prompt || isGenerating) return;

    promptInput.value = '';
    appendUserMessage(prompt);
    conversationHistory.push({ role: 'user', content: prompt });

    isGenerating = true;
    sendBtn.classList.add('hidden');
    stopBtn.classList.remove('hidden');
    abortController = new AbortController();

    window.taskRuntime.setState(TaskState.RUNNING, 'AI 응답 및 작업 계획 생성 중...');

    try {
      // 1. Retrieve semantic memory context
      const relevantMemories = await window.agentMemory.searchRelevantContext(prompt);
      let memoryPrompt = '';
      if (relevantMemories.length > 0) {
        memoryPrompt = `\n[사용자의 과거 브라우징 기억 컨텍스트]\n` +
          relevantMemories.map(m => `- ${m.title} (${m.url}): ${m.content}`).join('\n') + `\n`;
      }

      const settings = AIProviderFactory.getSettings();
      const provider = AIProviderFactory.createProvider(settings);
      const systemPrompt = settings.systemPrompt + memoryPrompt;
      const tools = window.taskRuntime.getAvailableTools();

      let loopCount = 0;
      const maxLoops = 6;

      while (loopCount < maxLoops) {
        loopCount++;
        const bubble = createStreamingBubble();
        let toolCalls = [];

        await provider.chatStream({
          messages: conversationHistory,
          tools,
          systemPrompt,
          signal: abortController.signal,
          onStatus: (statusInfo) => {
            if (statusInfo.type === 'rate_limit_retry') {
              bubble.statusNotice.classList.remove('hidden');
              bubble.statusNotice.innerHTML = `<span>⏳</span><span>${escapeHtml(statusInfo.message)}</span>`;
              window.taskRuntime.setState(TaskState.RUNNING, statusInfo.message);
              chatContainer.scrollTop = chatContainer.scrollHeight;
            }
          },
          onThinking: (chunk, full) => {
            bubble.statusNotice.classList.add('hidden');
            bubble.thinkingBox.classList.remove('hidden');
            bubble.thinkingContent.textContent = full;
          },
          onChunk: (chunk, full) => {
            bubble.statusNotice.classList.add('hidden');
            bubble.contentDiv.innerHTML = formatMarkdown(full);
            chatContainer.scrollTop = chatContainer.scrollHeight;
          },
          onToolCall: (tc) => {
            bubble.statusNotice.classList.add('hidden');
            toolCalls.push(tc);
          }
        });

        const fullAssistantText = (bubble.contentDiv.innerText || '').trim();
        const hasThinking = !bubble.thinkingBox.classList.contains('hidden');

        // If turn only produced tool calls and no text or thinking, remove empty placeholder
        if (!fullAssistantText && !hasThinking) {
          bubble.container.remove();
        }

        conversationHistory.push({
          role: 'assistant',
          content: fullAssistantText || undefined,
          tool_calls: toolCalls.length > 0 ? toolCalls.map((tc, idx) => ({
            id: tc.id || ('call_' + idx + '_' + Math.random().toString(36).substring(7)),
            type: 'function',
            function: { 
              name: tc.name, 
              arguments: JSON.stringify(tc.args),
              thought_signature: tc.thought_signature || tc.thoughtSignature || undefined
            },
            thought_signature: tc.thought_signature || tc.thoughtSignature || undefined
          })) : undefined
        });

        // Index page or assistant summary in memory
        if (fullAssistantText && fullAssistantText.length > 20) {
          window.agentMemory.addMemory({
            type: 'ai_conversation',
            title: prompt.slice(0, 50),
            content: fullAssistantText
          });
          updateMemoryStats();
        }

        // If no tool calls, task loop is finished
        if (toolCalls.length === 0) {
          break;
        }

        window.taskRuntime.setState(TaskState.RUNNING, '도구 실행 중...');

        // Execute tool calls step by step
        for (const tc of toolCalls) {
          try {
            const toolResult = await window.taskRuntime.executeToolAction(tc.name, tc.args);
            appendToolCard(tc.name, tc.args, toolResult);

            conversationHistory.push({
              role: 'tool',
              tool_call_id: tc.id,
              name: tc.name,
              content: JSON.stringify(toolResult)
            });
          } catch (e) {
            appendToolCard(tc.name, tc.args, { isError: true, error: e.message });
            conversationHistory.push({
              role: 'tool',
              tool_call_id: tc.id,
              name: tc.name,
              content: JSON.stringify({ status: 'error', message: e.message })
            });
          }
        }

        window.taskRuntime.setState(TaskState.RUNNING, 'AI 답변 생성 중...');
      }

      window.taskRuntime.setState(TaskState.DONE, '작업 완료');
    } catch (err) {
      if (err.name !== 'AbortError') {
        appendAssistantMessage(`❌ 오류가 발생했습니다: ${err.message}`);
        window.taskRuntime.setState(TaskState.IDLE, err.message);
      }
    } finally {
      isGenerating = false;
      sendBtn.classList.remove('hidden');
      stopBtn.classList.add('hidden');
      abortController = null;
    }
  }

  sendBtn.addEventListener('click', () => handleSendPrompt());
  stopBtn.addEventListener('click', () => {
    if (abortController) abortController.abort();
    window.taskRuntime.stop();
  });

  promptInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSendPrompt();
    }
  });

  // Suggestion Chips Click
  document.querySelectorAll('.chip').forEach(chip => {
    chip.addEventListener('click', () => {
      const prompt = chip.getAttribute('data-prompt');
      if (prompt) handleSendPrompt(prompt);
    });
  });

  clearChatBtn.addEventListener('click', () => {
    conversationHistory = [];
    messagesList.innerHTML = '';
    if (welcomeCard) welcomeCard.style.display = 'block';
    window.taskRuntime.setState(TaskState.IDLE, '대화 리셋');
  });

  // Helpers
  function escapeHtml(str) {
    if (!str) return '';
    return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }

  function formatMarkdown(text) {
    if (!text) return '';
    return text
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/```([\s\S]*?)```/g, '<pre><code>$1</code></pre>')
      .replace(/`([^`]+)`/g, '<code>$1</code>')
      .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
      .replace(/\*([^*]+)\*/g, '<em>$1</em>')
      .replace(/\n/g, '<br>');
  }

  // Initial UI Setup
  loadSettingsIntoUI();
});
