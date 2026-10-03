/**
 * Lite Browser - AI Provider Abstraction Layer
 * Supports Google Gemini (gemini-3.7-flash), OpenAI, Anthropic Claude, and Ollama.
 */

// Base Provider Interface
class AIProviderInterface {
  constructor(config = {}) {
    this.apiKey = config.apiKey || '';
    this.authType = config.authType || 'apikey';
    this.subscriptionToken = config.subscriptionToken || '';
    this.model = config.model || '';
    this.baseUrl = config.baseUrl || '';
    this.temperature = config.temperature ?? 0.7;
  }

  async chatStream({ messages, tools, systemPrompt, onChunk, onThinking, onToolCall, onStatus, onComplete, onError, signal }) {
    throw new Error('chatStream method must be implemented by subclasses');
  }
}

// Helper for resilient fetch with exponential backoff on 429 Rate Limit
async function fetchWithBackoff(url, fetchOptions, providerName = 'AI', onStatus = null) {
  const signal = fetchOptions.signal;
  const maxRetries = 3;

  for (let attempt = 0; attempt <= maxRetries; attempt++) {
    if (signal?.aborted) {
      throw new DOMException('Aborted by user', 'AbortError');
    }

    const response = await fetch(url, fetchOptions);

    if (response.ok) {
      return response;
    }

    const errText = await response.text();

    if (response.status === 429 && attempt < maxRetries) {
      let delayMs = 1500 * Math.pow(2, attempt);
      const retryMatch = errText.match(/retry in\s+([\d\.]+)\s*(ms|s)/i);
      if (retryMatch) {
        const val = parseFloat(retryMatch[1]);
        const unit = retryMatch[2].toLowerCase();
        delayMs = Math.max(1000, unit === 's' ? val * 1000 : val) + 500;
      }

      const retrySec = (delayMs / 1000).toFixed(1);
      const retryMsg = `⚠️ [${providerName} 429] 요청 한도(Rate Limit)에 도달했습니다. ${retrySec}초 후 자동으로 재시도합니다... (${attempt + 1}/${maxRetries})`;
      console.warn(retryMsg);

      if (onStatus) {
        onStatus({
          type: 'rate_limit_retry',
          provider: providerName,
          status: 429,
          attempt: attempt + 1,
          maxRetries,
          delayMs,
          message: retryMsg,
          rawError: errText
        });
      }

      await new Promise((resolve, reject) => {
        let timer = null;
        const onAbort = () => {
          if (timer) clearTimeout(timer);
          reject(new DOMException('Aborted by user', 'AbortError'));
        };
        if (signal?.aborted) return onAbort();
        if (signal) signal.addEventListener('abort', onAbort, { once: true });
        timer = setTimeout(() => {
          if (signal) signal.removeEventListener('abort', onAbort);
          resolve();
        }, delayMs);
      });

      continue;
    }

    throw new Error(`${providerName} API 오류 (${response.status}): ${errText}`);
  }
}

