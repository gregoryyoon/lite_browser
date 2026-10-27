/**
 * Unit tests for Stage 5: Multimodal Vision Providers Payload Serialization
 */
const assert = require('assert');
const { GeminiProvider, OpenAIProvider, AnthropicProvider } = require('../ui/ai_providers.js');

console.log('--- [Test 2: Multimodal Vision Providers Payload Serialization] ---');

// Mock global fetch
let lastInterceptedRequest = null;
global.fetch = async (url, options) => {
  lastInterceptedRequest = {
    url,
    headers: options.headers,
    body: options.body ? JSON.parse(options.body) : null
  };
  return {
    ok: true,
    status: 200,
    headers: {
      get(header) { return 'text/event-stream'; }
    },
    body: {
      getReader() {
        let sent = false;
        return {
          async read() {
            if (sent) return { done: true, value: undefined };
            sent = true;
            return {
              done: false,
              value: new TextEncoder().encode('data: [DONE]\n\n')
            };
          }
        };
      }
    }
  };
};

const sampleJpegB64 = 'data:image/jpeg;base64,/9j/4AAQSkZJRgABAQEASABIAAD/';
const rawB64 = '/9j/4AAQSkZJRgABAQEASABIAAD/';

async function runTests() {
  // 1. GeminiProvider Multimodal Payload
  {
    const gemini = new GeminiProvider({ apiKey: 'AIzaSyTestKey123', model: 'gemini-2.5-flash' });
    const messages = [
      {
        role: 'user',
        content: '이 차트의 추세를 분석해줘',
        image: sampleJpegB64
      }
    ];

    await gemini.chatStream({ messages, onComplete: () => {} });

    assert.ok(lastInterceptedRequest, 'Request should be sent');
    const contents = lastInterceptedRequest.body.contents;
    assert.strictEqual(contents.length, 1);
    assert.strictEqual(contents[0].role, 'user');

    const inlineDataPart = contents[0].parts.find(p => p.inlineData);
    assert.ok(inlineDataPart, 'Gemini payload must include inlineData');
    assert.strictEqual(inlineDataPart.inlineData.mimeType, 'image/jpeg');
    assert.strictEqual(inlineDataPart.inlineData.data, rawB64);

    const textPart = contents[0].parts.find(p => p.text);
    assert.strictEqual(textPart.text, '이 차트의 추세를 분석해줘');

    console.log('✓ GeminiProvider User Image inlineData payload verified');

    // Test Gemini Tool response with screenshot
    const toolMessages = [
      { role: 'user', content: '차트 보여줘' },
      { role: 'assistant', content: '', tool_calls: [{ id: 'call_1', function: { name: 'browser_get_page_content', arguments: '{}' } }] },
      {
        role: 'tool',
        tool_call_id: 'call_1',
        name: 'browser_get_page_content',
        content: JSON.stringify({ status: 'success', title: 'TradingView', screenshot: sampleJpegB64 })
      }
    ];

    await gemini.chatStream({ messages: toolMessages, onComplete: () => {} });
    const toolContents = lastInterceptedRequest.body.contents;
    const toolTurn = toolContents[toolContents.length - 1];
    assert.strictEqual(toolTurn.role, 'user');
    assert.ok(toolTurn.parts.some(p => p.functionResponse), 'Must contain functionResponse');
    assert.ok(toolTurn.parts.some(p => p.inlineData && p.inlineData.data === rawB64), 'Must contain inlineData for tool screenshot');

    console.log('✓ GeminiProvider Tool Screenshot inlineData payload verified');
  }

  // 2. OpenAIProvider Multimodal Payload
  {
    const openai = new OpenAIProvider({ apiKey: 'sk-test-key-123', authType: 'apikey', model: 'gpt-4o' });
    const messages = [
      {
        role: 'user',
        content: '현재 차트 지표 해석',
        screenshot: sampleJpegB64
      }
    ];

    await openai.chatStream({ messages, onComplete: () => {} });

    assert.ok(lastInterceptedRequest, 'Request should be sent');
    const userMsg = lastInterceptedRequest.body.messages.find(m => m.role === 'user');
    assert.ok(Array.isArray(userMsg.content), 'OpenAI user content must be an array for multimodal');

    const textPart = userMsg.content.find(p => p.type === 'text');
    assert.strictEqual(textPart.text, '현재 차트 지표 해석');

    const imagePart = userMsg.content.find(p => p.type === 'image_url');
    assert.ok(imagePart, 'OpenAI user content must include image_url part');
    assert.strictEqual(imagePart.image_url.url, sampleJpegB64);
    assert.strictEqual(imagePart.image_url.detail, 'auto');

    console.log('✓ OpenAIProvider User Image image_url payload verified');

    // Test OpenAI Tool response with screenshot
    const toolMessages = [
      { role: 'user', content: '스크린샷 찍어줘' },
      { role: 'assistant', content: '', tool_calls: [{ id: 'call_snap', function: { name: 'browser_take_screenshot', arguments: '{}' } }] },
      {
        role: 'tool',
        tool_call_id: 'call_snap',
        name: 'browser_take_screenshot',
        content: JSON.stringify({ status: 'success', width: 1280, height: 800, screenshot: sampleJpegB64 })
      }
    ];

    await openai.chatStream({ messages: toolMessages, onComplete: () => {} });
    const reqMessages = lastInterceptedRequest.body.messages;
    const visualFollowup = reqMessages.find(m => m.role === 'user' && Array.isArray(m.content) && m.content.some(c => c.type === 'image_url'));
    assert.ok(visualFollowup, 'OpenAI must send visual user follow-up for tool screenshot');

    console.log('✓ OpenAIProvider Tool Screenshot image_url payload verified');
  }

  // 3. AnthropicProvider Multimodal Payload
  {
    const anthropic = new AnthropicProvider({ apiKey: 'sk-ant-test123', authType: 'apikey', model: 'claude-3-7-sonnet' });
    const messages = [
      {
        role: 'user',
        content: '차트 지지선과 저항선 찾아줘',
        image: sampleJpegB64
      }
    ];

    await anthropic.chatStream({ messages, onComplete: () => {} });

    assert.ok(lastInterceptedRequest, 'Request should be sent');
    const userMsg = lastInterceptedRequest.body.messages.find(m => m.role === 'user');
    assert.ok(Array.isArray(userMsg.content), 'Anthropic user content must be an array');

    const imagePart = userMsg.content.find(p => p.type === 'image');
    assert.ok(imagePart, 'Anthropic user content must include image part');
    assert.strictEqual(imagePart.source.type, 'base64');
    assert.strictEqual(imagePart.source.media_type, 'image/jpeg');
    assert.strictEqual(imagePart.source.data, rawB64);

    const textPart = userMsg.content.find(p => p.type === 'text');
    assert.strictEqual(textPart.text, '차트 지지선과 저항선 찾아줘');

    console.log('✓ AnthropicProvider User Image base64 source payload verified');

    // Test Anthropic Tool response with screenshot
    const toolMessages = [
      { role: 'user', content: '차트 캡처해줘' },
      { role: 'assistant', content: '', tool_calls: [{ id: 'tool_123', function: { name: 'browser_take_screenshot', arguments: '{}' } }] },
      {
        role: 'tool',
        tool_call_id: 'tool_123',
        name: 'browser_take_screenshot',
        content: JSON.stringify({ status: 'success', width: 1920, height: 1080, screenshot: sampleJpegB64 })
      }
    ];

    await anthropic.chatStream({ messages: toolMessages, onComplete: () => {} });
    const reqMessages = lastInterceptedRequest.body.messages;
    const toolMsg = reqMessages.find(m => m.role === 'user' && m.content && m.content[0]?.type === 'tool_result');
    assert.ok(toolMsg, 'Anthropic tool result turn must exist');
    const toolResultContent = toolMsg.content[0].content;
    assert.ok(Array.isArray(toolResultContent), 'tool_result content must be array with visual payload');
    const toolImagePart = toolResultContent.find(p => p.type === 'image');
    assert.ok(toolImagePart, 'tool_result must contain image block');
    const toolTextPart = toolResultContent.find(p => p.type === 'text');
    assert.ok(toolTextPart, 'tool_result must contain text block');
    assert.ok(!toolTextPart.text.includes(rawB64), 'tool_result text block must not bloat context with raw base64');

    console.log('✓ AnthropicProvider Tool Screenshot base64 source payload verified (no token bloat)');
  }

  console.log('All Multimodal Providers tests passed successfully!');
}

runTests().catch(err => {
  console.error('Test Failed:', err);
  process.exit(1);
});
