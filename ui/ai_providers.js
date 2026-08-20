/**
 * Lite Browser - AI Provider Abstraction Layer
 * Supports Google Gemini (gemini-3.7-flash), OpenAI, Anthropic Claude, and Ollama.
 */

// Base Provider Interface
class AIProviderInterface {
  constructor(config = {}) {
    this.apiKey = config.apiKey || '';
    this.model = config.model || '';
    this.baseUrl = config.baseUrl || '';
    this.temperature = config.temperature ?? 0.7;
  }

  async chatStream({ messages, tools, systemPrompt, onChunk, onThinking, onToolCall, onComplete, onError, signal }) {
    throw new Error('chatStream method must be implemented by subclasses');
  }
}

// 1. Google Gemini Provider
class GeminiProvider extends AIProviderInterface {
  constructor(config = {}) {
    super(config);
    this.model = config.model || 'gemini-3.7-flash';
  }

  async chatStream({ messages, tools, systemPrompt, onChunk, onThinking, onToolCall, onComplete, onError, signal }) {
    try {
      if (!this.apiKey) throw new Error('Gemini API 키가 설정되지 않았습니다. 설정(⚙️)에서 입력해주세요.');

      const url = `https://generativelanguage.googleapis.com/v1beta/models/${this.model}:streamGenerateContent?alt=sse&key=${encodeURIComponent(this.apiKey)}`;

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
            role: 'function',
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
              parts.push({
                functionCall: {
                  name: fn.name,
                  args: args
                }
              });
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

      const response = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
        signal
      });

      if (!response.ok) {
        const errText = await response.text();
        throw new Error(`Gemini API 오류 (${response.status}): ${errText}`);
      }

      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let buffer = '';
      let fullText = '';
      let accumulatedThinking = '';

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split('\n');
        buffer = lines.pop(); // keep partial line

        for (const line of lines) {
          const trimmed = line.trim();
          if (!trimmed || !trimmed.startsWith('data: ')) continue;
          const jsonStr = trimmed.substring(6).trim();
          if (jsonStr === '[DONE]') continue;

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
                    args: part.functionCall.args || {}
                  });
                }
              }
            }
          } catch (e) {
            console.warn('Gemini stream parse warning:', e);
          }
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
    this.model = config.model || 'gpt-4o';
    this.baseUrl = config.baseUrl || 'https://api.openai.com/v1';
  }

  async chatStream({ messages, tools, systemPrompt, onChunk, onThinking, onToolCall, onComplete, onError, signal }) {
    try {
      if (!this.apiKey) throw new Error('OpenAI API 키가 설정되지 않았습니다. 설정(⚙️)에서 입력해주세요.');

      const url = `${this.baseUrl}/chat/completions`;
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

      const body = {
        model: this.model,
        messages: fullMessages,
        temperature: this.temperature,
        stream: true,
        tools: openaiTools
      };

      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${this.apiKey}`
        },
        body: JSON.stringify(body),
        signal
      });

      if (!response.ok) {
        const errText = await response.text();
        throw new Error(`OpenAI API 오류 (${response.status}): ${errText}`);
      }

      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let buffer = '';
      let fullText = '';
      const toolCallsMap = {};

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split('\n');
        buffer = lines.pop();

        for (const line of lines) {
          const trimmed = line.trim();
          if (!trimmed || !trimmed.startsWith('data: ')) continue;
          const jsonStr = trimmed.substring(6).trim();
          if (jsonStr === '[DONE]') continue;

          try {
            const data = JSON.parse(jsonStr);
            const delta = data.choices?.[0]?.delta;
            if (!delta) continue;

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
          } catch (e) {}
        }
      }

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

      if (onComplete) onComplete({ fullText });
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
    this.model = config.model || 'claude-3-7-sonnet-20250219';
  }

  async chatStream({ messages, tools, systemPrompt, onChunk, onThinking, onToolCall, onComplete, onError, signal }) {
    try {
      if (!this.apiKey) throw new Error('Anthropic API 키가 설정되지 않았습니다. 설정(⚙️)에서 입력해주세요.');

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

      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'x-api-key': this.apiKey,
          'anthropic-version': '2023-06-01',
          'anthropic-dangerous-direct-browser-access': 'true'
        },
        body: JSON.stringify(body),
        signal
      });

      if (!response.ok) {
        const errText = await response.text();
        throw new Error(`Anthropic API 오류 (${response.status}): ${errText}`);
      }

      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let buffer = '';
      let fullText = '';
      let thinkingText = '';
      let currentTool = null;

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        buffer += decoder.decode(value, { stream: true });
        const lines = buffer.split('\n');
        buffer = lines.pop();

        for (const line of lines) {
          const trimmed = line.trim();
          if (!trimmed || !trimmed.startsWith('data: ')) continue;
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

  async chatStream({ messages, tools, systemPrompt, onChunk, onThinking, onToolCall, onComplete, onError, signal }) {
    try {
      const url = `${this.baseUrl}/api/chat`;
      const fullMessages = [];
      if (systemPrompt) fullMessages.push({ role: 'system', content: systemPrompt });
      fullMessages.push(...messages);

      const body = {
        model: this.model,
        messages: fullMessages,
        stream: true
      };

      const response = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
        signal
      });

      if (!response.ok) {
        throw new Error(`Ollama API 연결 실패 (${response.status}). Ollama가 실행 중인지 확인하세요.`);
      }

      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let buffer = '';
      let fullText = '';

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
            if (data.message?.content && onChunk) {
              fullText += data.message.content;
              onChunk(data.message.content, fullText);
            }
          } catch (e) {}
        }
      }

      if (onComplete) onComplete({ fullText });
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
      geminiKey: '',
      geminiModel: 'gemini-3.7-flash',
      openaiKey: '',
      openaiModel: 'gpt-4o',
      anthropicKey: '',
      anthropicModel: 'claude-3-7-sonnet-20250219',
      ollamaUrl: 'http://localhost:11434',
      ollamaModel: 'llama3.2',
      systemPrompt: '당신은 사용자의 웹 브라우징을 능동적으로 돕는 지능형 AI 브라우저 에이전트입니다. 간결하고 정확하게 설명하고, 요청 시 도구를 적절히 활용하여 작업을 완수하세요.'
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
          model: settings.openaiModel
        });
      case 'anthropic':
        return new AnthropicProvider({
          apiKey: settings.anthropicKey,
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
          model: settings.geminiModel || 'gemini-3.7-flash'
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