// Universal AI Proxy Stream Runner via native C WinHTTP (Bypasses browser CORS & handles SSE)
function runAIProxyStream({ url, headers = {}, body, signal, onRawLine, onStatus, onComplete, onError }) {
  return new Promise((resolve, reject) => {
    const reqId = 'req_' + Date.now() + '_' + Math.random().toString(36).substring(2, 8);
    let sseBuffer = '';
    let isFinished = false;
    const decoder = new TextDecoder('utf-8', { stream: true });

    const cleanup = () => {
      isFinished = true;
      if (window._aiProxyCallbacks && window._aiProxyCallbacks[reqId]) {
        delete window._aiProxyCallbacks[reqId];
      }
      if (signal) {
        signal.removeEventListener('abort', onAbort);
      }
    };

    const onAbort = () => {
      if (isFinished) return;
      window.location.href = `http://ui-action/ai-proxy-cancel?req_id=${encodeURIComponent(reqId)}`;
      cleanup();
      const abortErr = new DOMException('Aborted by user', 'AbortError');
      if (onError) onError(abortErr);
      reject(abortErr);
    };

    if (signal?.aborted) return onAbort();
    if (signal) signal.addEventListener('abort', onAbort, { once: true });

    if (!window._aiProxyCallbacks) {
      window._aiProxyCallbacks = {};
      window.onAIProxyChunk = (id, b64Chunk) => {
        const handler = window._aiProxyCallbacks[id];
        if (handler && handler.handleChunk) handler.handleChunk(b64Chunk);
      };
      window.onAIProxyDone = (id) => {
        const handler = window._aiProxyCallbacks[id];
        if (handler && handler.handleDone) handler.handleDone();
      };
      window.onAIProxyError = (id, status, b64Err) => {
        const handler = window._aiProxyCallbacks[id];
        if (handler && handler.handleError) handler.handleError(status, b64Err);
      };
    }

    window._aiProxyCallbacks[reqId] = {
      handleChunk: (b64Chunk) => {
        if (isFinished) return;
        try {
          const rawBytes = Uint8Array.from(atob(b64Chunk), c => c.charCodeAt(0));
          const textChunk = decoder.decode(rawBytes, { stream: true });
          sseBuffer += textChunk;
          const lines = sseBuffer.split('\n');
          sseBuffer = lines.pop(); // keep partial line
          for (const line of lines) {
            if (onRawLine) onRawLine(line);
          }
        } catch (e) {
          console.warn('AI Proxy Chunk Decode Warning:', e);
        }
      },
      handleDone: () => {
        if (isFinished) return;
        const remainder = decoder.decode();
        if (remainder) sseBuffer += remainder;
        if (sseBuffer.trim() && onRawLine) {
          onRawLine(sseBuffer.trim());
        }
        cleanup();
        if (onComplete) onComplete();
        resolve();
      },
      handleError: (status, b64Err) => {
        if (isFinished) return;
        cleanup();
        let errMsg = `HTTP ${status}`;
        try {
          const rawBytes = Uint8Array.from(atob(b64Err), c => c.charCodeAt(0));
          errMsg = new TextDecoder('utf-8').decode(rawBytes);
        } catch(e) {}
        const err = new Error(`AI 프록시 통신 오류 (${status}): ${errMsg}`);
        if (onError) onError(err);
        reject(err);
      }
    };

    // Serialize headers into CRLF string
    let headersStr = '';
    for (const [k, v] of Object.entries(headers)) {
      if (v !== undefined && v !== null) {
        headersStr += `${k}: ${v}\r\n`;
      }
    }

    const bodyStr = typeof body === 'string' ? body : JSON.stringify(body);
    const urlB64 = btoa(unescape(encodeURIComponent(url)));
    const headersB64 = btoa(unescape(encodeURIComponent(headersStr)));
    const bodyB64 = btoa(unescape(encodeURIComponent(bodyStr)));

    window.location.href = `http://ui-action/ai-proxy-stream-start?req_id=${encodeURIComponent(reqId)}&url_b64=${encodeURIComponent(urlB64)}&headers_b64=${encodeURIComponent(headersB64)}&body_b64=${encodeURIComponent(bodyB64)}`;
  });
}

// 1. Google Gemini Provider
class GeminiProvider extends AIProviderInterface {
  constructor(config = {}) {
    super(config);
    this.model = config.model || 'gemini-3.8-flash';
  }

