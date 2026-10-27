/**
 * Unit tests for Stage 5: Visual Heavy & Chart Context Extractor
 */
const assert = require('assert');
const ContentExtractor = require('../ui/content_extractor.js');

console.log('--- [Test 1: ContentExtractor.detectVisualContext] ---');

// Mock DOM Helper
function createMockElement(tag, attrs = {}, children = [], text = '') {
  return {
    tagName: tag.toUpperCase(),
    className: attrs.class || '',
    id: attrs.id || '',
    getAttribute(name) { return attrs[name] || null; },
    hasAttribute(name) { return Object.prototype.hasOwnProperty.call(attrs, name); },
    children,
    childNodes: children,
    textContent: text,
    innerText: text,
    width: attrs.width || 0,
    height: attrs.height || 0,
    getBoundingClientRect() {
      return { width: attrs.width || 0, height: attrs.height || 0, top: 0, left: 0 };
    },
    getContext(type) {
      if (attrs.webgl) return {};
      return null;
    }
  };
}

function createMockDoc({ canvases = [], svgs = [], svgNodes = [], chartContainers = [], bodyText = '' } = {}) {
  const elements = [];
  canvases.forEach(c => elements.push(c));
  svgs.forEach(s => elements.push(s));
  chartContainers.forEach(cc => elements.push(cc));

  return {
    body: {
      innerText: bodyText,
      textContent: bodyText
    },
    querySelectorAll(selector) {
      if (selector === 'canvas') return canvases;
      if (selector === 'svg') return svgs;
      if (selector === 'svg path, svg circle, svg rect, svg polyline') return svgNodes;
      if (selector.includes('chart') || selector.includes('dashboard')) return chartContainers;
      return [];
    },
    querySelector(selector) {
      if (selector.includes('.tradingview-widget-container')) {
        return chartContainers.find(el => (el.className || '').includes('tradingview-widget-container')) || null;
      }
      if (selector.includes('#tv-chart-container')) {
        return chartContainers.find(el => el.id === 'tv-chart-container') || null;
      }
      if (selector.includes('chart') || selector.includes('dashboard') || selector.includes('graph')) {
        return chartContainers[0] || null;
      }
      return null;
    }
  };
}

// Test Case 1: Canvas present with short text (<250 chars)
{
  const mockCanvas = createMockElement('canvas', { width: 800, height: 600 });
  const doc = createMockDoc({ canvases: [mockCanvas], bodyText: '코스피 지수 2,500p' });
  const res = ContentExtractor.detectVisualContext(doc, {}, 20);

  assert.strictEqual(res.isVisualHeavy, true, 'Short text + canvas should be visual heavy');
  assert.ok(res.reason.some(r => r.includes('캔버스')), 'Reason should mention canvas');
  console.log('✓ Case 1 Passed: Short text (<250) + Canvas -> isVisualHeavy: true');
}

// Test Case 2: Standard article text (>=250 chars) without charts
{
  const longText = '가'.repeat(300);
  const doc = createMockDoc({ bodyText: longText });
  const res = ContentExtractor.detectVisualContext(doc, {}, longText.length);

  assert.strictEqual(res.isVisualHeavy, false, 'Standard article should not be visual heavy');
  assert.strictEqual(res.reason.length, 0);
  console.log('✓ Case 2 Passed: Standard article text (>=250) -> isVisualHeavy: false');
}

// Test Case 3: Dense SVG charts (>= 3 svgs or >= 10 data nodes)
{
  const svgs = [createMockElement('svg'), createMockElement('svg'), createMockElement('svg')];
  const svgNodes = Array.from({ length: 15 }, () => createMockElement('path'));
  const doc = createMockDoc({ svgs, svgNodes, bodyText: '대시보드 요약' });
  const res = ContentExtractor.detectVisualContext(doc, {}, 10);

  assert.strictEqual(res.isVisualHeavy, true, 'Dense SVG chart should be visual heavy');
  assert.ok(res.reason.some(r => r.includes('SVG')), 'Reason should mention SVG');
  console.log('✓ Case 3 Passed: Dense SVG chart nodes -> isVisualHeavy: true');
}

// Test Case 4: Explicit Chart Widget Container (TradingView / Grafana)
{
  const tvContainer = createMockElement('div', { class: 'tradingview-widget-container' });
  const doc = createMockDoc({ chartContainers: [tvContainer], bodyText: '비트코인 1시간봉 차트' });
  const res = ContentExtractor.detectVisualContext(doc, {}, 500);

  assert.strictEqual(res.isVisualHeavy, true, 'Explicit chart container selector should be visual heavy regardless of text length');
  assert.ok(res.reason.some(r => r.includes('위젯 컨테이너')), 'Reason should mention widget container');
  console.log('✓ Case 4 Passed: Explicit .tradingview-widget-container -> isVisualHeavy: true');
}

// Test Case 5: Empty document or invalid input error resilience
{
  const res1 = ContentExtractor.detectVisualContext(null, null, 0);
  assert.strictEqual(res1.isVisualHeavy, false);
  assert.deepStrictEqual(res1.reason, []);

  const res2 = ContentExtractor.detectVisualContext({}, null, 0);
  assert.strictEqual(res2.isVisualHeavy, false);
  assert.deepStrictEqual(res2.reason, []);
  console.log('✓ Case 5 Passed: Null/invalid input handles safely without throwing');
}

// Test Case 6: Long article (>= 250 chars) with generic chart icon class should NOT trigger visual heavy false positive
{
  const genericChartEl = createMockElement('i', { class: 'icon-chart-bar' });
  const doc = {
    body: { innerText: '기사 본문'.repeat(100), textContent: '기사 본문'.repeat(100) },
    querySelectorAll: () => [],
    querySelector: (s) => (s === '[class*="chart"]' ? genericChartEl : null)
  };
  const res = ContentExtractor.detectVisualContext(doc, {}, 1000);
  assert.strictEqual(res.isVisualHeavy, false, 'Long article with generic chart icon must not be visual heavy');
  console.log('✓ Case 6 Passed: Long article with generic chart icon -> isVisualHeavy: false (false-positive prevented)');
}

console.log('All ContentExtractor Stage 5 tests passed successfully!');
