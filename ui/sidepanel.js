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
  const openaiModelSelect = document.getElementById('openai-model');
  const anthropicKeyInput = document.getElementById('anthropic-key');
  const anthropicModelSelect = document.getElementById('anthropic-model');
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

  // Auth Mode Toggles
  const authToggleBtns = document.querySelectorAll('.auth-toggle-btn');
  authToggleBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      const prov = btn.getAttribute('data-provider');
      const mode = btn.getAttribute('data-mode');
      document.querySelectorAll(`.auth-toggle-btn[data-provider="${prov}"]`).forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      setAuthModeUI(prov, mode);
    });
  });

  function setAuthModeUI(provider, mode) {
    const apikeyBox = document.getElementById(`${provider}-apikey-box`);
    const subBox = document.getElementById(`${provider}-sub-box`);
    if (apikeyBox && subBox) {
      apikeyBox.classList.toggle('hidden', mode === 'subscription');
      subBox.classList.toggle('hidden', mode !== 'subscription');
    }
    document.querySelectorAll(`.auth-toggle-btn[data-provider="${provider}"]`).forEach(b => {
      b.classList.toggle('active', b.getAttribute('data-mode') === mode);
    });
  }

  function requestAuthStatus() {
    window.location.href = 'http://ui-action/auth-get-status';
  }

  // --- PKCE OAuth 2.0 Utilities (Aside style) ---
  function generateCodeVerifier(length = 64) {
    const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~';
    const arr = new Uint8Array(length);
    if (window.crypto && window.crypto.getRandomValues) {
      window.crypto.getRandomValues(arr);
    } else {
      for (let i = 0; i < length; i++) arr[i] = Math.floor(Math.random() * 256);
    }
    let res = '';
    for (let i = 0; i < length; i++) res += chars[arr[i] % chars.length];
    return res;
  }

  async function generateCodeChallenge(verifier) {
    if (window.crypto && window.crypto.subtle && window.crypto.subtle.digest) {
      try {
        const data = new TextEncoder().encode(verifier);
        const hash = await window.crypto.subtle.digest('SHA-256', data);
        const b64 = btoa(String.fromCharCode(...new Uint8Array(hash)));
        return b64.replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
      } catch (e) {
        console.warn('WebCrypto subtle digest failed, falling back to pure JS SHA256:', e);
      }
    }
    return sha256Base64Url(verifier);
  }

  function sha256Base64Url(str) {
    function rightRotate(value, amount) {
      return (value >>> amount) | (value << (32 - amount));
    }
    const words = [];
    const asciiBitLength = str.length * 8;
    const hash = [
      0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    ];
    const k = [
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    ];
    for (let i = 0; i < str.length; i++) {
      words[i >> 2] |= (str.charCodeAt(i) & 255) << (8 * (3 - i % 4));
    }
    words[asciiBitLength >> 5] |= 0x80 << (24 - asciiBitLength % 32);
    words[(((asciiBitLength + 64) >> 9) << 4) + 15] = asciiBitLength;
    for (let i = 0; i < words.length; i += 16) {
      const oldHash = hash.slice(0);
      for (let j = 0; j < 64; j++) {
        let w_j;
        if (j < 16) {
          w_j = words[i + j] | 0;
        } else {
          const s0 = rightRotate(words[i + j - 15] | 0, 7) ^ rightRotate(words[i + j - 15] | 0, 18) ^ ((words[i + j - 15] | 0) >>> 3);
          const s1 = rightRotate(words[i + j - 2] | 0, 17) ^ rightRotate(words[i + j - 2] | 0, 19) ^ ((words[i + j - 2] | 0) >>> 10);
          w_j = ((words[i + j - 16] | 0) + s0 + (words[i + j - 7] | 0) + s1) | 0;
        }
        words[i + j] = w_j;
        const ch = (hash[4] & hash[5]) ^ (~hash[4] & hash[6]);
        const maj = (hash[0] & hash[1]) ^ (hash[0] & hash[2]) ^ (hash[1] & hash[2]);
        const temp1 = (hash[7] + (rightRotate(hash[4], 6) ^ rightRotate(hash[4], 11) ^ rightRotate(hash[4], 25)) + ch + k[j] + w_j) | 0;
        const temp2 = ((rightRotate(hash[0], 2) ^ rightRotate(hash[0], 13) ^ rightRotate(hash[0], 22)) + maj) | 0;
        hash[7] = hash[6];
        hash[6] = hash[5];
        hash[5] = hash[4];
        hash[4] = (hash[3] + temp1) | 0;
        hash[3] = hash[2];
        hash[2] = hash[1];
        hash[1] = hash[0];
        hash[0] = (temp1 + temp2) | 0;
      }
      for (let j = 0; j < 8; j++) hash[j] = (hash[j] + oldHash[j]) | 0;
    }
    const bytes = [];
    for (let i = 0; i < 8; i++) {
      for (let j = 3; j >= 0; j--) {
        bytes.push((hash[i] >> (8 * j)) & 255);
      }
    }
    const b64 = btoa(String.fromCharCode(...bytes));
    return b64.replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
  }

  function parseJwtPayload(token) {
    try {
      const parts = token.split('.');
      if (parts.length >= 2) {
        const base64 = parts[1].replace(/-/g, '+').replace(/_/g, '/');
        const jsonStr = decodeURIComponent(escape(atob(base64)));
        return JSON.parse(jsonStr);
      }
    } catch(e) {}
    return null;
  }

  async function startOAuthPKCEFlow(provider) {
    const oauthBox = document.getElementById(`${provider}-oauth-box`);
    const oauthStatus = document.getElementById(`${provider}-oauth-status`);
    if (oauthBox) oauthBox.classList.remove('hidden');
    if (oauthStatus) oauthStatus.textContent = '새 탭에서 로그인 및 승인을 대기하고 있습니다...';

    const verifier = generateCodeVerifier(64);
    sessionStorage.setItem(`${provider}_pkce_verifier`, verifier);
    const challenge = await generateCodeChallenge(verifier);

    const state = generateCodeVerifier(32);
    sessionStorage.setItem(`${provider}_pkce_state`, state);

    let authUrl = '';
    if (provider === 'openai') {
      const redirectUri = 'http://localhost:1455/auth/callback';
      const scope = encodeURIComponent('openid profile email offline_access api.connectors.read api.connectors.invoke');
      authUrl = `https://auth.openai.com/oauth/authorize?response_type=code&client_id=app_EMoamEEZ73f0CkXaXp7hrann&redirect_uri=${encodeURIComponent(redirectUri)}&scope=${scope}&code_challenge=${challenge}&code_challenge_method=S256&state=${state}&id_token_add_organizations=true&codex_cli_simplified_flow=true&originator=codex_cli_rs`;
    } else if (provider === 'anthropic') {
      const redirectUri = 'https://platform.claude.com/oauth/code/callback';
      authUrl = `https://claude.ai/oauth/authorize?response_type=code&client_id=9d1c250a-e61b-44d9-88ed-5944d1962f5e&redirect_uri=${encodeURIComponent(redirectUri)}&scope=org%3Acreate_api_key%20user%3Aprofile&code_challenge=${challenge}&code_challenge_method=S256&state=${state}`;
    }

    if (authUrl) {
      const urlB64 = btoa(unescape(encodeURIComponent(authUrl)));
      window.location.href = `http://ui-action/oauth-start?provider=${provider}&url_b64=${encodeURIComponent(urlB64)}`;
    }
  }

  window.onOAuthCallback = async function(provider, code, error, state) {
    const oauthBox = document.getElementById(`${provider}-oauth-box`);
    const oauthStatus = document.getElementById(`${provider}-oauth-status`);

    if (error) {
      if (oauthStatus) oauthStatus.textContent = `인증 오류: ${error}`;
      return;
    }
    if (!code) {
      if (oauthStatus) oauthStatus.textContent = '승인 코드를 수신하지 못했습니다.';
      return;
    }

    const expectedState = sessionStorage.getItem(`${provider}_pkce_state`);
    if (expectedState && state && state !== expectedState) {
      console.warn('OAuth state mismatch:', { expectedState, state });
      if (oauthStatus) oauthStatus.textContent = '보안 경고: 인증 State 불일치(CSRF 위험). 다시 시도해주세요.';
      return;
    }

    if (oauthStatus) oauthStatus.textContent = '인증 토큰 교환 중...';
    const verifier = sessionStorage.getItem(`${provider}_pkce_verifier`) || '';

    try {
      if (provider === 'openai') {
        const tokenParams = new URLSearchParams();
        tokenParams.append('grant_type', 'authorization_code');
        tokenParams.append('client_id', 'app_EMoamEEZ73f0CkXaXp7hrann');
        tokenParams.append('code', code);
        tokenParams.append('code_verifier', verifier);
        tokenParams.append('redirect_uri', 'http://localhost:1455/auth/callback');

        const tokenResp = await fetch('https://auth.openai.com/oauth/token', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
            'User-Agent': 'codex-cli/1.0'
          },
          body: tokenParams.toString()
        });

        const tokenData = await tokenResp.json().catch(() => ({}));
        if (tokenResp.ok && (tokenData.access_token || tokenData.id_token)) {
          if (oauthBox) oauthBox.classList.add('hidden');
          let accessToken = tokenData.access_token;
          const refreshToken = tokenData.refresh_token || '';
          const idToken = tokenData.id_token || '';
          const expAt = Math.floor(Date.now() / 1000) + (tokenData.expires_in || 864000);

          let userEmail = 'ChatGPT Plus';
          if (idToken) {
            const jwtPayload = parseJwtPayload(idToken);
            if (jwtPayload && jwtPayload.email) {
              userEmail = jwtPayload.email;
            }

            // Attempt token-exchange for openai-api-key if account supports it
            try {
              const exchangeParams = new URLSearchParams();
              exchangeParams.append('grant_type', 'urn:ietf:params:oauth:grant-type:token-exchange');
              exchangeParams.append('client_id', 'app_EMoamEEZ73f0CkXaXp7hrann');
              exchangeParams.append('requested_token', 'openai-api-key');
              exchangeParams.append('subject_token', idToken);
              exchangeParams.append('subject_token_type', 'urn:ietf:params:oauth:token-type:id_token');

              const exResp = await fetch('https://auth.openai.com/oauth/token', {
                method: 'POST',
                headers: {
                  'Content-Type': 'application/x-www-form-urlencoded',
                  'User-Agent': 'codex-cli/1.0'
                },
                body: exchangeParams.toString()
              });
              if (exResp.ok) {
                const exData = await exResp.json().catch(() => ({}));
                if (exData.access_token) {
                  accessToken = exData.access_token;
                }
              }
            } catch(e) {
              console.warn('Optional token-exchange skipped:', e);
            }
          }

          window.location.href = `http://ui-action/auth-save-session?provider=openai&email=${encodeURIComponent(userEmail)}&tier=ChatGPT%20Plus&access_token=${encodeURIComponent(accessToken)}&refresh_token=${encodeURIComponent(refreshToken)}&expires_at=${expAt}`;
        } else {
          if (oauthStatus) oauthStatus.textContent = `토큰 발급 실패: ${tokenData.error_description || tokenData.error || '알 수 없는 오류'}`;
        }
      } else if (provider === 'anthropic') {
        const tokenParams = new URLSearchParams();
        tokenParams.append('grant_type', 'authorization_code');
        tokenParams.append('client_id', '9d1c250a-e61b-44d9-88ed-5944d1962f5e');
        tokenParams.append('code', code);
        tokenParams.append('code_verifier', verifier);
        tokenParams.append('redirect_uri', 'https://platform.claude.com/oauth/code/callback');

        const tokenResp = await fetch('https://platform.claude.com/v1/oauth/token', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/x-www-form-urlencoded'
          },
          body: tokenParams.toString()
        });

        const tokenData = await tokenResp.json().catch(() => ({}));
        if (tokenResp.ok && tokenData.access_token) {
          if (oauthBox) oauthBox.classList.add('hidden');
          const accessToken = tokenData.access_token;
          const refreshToken = tokenData.refresh_token || '';
          const expAt = Math.floor(Date.now() / 1000) + (tokenData.expires_in || 864000);

          window.location.href = `http://ui-action/auth-save-session?provider=anthropic&email=Claude%20Pro&tier=Claude%20Pro&access_token=${encodeURIComponent(accessToken)}&refresh_token=${encodeURIComponent(refreshToken)}&expires_at=${expAt}`;
        } else {
          if (oauthStatus) oauthStatus.textContent = `토큰 교환 안내: 공식 API Key 또는 claude setup-token 등록을 권장합니다. (${tokenData.error_description || tokenData.error || ''})`;
        }
      }
    } catch (err) {
      if (oauthStatus) oauthStatus.textContent = `오류: ${err.message}`;
    }
  };

  window.renderAuthStatus = function(statusList) {
    if (!Array.isArray(statusList)) return;
    statusList.forEach(item => {
      const prov = item.provider;
      const statusDiv = document.getElementById(`${prov}-sub-status`);
      const loginBtn = document.querySelector(`.auth-login-btn[data-provider="${prov}"]`);
      const logoutBtn = document.querySelector(`.auth-logout-btn[data-provider="${prov}"]`);
      const oauthBox = document.getElementById(`${prov}-oauth-box`);

      if (statusDiv) {
        if (item.connected) {
          if (oauthBox) oauthBox.classList.add('hidden');
          statusDiv.innerHTML = `
            <span class="auth-badge auth-badge-connected"><svg class="icon-inline" viewBox="0 0 24 24"><polyline points="20 6 9 17 4 12"/></svg> <span>${escapeHtml(item.tier || '연결됨')}</span></span>
            <p class="auth-hint"><strong>${escapeHtml(item.email || '계정 연결 완료')}</strong></p>
          `;
          if (loginBtn) loginBtn.classList.add('hidden');
          if (logoutBtn) logoutBtn.classList.remove('hidden');
        } else {
          statusDiv.innerHTML = `
            <span class="auth-badge auth-badge-unconnected"><svg class="icon-inline" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><line x1="12" x2="12" y1="8" y2="12"/><line x1="12" x2="12.01" y1="16" y2="16"/></svg> <span>${prov === 'gemini' ? '무료 키 미등록' : '구독 미연결'}</span></span>
            <p class="auth-hint">${prov === 'gemini' ? 'Google AI Studio 무료 키를 등록하여 1,500회/일 무료로 사용하세요.' : '구독 계정을 1-클릭 대화형 승인하여 API 키 없이 사용하세요.'}</p>
          `;
          if (loginBtn) loginBtn.classList.remove('hidden');
          if (logoutBtn) logoutBtn.classList.add('hidden');
        }
      }
    });
  };

  window.onAuthUpdated = function() {
    requestAuthStatus();
  };

  window.addEventListener('focus', () => {
    requestAuthStatus();
  });

  document.querySelectorAll('.auth-login-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const prov = btn.getAttribute('data-provider');
      if (prov === 'gemini') {
        window.location.href = 'http://ui-action/new-tab?url=' + encodeURIComponent('https://aistudio.google.com/app/apikey');
      } else if (prov === 'openai' || prov === 'anthropic') {
        startOAuthPKCEFlow(prov);
      } else {
        window.location.href = `http://ui-action/auth-login?provider=${prov}`;
      }
    });
  });

  document.querySelectorAll('.auth-logout-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const prov = btn.getAttribute('data-provider');
      const oauthBox = document.getElementById(`${prov}-oauth-box`);
      if (oauthBox) oauthBox.classList.add('hidden');
      window.location.href = `http://ui-action/auth-delete-session?provider=${prov}`;
    });
  });

  document.querySelectorAll('.auth-save-token-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const prov = btn.getAttribute('data-provider');
      const tokenInput = document.getElementById(`${prov}-sub-token`);
      const token = tokenInput ? tokenInput.value.trim() : '';
      if (!token) {
        alert('키 또는 토큰을 입력해주세요.');
        return;
      }
      let tier = 'Subscription';
      if (prov === 'gemini') tier = 'Google AI Studio (무료)';
      else if (prov === 'anthropic') tier = 'Claude Pro';
      else if (prov === 'openai') tier = 'ChatGPT Plus';

      window.location.href = `http://ui-action/auth-save-session?provider=${prov}&email=${encodeURIComponent(tier)}&tier=${encodeURIComponent(tier)}&access_token=${encodeURIComponent(token)}`;
      tokenInput.value = '';
    });
  });

  // 1. Initialize State & Settings
  function loadSettingsIntoUI() {
    const s = AIProviderFactory.getSettings();
    providerSelect.value = s.provider || 'gemini';

    ['openai', 'anthropic'].forEach(prov => {
      const mode = s[`${prov}AuthMode`] || 'subscription';
      setAuthModeUI(prov, mode);
    });

    geminiKeyInput.value = s.geminiKey || '';
    if (!s.geminiKey) {
      getSubscriptionToken('gemini').then(token => {
        if (token && token.startsWith('AIza')) {
          geminiKeyInput.value = token;
          s.geminiKey = token;
          AIProviderFactory.saveSettings(s);
        }
      });
    }
    geminiModelSelect.value = s.geminiModel || 'gemini-3.8-flash';
    if (!geminiModelSelect.value) geminiModelSelect.value = 'gemini-3.8-flash';
    openaiKeyInput.value = s.openaiKey || '';
    openaiModelSelect.value = s.openaiModel || 'gpt-5.6-terra';
    if (!openaiModelSelect.value) openaiModelSelect.value = 'gpt-5.6-terra';
    anthropicKeyInput.value = s.anthropicKey || '';
    anthropicModelSelect.value = s.anthropicModel || 'claude-opus-4-7';
    if (!anthropicModelSelect.value) anthropicModelSelect.value = 'claude-opus-4-7';
    ollamaUrlInput.value = s.ollamaUrl || 'http://localhost:11434';
    ollamaModelInput.value = s.ollamaModel || 'llama3.2';

    updateProviderFields(s.provider || 'gemini');
    updateMemoryStats();
    requestVaultList();
    requestAuthStatus();
  }

  function updateProviderFields(provider) {
    geminiFields.classList.toggle('hidden', provider !== 'gemini');
    openaiFields.classList.toggle('hidden', provider !== 'openai');
    anthropicFields.classList.toggle('hidden', provider !== 'anthropic');
    ollamaFields.classList.toggle('hidden', provider !== 'ollama');

    if (['openai', 'anthropic'].includes(provider)) {
      const s = AIProviderFactory.getSettings();
      const mode = s[`${provider}AuthMode`] || 'subscription';
      setAuthModeUI(provider, mode);
    }
  }

  providerSelect.addEventListener('change', (e) => {
    const selectedProvider = e.target.value;
    updateProviderFields(selectedProvider);
    if (['openai', 'anthropic'].includes(selectedProvider)) {
      setAuthModeUI(selectedProvider, 'subscription');
    }
  });

  saveSettingsBtn.addEventListener('click', () => {
    const activeOpenAIMode = document.querySelector('.auth-toggle-btn[data-provider="openai"].active')?.getAttribute('data-mode') || 'subscription';
    const activeAnthropicMode = document.querySelector('.auth-toggle-btn[data-provider="anthropic"].active')?.getAttribute('data-mode') || 'subscription';

    const updated = {
      provider: providerSelect.value,
      geminiAuthMode: 'apikey',
      openaiAuthMode: activeOpenAIMode,
      anthropicAuthMode: activeAnthropicMode,
      geminiKey: geminiKeyInput.value.trim(),
      geminiModel: geminiModelSelect.value,
      openaiKey: openaiKeyInput.value.trim(),
      openaiModel: openaiModelSelect.value,
      anthropicKey: anthropicKeyInput.value.trim(),
      anthropicModel: anthropicModelSelect.value,
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
        <button class="icon-btn icon-btn-danger" title="삭제" data-domain="${escapeHtml(e.domain)}">
          <svg viewBox="0 0 24 24"><path d="M3 6h18"/><path d="M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6"/><path d="M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2"/><line x1="10" x2="10" y1="11" y2="17"/><line x1="14" x2="14" y1="11" y2="17"/></svg>
        </button>
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
    thinkingBox.innerHTML = '<summary><svg class="icon-inline" viewBox="0 0 24 24"><path d="M12 5a3 3 0 1 0-5.997.125 4 4 0 0 0-2.526 5.77 4 4 0 0 0 .556 6.588A4 4 0 1 0 12 18Z"/><path d="M12 5a3 3 0 1 1 5.997.125 4 4 0 0 1 2.526 5.77 4 4 0 0 1-.556 6.588A4 4 0 1 1 12 18Z"/><path d="M15 13a4.5 4.5 0 0 1-3-4 4.5 4.5 0 0 1-3 4"/><path d="M12 18v4"/></svg> <span>사고 과정 (Thinking)...</span></summary><div class="thinking-content"></div>';

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

  // Aside Style Friendly Tool Name Mapping
  const TOOL_NAME_KO = {
    browser_navigate: '🌐 웹페이지 이동',
    browser_get_page_content: '📄 페이지 실시간 분석',
    browser_click_element: '🖱️ 요소 클릭',
    browser_type_text: '⌨️ 텍스트 입력',
    browser_scroll: '📜 화면 스크롤',
    browser_autofill_login: '🔐 볼트 자동 로그인'
  };

  function appendToolCard(toolName, args, result) {
    const isError = Boolean(result?.isError || result?.status === 'error');
    const friendlyName = TOOL_NAME_KO[toolName] || toolName;
    const card = document.createElement('div');
    card.className = 'timeline-step';

    let argSummary = '';
    if (toolName === 'browser_navigate') argSummary = `URL: ${args.url || ''}`;
    else if (toolName === 'browser_click_element') argSummary = `클릭: ${args.text || args.selector || ''}`;
    else if (toolName === 'browser_type_text') argSummary = `입력: "${args.text || ''}" (${args.selector || ''})`;
    else if (toolName === 'browser_get_page_content') argSummary = `본문 추출 (형식: ${args.format || 'summary'})`;
    else argSummary = JSON.stringify(args);

    let resultSummary = '';
    if (toolName === 'browser_get_page_content' && result?.title) {
      resultSummary = `[${result.title}] - ${result.url}\n${(result.bodySnippet || result.markdown || '').slice(0, 400)}...`;
    } else {
      resultSummary = typeof result === 'string' ? result : JSON.stringify(result, null, 2);
    }

    card.innerHTML = `
      <div class="timeline-step-header">
        <div class="step-info">
          <span class="step-badge ${isError ? 'status' : 'tool'}" style="${isError ? 'background: rgba(239, 68, 68, 0.15); color: #ef4444;' : ''}">
            ${isError ? '오류' : '도구 실행'}
          </span>
          <span class="step-name">${escapeHtml(friendlyName)}</span>
        </div>
        <span class="step-status-icon">${isError ? '❌ 실패' : '✓ 완료'}</span>
      </div>
      <div class="timeline-step-body">
        <div style="font-weight: 500; margin-bottom: 4px; color: var(--text-primary);">${escapeHtml(argSummary)}</div>
        <details style="margin-top: 4px;">
          <summary style="font-size: 10.5px; color: var(--text-muted); cursor: pointer;">실행 결과 상세 보기</summary>
          <pre>${escapeHtml(resultSummary)}</pre>
        </details>
      </div>
    `;

    const header = card.querySelector('.timeline-step-header');
    if (header) {
      header.addEventListener('click', () => {
        const body = card.querySelector('.timeline-step-body');
        if (body) body.classList.toggle('hidden');
      });
    }

    messagesList.appendChild(card);
    chatContainer.scrollTop = chatContainer.scrollHeight;
  }


  function getSubscriptionToken(provider) {
    return new Promise((resolve) => {
      const handler = (prov, token, ok) => {
        if (prov === provider) {
          window.onAuthTokenReceived = null;
          resolve(ok ? token : null);
        }
      };
      window.onAuthTokenReceived = handler;
      window.location.href = `http://ui-action/auth-get-token?provider=${provider}`;
      setTimeout(() => {
        if (window.onAuthTokenReceived === handler) {
          window.onAuthTokenReceived = null;
          resolve(null);
        }
      }, 1000);
    });
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
      const currentAuthMode = settings[`${settings.provider}AuthMode`] || 'apikey';
      settings.authType = currentAuthMode;
      if (currentAuthMode === 'subscription') {
        const subToken = await getSubscriptionToken(settings.provider);
        if (!subToken) {
          throw new Error(`${settings.provider.toUpperCase()} 구독 계정이 연결되지 않았거나 세션이 만료되었습니다. 설정에서 구독 계정으로 로그인해주세요.`);
        }
        settings.subscriptionToken = subToken;
      }

      const provider = AIProviderFactory.createProvider(settings);
      const baseDirective = `[AI 브라우저 에이전트 핵심 행동 지침]\n` +
        `1. 당신은 웹 브라우징을 능동적으로 돕는 실시간 AI 브라우저 에이전트입니다.\n` +
        `2. [사용자 지시 준수 및 임의 조작 금지 원칙]: 사용자가 URL 질문, 본문 요약, 내용 설명 등 '조회/질문'을 했을 때는 오직 'browser_get_page_content'로 내용만 확인하고 즉시 답변해야 합니다. 사용자가 명시적으로 검색이나 클릭을 지시하지 않았다면, 화면에 검색창이나 버튼이 보이더라도 절대로 'browser_type_text'나 'browser_click_element'를 임의로 실행하지 마십시오.\n` +
        `3. [실시간 갱신 규칙]: 웹 브라우저는 사용자가 대화 도중에도 수시로 다른 페이지로 이동할 수 있는 동적 환경입니다. 사용자가 새로 질문(새 턴)할 때마다 과거 대화 기록에 의존하지 말고 'browser_get_page_content' 도구를 호출하여 최신 실시간 브라우저 상태를 확인하세요.\n` +
        `4. [도구 완료 후 답변 원칙]: 이번 턴에서 도구를 실행하여 결과를 받았다면 불필요한 추가 조작을 하지 말고, 수신된 최신 URL 및 본문 데이터를 바탕으로 즉시 사용자에게 최종 텍스트 답변을 작성하여 완료하십시오.\n` +
        `5. 도구 실행 결과를 받으면 그 안의 실제 최신 URL과 본문 내용을 바탕으로 정확하게 답변하세요.\n\n`;
      const systemPrompt = baseDirective + (settings.systemPrompt || '') + memoryPrompt;
      const tools = window.taskRuntime.getAvailableTools();

      let loopCount = 0;
      const maxLoops = 6;
      const executedToolsInTurn = new Set();

      while (loopCount < maxLoops) {
        loopCount++;
        const bubble = createStreamingBubble();
        let toolCalls = [];

        // If page content was already retrieved in this turn without subsequent navigation,
        // filter it out from tools so the model is prompted to produce the final text answer.
        let activeTools = tools;
        if (executedToolsInTurn.has('browser_get_page_content') && !executedToolsInTurn.has('browser_navigate')) {
          activeTools = tools.filter(t => t.name !== 'browser_get_page_content');
        }

        await provider.chatStream({
          messages: conversationHistory,
          tools: activeTools && activeTools.length > 0 ? activeTools : undefined,
          systemPrompt,
          signal: abortController.signal,
          onStatus: (statusInfo) => {
            if (statusInfo.type === 'rate_limit_retry') {
              bubble.statusNotice.classList.remove('hidden');
              bubble.statusNotice.innerHTML = `<svg class="icon-inline spin" viewBox="0 0 24 24"><path d="M21 12a9 9 0 1 1-6.219-8.56"/></svg> <span>${escapeHtml(statusInfo.message)}</span>`;
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
          },
          onComplete: ({ fullText }) => {
            if (fullText && !bubble.contentDiv.innerText.trim()) {
              bubble.statusNotice.classList.add('hidden');
              bubble.contentDiv.innerHTML = formatMarkdown(fullText);
              chatContainer.scrollTop = chatContainer.scrollHeight;
            }
          }
        });

        const fullAssistantText = (bubble.contentDiv.innerText || '').trim();
        const hasThinking = !bubble.thinkingBox.classList.contains('hidden');

        // If turn only produced tool calls and no text or thinking, remove empty placeholder
        if (!fullAssistantText && !hasThinking && toolCalls.length > 0) {
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
          executedToolsInTurn.add(tc.name);
          if (tc.name === 'browser_navigate') {
            executedToolsInTurn.delete('browser_get_page_content');
          }
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
        appendAssistantMessage(`오류가 발생했습니다: ${err.message}`);
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