  async chatStream({ messages, tools, systemPrompt, onChunk, onThinking, onToolCall, onStatus, onComplete, onError, signal }) {
    try {
      let url = '';
      const headers = { 'Content-Type': 'application/json' };

      if (this.authType === 'subscription') {
        if (!this.subscriptionToken) {
          throw new Error('Google/Gemini 구독 계정이 연결되지 않았습니다. 설정(⚙️)에서 로그인해주세요.');
        }

        if (this.subscriptionToken.startsWith('ya29.')) {
          url = `https://generativelanguage.googleapis.com/v1beta/models/${this.model}:streamGenerateContent?alt=sse`;
          headers['Authorization'] = `Bearer ${this.subscriptionToken}`;
        } else if (this.subscriptionToken.startsWith('AIza') || this.subscriptionToken.length >= 20) {
          url = `https://generativelanguage.googleapis.com/v1beta/models/${this.model}:streamGenerateContent?alt=sse&key=${encodeURIComponent(this.subscriptionToken)}`;
        } else if (this.apiKey) {
          url = `https://generativelanguage.googleapis.com/v1beta/models/${this.model}:streamGenerateContent?alt=sse&key=${encodeURIComponent(this.apiKey)}`;
        } else {
          throw new Error('Google Gemini API 통신을 위해 Google AI Studio 무료 키(AIza...) 또는 Google OAuth 토큰이 필요합니다. 설정(⚙️)에서 등록해주세요.');
        }
      } else {
        if (!this.apiKey) throw new Error('Gemini API 키가 설정되지 않았습니다. 설정(⚙️)에서 입력해주세요.');
        url = `https://generativelanguage.googleapis.com/v1beta/models/${this.model}:streamGenerateContent?alt=sse&key=${encodeURIComponent(this.apiKey)}`;
      }

      // Convert messages to Gemini format
      const contents = [];
      for (const msg of messages) {
        if (msg.role === 'system') continue;

        if (msg.role === 'tool') {
          let parsedResponse = {};
          try {
            parsedResponse = typeof msg.content === 'string' ? JSON.parse(msg.content) : msg.content;
          } catch (e) {
            parsedResponse = { content: msg.content };
          }
          contents.push({
            role: 'user',
            parts: [{
              functionResponse: {
                name: msg.name || 'tool_response',
                response: {
                  name: msg.name || 'tool_response',
                  content: parsedResponse
                }
              }
            }]
          });
          continue;
        }

        if (msg.role === 'assistant') {
          const parts = [];
          if (msg.content && typeof msg.content === 'string' && msg.content.trim().length > 0) {
            parts.push({ text: msg.content });
          }
          if (msg.tool_calls && Array.isArray(msg.tool_calls)) {
            for (const tc of msg.tool_calls) {
              const fn = tc.function || tc;
              let args = {};
              try {
                args = typeof fn.arguments === 'string' ? JSON.parse(fn.arguments || '{}') : (fn.arguments || fn.args || {});
              } catch (e) {
                args = {};
              }
              const partObj = {
                functionCall: {
                  name: fn.name,
                  args: args
                }
              };
              const sig = tc.thought_signature || tc.thoughtSignature || fn.thought_signature || fn.thoughtSignature;
              if (sig) {
                partObj.thought_signature = sig;
              } else {
                partObj.thought_signature = 'skip_thought_signature_validator';
              }
              parts.push(partObj);
            }
          }
          if (parts.length > 0) {
            contents.push({ role: 'model', parts });
          }
          continue;
        }

        if (msg.role === 'user') {
          const parts = [];
          if (msg.content) {
            parts.push({ text: typeof msg.content === 'string' ? msg.content : JSON.stringify(msg.content) });
          }
          if (parts.length > 0) {
            contents.push({ role: 'user', parts });
          }
        }
      }

      // Convert tools to Gemini format
      const geminiTools = [];
      if (tools && tools.length > 0) {
        const functionDeclarations = tools.map(t => ({
          name: t.name,
          description: t.description,
          parameters: t.parameters || t.inputSchema || { type: 'object', properties: {} }
        }));
        geminiTools.push({ functionDeclarations });
      }

      const body = {
        contents,
        generationConfig: {
          temperature: this.temperature,
          maxOutputTokens: 8192
        }
      };

      if (systemPrompt) {
        body.systemInstruction = {
          parts: [{ text: systemPrompt }]
        };
      }
      if (geminiTools.length > 0) {
        body.tools = geminiTools;
      }

      let fullText = '';
      let accumulatedThinking = '';

      const processLine = (line) => {
        const trimmed = line.trim();
        if (!trimmed || !trimmed.startsWith('data: ')) return;
        const jsonStr = trimmed.substring(6).trim();
        if (jsonStr === '[DONE]') return;

        try {
          const data = JSON.parse(jsonStr);
          const candidates = data.candidates || [];
          if (candidates.length > 0) {
            const parts = candidates[0].content?.parts || [];
            for (const part of parts) {
              if (part.thought && onThinking) {
                accumulatedThinking += part.thought;
                onThinking(part.thought, accumulatedThinking);
              }
              if (part.text && onChunk) {
                fullText += part.text;
                onChunk(part.text, fullText);
              }
              if (part.functionCall && onToolCall) {
                onToolCall({
                  name: part.functionCall.name,
                  args: part.functionCall.args || {},
                  thought_signature: part.thought_signature || part.thoughtSignature || (part.functionCall ? (part.functionCall.thought_signature || part.functionCall.thoughtSignature) : null) || null
                });
              }
            }
          }
        } catch (e) {
          console.warn('Gemini stream parse warning:', e);
        }
      };

      if (this.authType === 'subscription') {
        await runAIProxyStream({
          url,
          headers,
          body,
          signal,
          onRawLine: processLine,
          onStatus,
          onComplete: () => {
            if (onComplete) onComplete({ fullText, accumulatedThinking });
          },
          onError
        });
        return;
      }

      const response = await fetchWithBackoff(url, {
        method: 'POST',
        headers: headers,
        body: JSON.stringify(body),
        signal
      }, 'Gemini', onStatus);

      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let buffer = '';

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split('\n');
        buffer = lines.pop(); // keep partial line

        for (const line of lines) {
          processLine(line);
        }
      }

      if (onComplete) onComplete({ fullText, accumulatedThinking });
    } catch (err) {
      if (err.name === 'AbortError') {
        if (onComplete) onComplete({ fullText: '', interrupted: true });
      } else {
        if (onError) onError(err);
        throw err;
      }
    }
  }
}

