/**
 * Lite Browser - Agent Memory & Vector / Semantic Context Store
 * Stores page summaries, browsing history, bookmarks, and form templates.
 * Provides user data controls (clear memory, disable indexing).
 */

class AgentMemoryStore {
  constructor() {
    this.dbName = 'LiteBrowserAgentMemory';
    this.dbVersion = 1;
    this.db = null;
    this.isIndexingEnabled = true;
    this.init();
  }

  async init() {
    try {
      const pref = localStorage.getItem('lite_browser_memory_indexing_enabled');
      if (pref !== null) {
        this.isIndexingEnabled = pref === 'true';
      }

      this.db = await new Promise((resolve, reject) => {
        const req = indexedDB.open(this.dbName, this.dbVersion);
        req.onupgradeneeded = (e) => {
          const db = e.target.result;
          if (!db.objectStoreNames.contains('memories')) {
            const store = db.createObjectStore('memories', { keyPath: 'id', autoIncrement: true });
            store.createIndex('type', 'type', { unique: false });
            store.createIndex('url', 'url', { unique: false });
            store.createIndex('timestamp', 'timestamp', { unique: false });
          }
        };
        req.onsuccess = () => resolve(req.result);
        req.onerror = () => reject(req.error);
      });
    } catch (e) {
      console.warn('AgentMemoryStore init warning:', e);
    }
  }

  setIndexingEnabled(enabled) {
    this.isIndexingEnabled = enabled;
    localStorage.setItem('lite_browser_memory_indexing_enabled', enabled ? 'true' : 'false');
  }

  // Generate lightweight character/word n-gram frequency vector
  static generateVector(text) {
    if (!text) return {};
    const tokens = text.toLowerCase().replace(/[^\w가-힣\s]/g, ' ').split(/\s+/).filter(t => t.length >= 2);
    const vec = {};
    for (const t of tokens) {
      vec[t] = (vec[t] || 0) + 1;
    }
    return vec;
  }

  // Calculate cosine similarity between two frequency vectors
  static cosineSimilarity(vecA, vecB) {
    let dot = 0;
    let normA = 0;
    let normB = 0;

    for (const key in vecA) {
      const valA = vecA[key];
      normA += valA * valA;
      if (vecB[key]) {
        dot += valA * vecB[key];
      }
    }
    for (const key in vecB) {
      normB += vecB[key] * vecB[key];
    }

    if (normA === 0 || normB === 0) return 0;
    return dot / (Math.sqrt(normA) * Math.sqrt(normB));
  }

  async addMemory({ type, title, url, content, tags = [] }) {
    if (!this.isIndexingEnabled || !this.db || !content) return;

    try {
      const vector = AgentMemoryStore.generateVector(`${title} ${content} ${tags.join(' ')}`);
      const record = {
        type: type || 'page_summary',
        title: title || '',
        url: url || '',
        content: content.slice(0, 1500),
        tags,
        vector,
        timestamp: Date.now()
      };

      const tx = this.db.transaction('memories', 'readwrite');
      const store = tx.objectStore('memories');
      store.add(record);
    } catch (e) {
      console.warn('Failed to add memory record:', e);
    }
  }

  async searchRelevantContext(query, topK = 3) {
    if (!this.db || !query) return [];

    try {
      const queryVec = AgentMemoryStore.generateVector(query);
      const allRecords = await new Promise((resolve) => {
        const tx = this.db.transaction('memories', 'readonly');
        const store = tx.objectStore('memories');
        const req = store.getAll();
        req.onsuccess = () => resolve(req.result || []);
        req.onerror = () => resolve([]);
      });

      const scored = allRecords.map(rec => ({
        ...rec,
        score: AgentMemoryStore.cosineSimilarity(queryVec, rec.vector || {})
      })).filter(r => r.score > 0.05);

      scored.sort((a, b) => b.score - a.score);
      return scored.slice(0, topK);
    } catch (e) {
      console.warn('Memory search error:', e);
      return [];
    }
  }

  async getMemoryCount() {
    if (!this.db) return 0;
    return new Promise((resolve) => {
      try {
        const tx = this.db.transaction('memories', 'readonly');
        const store = tx.objectStore('memories');
        const req = store.count();
        req.onsuccess = () => resolve(req.result || 0);
        req.onerror = () => resolve(0);
      } catch (e) {
        resolve(0);
      }
    });
  }

  async clearAllMemory() {
    if (!this.db) return true;
    return new Promise((resolve) => {
      try {
        const tx = this.db.transaction('memories', 'readwrite');
        const store = tx.objectStore('memories');
        const req = store.clear();
        req.onsuccess = () => resolve(true);
        req.onerror = () => resolve(false);
      } catch (e) {
        resolve(false);
      }
    });
  }
}

window.AgentMemoryStore = AgentMemoryStore;
window.agentMemory = new AgentMemoryStore();
