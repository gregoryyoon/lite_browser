let loading = null;
/** mermaid is large: load it only when a document actually has a diagram. */
const loadMermaid = () => (loading ??= import('mermaid').then((m) => m.default));

// mermaid.initialize/render use global state, so renders run one at a time.
let queue = Promise.resolve();
let renderCount = 0;
const svgCache = new Map();
const CACHE_LIMIT = 100;

/**
 * Renders mermaid source to SVG markup; rejects with mermaid's message on a syntax error.
 * @param {string} source
 * @param {boolean} dark
 * @returns {Promise<string>}
 */
export function renderMermaid(source, dark) {
  const theme = dark ? 'dark' : 'default';
  const key = `${theme}\n${source}`;
  const cached = svgCache.get(key);
  if (cached) return Promise.resolve(cached);
  const run = queue.then(async () => {
    const mermaid = await loadMermaid();
    mermaid.initialize({ startOnLoad: false, theme, securityLevel: 'strict' });
    await mermaid.parse(source); // throws on syntax errors without leaving error graphics in the page
    const { svg } = await mermaid.render(`mermaid-svg-${++renderCount}`, source);
    if (svgCache.size >= CACHE_LIMIT) svgCache.delete(svgCache.keys().next().value);
    svgCache.set(key, svg);
    return svg;
  });
  queue = run.catch(() => {});
  return run;
}

const escapeHtml = (s) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

/**
 * Draws `source` into `el` (a `.mermaid-diagram` box), or shows the error there. Ignores stale results.
 * @param {HTMLElement} el
 * @param {string} source
 * @param {boolean} dark
 */
export async function drawDiagram(el, source, dark) {
  el.dataset.source = source;
  el.dataset.theme = dark ? 'dark' : 'light';
  try {
    const svg = await renderMermaid(source, dark);
    if (el.dataset.source === source) el.innerHTML = svg;
  } catch (e) {
    if (el.dataset.source !== source) return;
    const message = e instanceof Error ? e.message : String(e);
    el.innerHTML = `<div class="mermaid-error">mermaid 오류: ${escapeHtml(message)}</div>`;
  }
}

/**
 * Re-draws every diagram under `root` whose theme differs (after a theme switch).
 * @param {HTMLElement} root
 * @param {boolean} dark
 */
export function redrawDiagrams(root, dark) {
  const theme = dark ? 'dark' : 'light';
  root.querySelectorAll('.mermaid-diagram').forEach((el) => {
    if (el.dataset.source !== undefined && el.dataset.theme !== theme) void drawDiagram(el, el.dataset.source, dark);
  });
}