// 2. OpenAI Provider
class OpenAIProvider extends AIProviderInterface {
  constructor(config = {}) {
    super(config);
    this.model = config.model || 'gpt-5.6-terra';
    this.baseUrl = config.baseUrl || 'https://api.openai.com/v1';
  }

  async chatStream({ messages, tools, systemPrompt, onChunk, onThinking, onToolCall, onStatus, onComplete, onError, signal }) {
    try {
      let url = '';
      const headers = {
        'Content-Type': 'application/json',
        'Accept': 'text/event-stream'
      };
      let body = {};

      if (this.authType === 'subscription') {
        if (!this.subscriptionToken) throw new Error('OpenAI ChatGPT 구독 계정이 연결되지 않았습니다. 설정(⚙️)에서 구독 연결을 진행해주세요.');
        headers['Authorization'] = `Bearer ${this.subscriptionToken}`;
        headers['User-Agent'] = 'codex-cli/1.0';
        url = 'https://chatgpt.com/backend-api/codex/responses';

        // Responses API format: input array and flattened tools
        const inputItems = [];
        for (const msg of messages) {
          if (msg.role === 'tool') {
            inputItems.push({
              type: 'function_call_output',
              call_id: msg.tool_call_id || msg.id || ('call_' + (msg.name || 'tool')),
              output: typeof msg.content === 'string' ? msg.content : JSON.stringify(msg.content)
            });
          } else if (msg.role === 'assistant') {
            if (msg.content) {
              inputItems.push({ role: 'assistant', content: msg.content });
            }
            if (msg.tool_calls && msg.tool_calls.length > 0) {
              for (const tc of msg.tool_calls) {
                const fn = tc.function || tc;
                inputItems.push({
                  type: 'function_call',
                  call_id: tc.id || ('call_' + (fn.name || 'tool')),
                  name: fn.name,
                  arguments: typeof fn.arguments === 'string' ? fn.arguments : JSON.stringify(fn.arguments || {})
                });
              }
            }
          } else if (msg.role === 'user') {
            inputItems.push({ role: 'user', content: msg.content });
          }
        }

        const codexTools = tools ? tools.map(t => ({
          type: 'function',
          name: t.name,
          description: t.description,
          parameters: t.parameters || t.inputSchema || {}
        })) : undefined;

        body = {
          model: this.model,
          instructions: systemPrompt || undefined,
          input: inputItems,
          stream: true,
          store: false,
          tools: codexTools
        };
      } else {
        if (!this.apiKey) throw new Error('OpenAI API 키가 설정되지 않았습니다. 설정(⚙️)에서 입력해주세요.');
        headers['Authorization'] = `Bearer ${this.apiKey}`;
        url = `${this.baseUrl}/chat/completions`;

        const fullMessages = [];
        if (systemPrompt) {
          fullMessages.push({ role: 'system', content: systemPrompt });
        }

        for (const msg of messages) {
          if (msg.role === 'tool') {
            fullMessages.push({
              role: 'tool',
              tool_call_id: msg.tool_call_id || msg.id || ('call_' + (msg.name || 'tool')),
              name: msg.name,
              content: typeof msg.content === 'string' ? msg.content : JSON.stringify(msg.content)
            });
          } else if (msg.role === 'assistant') {
            const m = { role: 'assistant', content: msg.content || null };
            if (msg.tool_calls && msg.tool_calls.length > 0) {
              m.tool_calls = msg.tool_calls;
            }
            fullMessages.push(m);
          } else if (msg.role === 'user') {
            fullMessages.push({ role: 'user', content: msg.content });
          }
        }

        const openaiTools = tools ? tools.map(t => ({
          type: 'function',
          function: {
            name: t.name,
            description: t.description,
            parameters: t.parameters || t.inputSchema || {}
          }
        })) : undefined;

        body = {
          model: this.model,
          messages: fullMessages,
          temperature: this.temperature,
          stream: true,
          tools: openaiTools
        };
      }

      let fullText = '';
      let accumulatedThinking = '';
      const toolCallsMap = {};

      const processLine = (line) => {
        const trimmed = line.trim();
        if (!trimmed || !trimmed.startsWith('data: ')) return;
        const jsonStr = trimmed.substring(6).trim();
        if (jsonStr === '[DONE]') return;

        try {
          const data = JSON.parse(jsonStr);

          // 1. Standard OpenAI Chat Completions SSE format
          const delta = data.choices?.[0]?.delta;
          if (delta) {
            if (delta.content && onChunk) {
              fullText += delta.content;
              onChunk(delta.content, fullText);
            }
            if (delta.tool_calls) {
              for (const tc of delta.tool_calls) {
                const idx = tc.index ?? 0;
                if (!toolCallsMap[idx]) {
                  toolCallsMap[idx] = { id: tc.id || '', name: tc.function?.name || '', argsStr: '' };
                }
                if (tc.id) toolCallsMap[idx].id = tc.id;
                if (tc.function?.name) toolCallsMap[idx].name = tc.function.name;
                if (tc.function?.arguments) toolCallsMap[idx].argsStr += tc.function.arguments;
              }
            }
          }

          // 2. OpenAI Codex Responses API SSE format
          const eventType = data.type || '';

          // Output text chunks (support all Responses API event variations)
          if (
            eventType === 'response.output_text.delta' ||
            eventType === 'response.text.delta' ||
            eventType === 'response.output_item.delta' ||
            eventType === 'response.content_part.delta'
          ) {
            const text = data.delta?.text || data.delta?.value || (typeof data.delta === 'string' ? data.delta : '') || data.text || '';
            if (text) {
              fullText += text;
              if (onChunk) onChunk(text, fullText);
            }
          }

          // Reasoning / thinking chunks
          if (
            eventType === 'response.reasoning_text.delta' ||
            eventType === 'response.reasoning.delta' ||
            eventType === 'response.thought.delta'
          ) {
            const thought = data.delta?.text || (typeof data.delta === 'string' ? data.delta : '') || data.text || '';
            if (thought) {
              accumulatedThinking += thought;
              if (onThinking) onThinking(thought, accumulatedThinking);
            }
          }

          // Function call item added
          if (eventType === 'response.output_item.added' && (data.item?.type === 'function_call' || data.item?.type === 'custom_tool_call')) {
            const callId = data.item.call_id || data.item.id || ('call_' + Object.keys(toolCallsMap).length);
            toolCallsMap[callId] = {
              id: data.item.call_id || callId,
              name: data.item.name || '',
              argsStr: data.item.arguments || ''
            };
          }

          // Function call arguments delta
          if (eventType === 'response.function_call_arguments.delta') {
            const keys = Object.keys(toolCallsMap);
            const targetKey = data.call_id || data.item_id || keys[keys.length - 1];
            if (targetKey && toolCallsMap[targetKey]) {
              toolCallsMap[targetKey].argsStr += (data.delta || data.arguments || '');
            }
          }

          // Function call item completed
          if (eventType === 'response.output_item.done' && data.item?.type === 'function_call') {
            const callId = data.item.call_id || data.item.id;
            if (callId && toolCallsMap[callId] && data.item.arguments) {
              toolCallsMap[callId].argsStr = data.item.arguments;
            }
          }

          // Response completed (full response fallback if deltas were somehow missed)
          if ((eventType === 'response.completed' || eventType === 'response.done') && data.response?.output) {
            for (const item of data.response.output) {
              if (item.type === 'message' && Array.isArray(item.content)) {
                for (const part of item.content) {
                  const partText = part.text || part.output_text || (typeof part === 'string' ? part : '');
                  if (partText && !fullText) {
                    fullText = partText;
                    if (onChunk) onChunk(partText, fullText);
                  }
                }
              } else if (item.type === 'function_call') {
                const callId = item.call_id || item.id || ('call_' + Object.keys(toolCallsMap).length);
                if (!toolCallsMap[callId]) {
                  toolCallsMap[callId] = {
                    id: item.call_id || callId,
                    name: item.name || '',
                    argsStr: item.arguments || ''
                  };
                }
              }
            }
          }
        } catch (e) {}
      };

      const dispatchToolCalls = () => {
        for (const idx in toolCallsMap) {
          const tc = toolCallsMap[idx];
          if (tc.name && onToolCall) {
            try {
              const args = JSON.parse(tc.argsStr || '{}');
              onToolCall({ id: tc.id, name: tc.name, args });
            } catch (e) {
              onToolCall({ id: tc.id, name: tc.name, args: {} });
            }
          }
        }
      };

      if (this.authType === 'subscription') {
        await runAIProxyStream({
          url,
          headers,
          body,
          signal,
          onRawLine: processLine,
          onStatus,
          onComplete: () => {
            dispatchToolCalls();
            if (onComplete) onComplete({ fullText, accumulatedThinking });
          },
          onError
        });
        return;
      }

      const response = await fetchWithBackoff(url, {
        method: 'POST',
        headers: headers,
        body: JSON.stringify(body),
        signal
      }, 'OpenAI', onStatus);

      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let buffer = '';

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split('\n');
        buffer = lines.pop();

        for (const line of lines) {
          processLine(line);
        }
      }

      dispatchToolCalls();
      if (onComplete) onComplete({ fullText, accumulatedThinking });
    } catch (err) {
      if (err.name === 'AbortError') {
        if (onComplete) onComplete({ fullText: '', interrupted: true });
      } else {
        if (onError) onError(err);
        throw err;
      }
    }
  }
}

