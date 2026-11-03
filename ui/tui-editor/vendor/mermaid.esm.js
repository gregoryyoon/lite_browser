import './mermaid.min.js';
export default (typeof window !== 'undefined' ? window.mermaid : (typeof globalThis !== 'undefined' ? globalThis.mermaid : undefined));