// 3. Anthropic Claude Provider
class AnthropicProvider extends AIProviderInterface {
  constructor(config = {}) {
    super(config);
    this.model = config.model || 'claude-opus-4-7';
  }

  async chatStream({ messages, tools, systemPrompt, onChunk, onThinking, onToolCall, onStatus, onComplete, onError, signal }) {
    try {
      const headers = {
        'Content-Type': 'application/json',
        'anthropic-version': '2023-06-01',
        'anthropic-dangerous-direct-browser-access': 'true'
      };

      if (this.authType === 'subscription') {
        if (!this.subscriptionToken) throw new Error('Anthropic Claude 구독 계정이 연결되지 않았습니다. 설정(⚙️)에서 로그인해주세요.');
        if (this.subscriptionToken.startsWith('sk-ant-')) {
          headers['x-api-key'] = this.subscriptionToken;
        } else {
          headers['Authorization'] = `Bearer ${this.subscriptionToken}`;
        }
      } else {
        if (!this.apiKey) throw new Error('Anthropic API 키가 설정되지 않았습니다. 설정(⚙️)에서 입력해주세요.');
        headers['x-api-key'] = this.apiKey;
      }

      const url = 'https://api.anthropic.com/v1/messages';
      const anthropicTools = tools ? tools.map(t => ({
        name: t.name,
        description: t.description,
        input_schema: t.parameters || t.inputSchema || { type: 'object', properties: {} }
      })) : undefined;

      const formattedMessages = [];
      for (const msg of messages) {
        if (msg.role === 'system') continue;
        if (msg.role === 'tool') {
          formattedMessages.push({
            role: 'user',
            content: [{
              type: 'tool_result',
              tool_use_id: msg.tool_call_id || msg.id || 'tool_call_1',
              content: typeof msg.content === 'string' ? msg.content : JSON.stringify(msg.content)
            }]
          });
        } else if (msg.role === 'assistant') {
          const contents = [];
          if (msg.content) contents.push({ type: 'text', text: msg.content });
          if (msg.tool_calls) {
            for (const tc of msg.tool_calls) {
              const fn = tc.function || tc;
              let args = {};
              try {
                args = typeof fn.arguments === 'string' ? JSON.parse(fn.arguments || '{}') : (fn.arguments || fn.args || {});
              } catch (e) {
                args = {};
              }
              contents.push({
                type: 'tool_use',
                id: tc.id || 'tool_call_1',
                name: fn.name,
                input: args
              });
            }
          }
          formattedMessages.push({ role: 'assistant', content: contents.length > 0 ? contents : [{ type: 'text', text: ' ' }] });
        } else {
          formattedMessages.push({ role: 'user', content: msg.content });
        }
      }

      const body = {
        model: this.model,
        max_tokens: 4096,
        system: systemPrompt || undefined,
        messages: formattedMessages,
        tools: anthropicTools,
        stream: true
      };

      let fullText = '';
      let thinkingText = '';
      let currentTool = null;

      const processLine = (line) => {
        const trimmed = line.trim();
        if (!trimmed || !trimmed.startsWith('data: ')) return;
        const jsonStr = trimmed.substring(6).trim();

        try {
          const event = JSON.parse(jsonStr);
          if (event.type === 'content_block_start') {
            if (event.content_block?.type === 'tool_use') {
              currentTool = { id: event.content_block.id || 'tool_call_1', name: event.content_block.name, jsonStr: '' };
            }
          } else if (event.type === 'content_block_delta') {
            const delta = event.delta;
            if (delta.type === 'text_delta' && delta.text && onChunk) {
              fullText += delta.text;
              onChunk(delta.text, fullText);
            } else if (delta.type === 'thinking_delta' && delta.thinking && onThinking) {
              thinkingText += delta.thinking;
              onThinking(delta.thinking, thinkingText);
            } else if (delta.type === 'input_json_delta' && currentTool) {
              currentTool.jsonStr += delta.partial_json;
            }
          } else if (event.type === 'content_block_stop') {
            if (currentTool && onToolCall) {
              try {
                const args = JSON.parse(currentTool.jsonStr || '{}');
                onToolCall({ id: currentTool.id, name: currentTool.name, args });
              } catch (e) {
                onToolCall({ id: currentTool.id, name: currentTool.name, args: {} });
              }
              currentTool = null;
            }
          }
        } catch (e) {}
      };

      if (this.authType === 'subscription') {
        await runAIProxyStream({
          url,
          headers,
          body,
          signal,
          onRawLine: processLine,
          onStatus,
          onComplete: () => {
            if (onComplete) onComplete({ fullText, thinkingText });
          },
          onError
        });
        return;
      }

      const response = await fetchWithBackoff(url, {
        method: 'POST',
        headers: headers,
        body: JSON.stringify(body),
        signal
      }, 'Anthropic', onStatus);

      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let buffer = '';

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split('\n');
        buffer = lines.pop();

        for (const line of lines) {
          processLine(line);
        }
      }

      if (onComplete) onComplete({ fullText, thinkingText });
    } catch (err) {
      if (err.name === 'AbortError') {
        if (onComplete) onComplete({ fullText: '', interrupted: true });
      } else {
        if (onError) onError(err);
        throw err;
      }
    }
  }
}

// 4. Ollama Local Provider
class OllamaProvider extends AIProviderInterface {
  constructor(config = {}) {
    super(config);
    this.model = config.model || 'llama3.2';
    this.baseUrl = config.baseUrl || 'http://localhost:11434';
  }

  async chatStream({ messages, tools, systemPrompt, onChunk, onThinking, onToolCall, onStatus, onComplete, onError, signal }) {
    try {
      const url = `${this.baseUrl}/api/chat`;
      const fullMessages = [];
      if (systemPrompt) {
        fullMessages.push({ role: 'system', content: systemPrompt });
      }

      for (const msg of messages) {
        if (msg.role === 'tool') {
          let contentStr = typeof msg.content === 'string' ? msg.content : JSON.stringify(msg.content);
          try {
            const parsed = typeof msg.content === 'string' ? JSON.parse(msg.content) : msg.content;
            if (parsed && (parsed.url || parsed.title || parsed.bodySnippet)) {
              const u = parsed.url || '';
              const t = parsed.title || '';
              const b = parsed.bodySnippet || parsed.markdown || '';
              contentStr = `[도구 실행 성공 - 현재 브라우저 탭 정보]\n- 현재 페이지 URL: ${u}\n- 웹페이지 제목: ${t}\n- 본문 내용:\n${b}`;
            }
          } catch (e) {}

          fullMessages.push({
            role: 'tool',
            content: contentStr
          });
        } else if (msg.role === 'assistant') {
          const m = { role: 'assistant', content: msg.content || '' };
          if (msg.tool_calls && msg.tool_calls.length > 0) {
            m.tool_calls = msg.tool_calls.map(tc => {
              let argsObj = tc.function?.arguments || tc.args || {};
              if (typeof argsObj === 'string') {
                try {
                  argsObj = JSON.parse(argsObj);
                } catch (e) {
                  argsObj = {};
                }
              }
              return {
                function: {
                  name: tc.function?.name || tc.name,
                  arguments: argsObj
                }
              };
            });
          }
          fullMessages.push(m);
        } else if (msg.role === 'user') {
          fullMessages.push({ role: 'user', content: msg.content });
        }
      }

      const ollamaTools = tools && tools.length > 0 ? tools.map(t => ({
        type: 'function',
        function: {
          name: t.name,
          description: t.description,
          parameters: t.parameters || t.inputSchema || {}
        }
      })) : undefined;

      const body = {
        model: this.model,
        messages: fullMessages,
        stream: true
      };
      if (ollamaTools) {
        body.tools = ollamaTools;
      }

      const response = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
        signal
      });

      if (!response.ok) {
        let errDetail = '';
        try {
          const errJson = await response.json();
          errDetail = errJson.error || JSON.stringify(errJson);
        } catch (e) {
          try {
            errDetail = await response.text();
          } catch (e2) {}
        }
        throw new Error(`Ollama API 오류 (${response.status}): ${errDetail || '요청 처리 실패'}`);
      }

      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let buffer = '';
      let fullText = '';
      let thinkingText = '';
      let inThinking = false;
      const collectedToolCalls = [];

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split('\n');
        buffer = lines.pop();

        for (const line of lines) {
          const trimmed = line.trim();
          if (!trimmed) continue;
          try {
            const data = JSON.parse(trimmed);

            // Handle content / thinking chunks
            if (data.message?.content) {
              const chunk = data.message.content;
              if (chunk.includes('<think>')) {
                inThinking = true;
              }
              if (inThinking) {
                thinkingText += chunk;
                if (onThinking) onThinking(chunk, thinkingText);
                if (chunk.includes('</think>')) {
                  inThinking = false;
                }
              } else {
                fullText += chunk;
                if (onChunk) onChunk(chunk, fullText);
              }
            }

            // Handle Ollama tool calls
            if (data.message?.tool_calls && Array.isArray(data.message.tool_calls)) {
              for (const tc of data.message.tool_calls) {
                if (tc.function?.name) {
                  let fnArgs = tc.function.arguments;
                  if (typeof fnArgs === 'string') {
                    try { fnArgs = JSON.parse(fnArgs); } catch (e) { fnArgs = {}; }
                  } else if (!fnArgs || typeof fnArgs !== 'object') {
                    fnArgs = {};
                  }
                  collectedToolCalls.push({
                    id: 'call_' + Math.random().toString(36).substring(2, 9),
                    name: tc.function.name,
                    args: fnArgs
                  });
                }
              }
            }
          } catch (e) {}
        }
      }

      // Dispatch tool calls to task runtime
      for (const tc of collectedToolCalls) {
        if (tc.name && onToolCall) {
          onToolCall({ id: tc.id, name: tc.name, args: tc.args });
        }
      }

      if (onComplete) onComplete({ fullText, thinkingText });
    } catch (err) {
      if (err.name === 'AbortError') {
        if (onComplete) onComplete({ fullText: '', interrupted: true });
      } else {
        if (onError) onError(err);
        throw err;
      }
    }
  }
}

// AI Provider Factory & Key Store
class AIProviderFactory {
  static getSettings() {
    const defaultSettings = {
      provider: 'gemini',
      geminiAuthMode: 'subscription',
      openaiAuthMode: 'subscription',
      anthropicAuthMode: 'subscription',
      geminiKey: '',
      geminiModel: 'gemini-3.8-flash',
      openaiKey: '',
      openaiModel: 'gpt-5.6-terra',
      anthropicKey: '',
      anthropicModel: 'claude-opus-4-7',
      ollamaUrl: 'http://localhost:11434',
      ollamaModel: 'llama3.2',
      systemPrompt: '당신은 사용자의 웹 브라우징을 능동적으로 돕는 지능형 AI 브라우저 에이전트입니다. 사용자가 현재 페이지, URL, 웹페이지 본문, 요약, 검색 등을 질문하거나 요청하면 절대 브라우저를 볼 수 없다고 거절하지 말고, 즉시 제공된 브라우저 도구(browser_get_page_content 등)를 호출하여 정보를 확인한 뒤 완벽하게 답변하세요.'
    };

    try {
      const stored = localStorage.getItem('lite_browser_ai_settings');
      if (stored) return { ...defaultSettings, ...JSON.parse(stored) };
    } catch (e) {}
    return defaultSettings;
  }

  static saveSettings(settings) {
    try {
      localStorage.setItem('lite_browser_ai_settings', JSON.stringify(settings));
      return true;
    } catch (e) {
      return false;
    }
  }

  static createProvider(customSettings = null) {
    const settings = customSettings || this.getSettings();
    switch (settings.provider) {
      case 'openai':
        return new OpenAIProvider({
          apiKey: settings.openaiKey,
          authType: settings.authType || settings.openaiAuthMode || 'apikey',
          subscriptionToken: settings.subscriptionToken || '',
          model: settings.openaiModel
        });
      case 'anthropic':
        return new AnthropicProvider({
          apiKey: settings.anthropicKey,
          authType: settings.authType || settings.anthropicAuthMode || 'apikey',
          subscriptionToken: settings.subscriptionToken || '',
          model: settings.anthropicModel
        });
      case 'ollama':
        return new OllamaProvider({
          baseUrl: settings.ollamaUrl,
          model: settings.ollamaModel
        });
      case 'gemini':
      default:
        return new GeminiProvider({
          apiKey: settings.geminiKey,
          authType: settings.authType || settings.geminiAuthMode || 'apikey',
          subscriptionToken: settings.subscriptionToken || '',
          model: settings.geminiModel || 'gemini-3.8-flash'
        });
    }
  }
}

window.AIProviderInterface = AIProviderInterface;
window.AIProviderFactory = AIProviderFactory;
window.GeminiProvider = GeminiProvider;
window.OpenAIProvider = OpenAIProvider;
window.AnthropicProvider = AnthropicProvider;
window.OllamaProvider = OllamaProvider;
