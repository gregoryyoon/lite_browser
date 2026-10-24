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

  function fetchViaWinHttpProxy(url, headers, body) {
    return new Promise((resolve, reject) => {
      let rawResponse = '';
      if (typeof runAIProxyStream !== 'function') {
        return reject(new Error('runAIProxyStream 함수를 찾을 수 없습니다.'));
      }
      runAIProxyStream({
        url,
        headers,
        body,
        onRawLine: (line) => {
          rawResponse += line + '\n';
        },
        onComplete: () => {
          try {
            const trimmed = rawResponse.trim();
            const data = JSON.parse(trimmed);
            resolve(data);
          } catch (e) {
            const match = rawResponse.match(/\{[\s\S]*\}/);
            if (match) {
              try {
                return resolve(JSON.parse(match[0]));
              } catch (e2) {}
            }
            reject(new Error('응답 JSON 파싱 실패: ' + rawResponse.slice(0, 200)));
          }
        },
        onError: (err) => {
          reject(err);
        }
      });
    });
  }

  function logOAuthDebug(provider, step, detail) {
    const box = document.getElementById(`${provider}-oauth-debug`);
    const pre = document.getElementById(`${provider}-oauth-debug-content`);
    const timestamp = new Date().toLocaleTimeString();
    const str = typeof detail === 'object' ? JSON.stringify(detail, null, 2) : String(detail);
    const line = `[${timestamp}] [${step}] ${str}\n`;
    console.log(`[OAuth Debug ${provider}]`, step, detail);
    if (pre) pre.textContent += line;
    if (box) box.classList.remove('hidden');
  }

  async function startOAuthPKCEFlow(provider) {
    const oauthBox = document.getElementById(`${provider}-oauth-box`);
    const oauthStatus = document.getElementById(`${provider}-oauth-status`);
    const debugPre = document.getElementById(`${provider}-oauth-debug-content`);
    if (debugPre) debugPre.textContent = '';
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
      const redirectUri = 'http://localhost:54545/callback';
      sessionStorage.setItem('anthropic_pkce_redirect_uri', redirectUri);
      const scopes = encodeURIComponent('org:create_api_key user:profile user:inference user:sessions:claude_code');
      authUrl = `https://claude.ai/oauth/authorize?response_type=code&client_id=9d1c250a-e61b-44d9-88ed-5944d1962f5e&redirect_uri=${encodeURIComponent(redirectUri)}&scope=${scopes}&code_challenge=${challenge}&code_challenge_method=S256&state=${state}`;
    }

    logOAuthDebug(provider, 'Flow Started', {
      redirect_uri: provider === 'openai' ? 'http://localhost:1455/auth/callback' : 'http://localhost:54545/callback',
      challenge: challenge.slice(0, 10) + '...',
      state: state.slice(0, 8) + '...'
    });

    if (authUrl) {
      const urlB64 = btoa(unescape(encodeURIComponent(authUrl)));
      window.location.href = `http://ui-action/oauth-start?provider=${provider}&url_b64=${encodeURIComponent(urlB64)}`;
    }
  }

  window.onOAuthCallback = async function(provider, code, error, state) {
    const oauthBox = document.getElementById(`${provider}-oauth-box`);
    const oauthStatus = document.getElementById(`${provider}-oauth-status`);

    logOAuthDebug(provider, 'Callback Received', {
      has_code: !!code,
      code_preview: code ? (code.slice(0, 8) + '...') : null,
      error: error || null,
      state: state ? (state.slice(0, 8) + '...') : null
    });

    if (error) {
      logOAuthDebug(provider, 'Callback Error', error);
      if (oauthStatus) oauthStatus.textContent = `인증 오류: ${error}`;
      return;
    }
    if (!code) {
      logOAuthDebug(provider, 'No Code', '승인 코드를 수신하지 못했습니다.');
      if (oauthStatus) oauthStatus.textContent = '승인 코드를 수신하지 못했습니다.';
      return;
    }

    const expectedState = sessionStorage.getItem(`${provider}_pkce_state`);
    if (expectedState && state && state !== expectedState) {
      logOAuthDebug(provider, 'State Mismatch', { expectedState, receivedState: state });
      console.warn('OAuth state mismatch:', { expectedState, state });
      if (oauthStatus) oauthStatus.textContent = '보안 경고: 인증 State 불일치(CSRF 위험). 다시 시도해주세요.';
      return;
    }

    if (oauthStatus) oauthStatus.textContent = '인증 토큰 교환 중...';
    const verifier = sessionStorage.getItem(`${provider}_pkce_verifier`) || '';

    try {
      if (provider === 'openai') {
        logOAuthDebug('openai', 'Requesting Token Exchange', { endpoint: 'https://auth.openai.com/oauth/token' });
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
        logOAuthDebug('openai', 'Token Response', { ok: tokenResp.ok, status: tokenResp.status, tokenData });
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

          logOAuthDebug('openai', 'Flow Completed', { status: '연결 성공! 계정 세션이 저장되었습니다.', userEmail, expires_at: new Date(expAt * 1000).toLocaleString() });
          window.location.href = `http://ui-action/auth-save-session?provider=openai&email=${encodeURIComponent(userEmail)}&tier=ChatGPT%20Plus&access_token=${encodeURIComponent(accessToken)}&refresh_token=${encodeURIComponent(refreshToken)}&expires_at=${expAt}`;
        } else {
          if (oauthStatus) oauthStatus.textContent = `토큰 발급 실패: ${tokenData.error_description || tokenData.error || '알 수 없는 오류'}`;
        }
      } else if (provider === 'anthropic') {
        const redirectUri = sessionStorage.getItem('anthropic_pkce_redirect_uri') || 'http://localhost:54545/callback';
        const tokenPayload = {
          grant_type: 'authorization_code',
          client_id: '9d1c250a-e61b-44d9-88ed-5944d1962f5e',
          code: code,
          code_verifier: verifier,
          redirect_uri: redirectUri,
          state: state
        };
        const tokenJsonStr = JSON.stringify(tokenPayload);
        const reqHeaders = {
          'Content-Type': 'application/json',
          'Accept': 'application/json, text/plain, */*',
          'User-Agent': 'Anthropic/JS 0.36.0 Node/v22.12.0 win32 (x64)'
        };

        logOAuthDebug('anthropic', 'Requesting Token Exchange', {
          endpoint: 'https://console.anthropic.com/v1/oauth/token',
          payload: {
            grant_type: tokenPayload.grant_type,
            client_id: tokenPayload.client_id,
            redirect_uri: tokenPayload.redirect_uri,
            code_preview: code.slice(0, 8) + '...',
            state: state ? (state.slice(0, 8) + '...') : null
          }
        });

        let tokenData = null;
        try {
          tokenData = await fetchViaWinHttpProxy(
            'https://console.anthropic.com/v1/oauth/token',
            reqHeaders,
            tokenJsonStr
          );
          logOAuthDebug('anthropic', 'Token Response (console.anthropic.com)', tokenData);
        } catch (proxyErr) {
          logOAuthDebug('anthropic', 'console.anthropic.com Error', proxyErr.message || String(proxyErr));
          console.warn('WinHTTP token exchange on console.anthropic.com failed, trying platform.claude.com fallback...', proxyErr);
          try {
            tokenData = await fetchViaWinHttpProxy(
              'https://platform.claude.com/v1/oauth/token',
              reqHeaders,
              tokenJsonStr
            );
            logOAuthDebug('anthropic', 'Token Response (platform.claude.com)', tokenData);
          } catch (fallbackErr) {
            logOAuthDebug('anthropic', 'platform.claude.com Error', fallbackErr.message || String(fallbackErr));
            let readableMsg = proxyErr.message || fallbackErr.message || '토큰 교환 실패';
            try {
              const match = readableMsg.match(/\{[\s\S]*\}/);
              if (match) {
                const parsed = JSON.parse(match[0]);
                if (parsed.error?.message) {
                  readableMsg = parsed.error.message;
                } else if (parsed.message) {
                  readableMsg = parsed.message;
                }
              }
            } catch (e) {}
            throw new Error(readableMsg);
          }
        }

        if (tokenData && (tokenData.access_token || tokenData.id_token)) {
          if (oauthBox) oauthBox.classList.add('hidden');
          let accessToken = tokenData.access_token || tokenData.id_token;
          const refreshToken = tokenData.refresh_token || '';
          const expAt = Math.floor(Date.now() / 1000) + (tokenData.expires_in || 864000);

          let userEmail = 'Claude Pro';
          if (tokenData.email) userEmail = tokenData.email;
          else if (tokenData.id_token) {
            const jwt = parseJwtPayload(tokenData.id_token);
            if (jwt?.email) userEmail = jwt.email;
          }

          // Attempt to generate standard Claude API Key via official CLI endpoint
          try {
            logOAuthDebug('anthropic', 'Requesting API Key via CLI endpoint', { endpoint: 'https://api.anthropic.com/api/oauth/claude_cli/create_api_key' });
            const keyData = await fetchViaWinHttpProxy(
              'https://api.anthropic.com/api/oauth/claude_cli/create_api_key',
              {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${accessToken}`,
                'User-Agent': 'Anthropic/JS 0.36.0 Node/v22.12.0 win32 (x64)'
              },
              '{}'
            );
            if (keyData && keyData.raw_key) {
              logOAuthDebug('anthropic', 'Generated Claude API Key successfully', { key_prefix: keyData.raw_key.slice(0, 10) + '...' });
              accessToken = keyData.raw_key;
            }
          } catch (keyErr) {
            console.warn('create_api_key step skipped, using access_token directly:', keyErr);
          }

          logOAuthDebug('anthropic', 'Flow Completed', {
            status: '연결 성공! 계정 세션이 저장되었습니다.',
            userEmail,
            token_type: accessToken.startsWith('sk-ant-') ? 'Claude API Key' : 'OAuth Bearer Token',
            token_preview: accessToken.slice(0, 10) + '...',
            expires_at: new Date(expAt * 1000).toLocaleString()
          });
          window.location.href = `http://ui-action/auth-save-session?provider=anthropic&email=${encodeURIComponent(userEmail)}&tier=Claude%20Pro&access_token=${encodeURIComponent(accessToken)}&refresh_token=${encodeURIComponent(refreshToken)}&expires_at=${expAt}`;
        } else {
          const errDetail = tokenData?.error_description || tokenData?.error?.message || tokenData?.error || '알 수 없는 오류';
          logOAuthDebug('anthropic', 'Token Issue Failed', errDetail);
          if (oauthStatus) oauthStatus.textContent = `토큰 발급 실패: ${errDetail}`;
        }
      }
    } catch (err) {
      logOAuthDebug(provider, 'Catch Error', err.message || String(err));
      let displayMsg = err.message || '알 수 없는 오류';
      try {
        const match = displayMsg.match(/\{[\s\S]*\}/);
        if (match) {
          const parsed = JSON.parse(match[0]);
          if (parsed.error?.message) {
            displayMsg = parsed.error.message;
          } else if (parsed.message) {
            displayMsg = parsed.message;
          }
        }
      } catch (e) {}
      if (oauthStatus) oauthStatus.textContent = `오류: ${displayMsg}`;
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
            <span class="auth-badge auth-badge-unconnected"><svg class="icon-inline" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><line x1="12" x2="12" y1="8" y2="12"/><line x1="12" x2="12.01" y1="16" y2="16"/></svg> <span>${prov === 'gemini' ? '무료 키 미등록' : (prov === 'anthropic' ? '재연결 필요 (권한 갱신)' : '구독 미연결')}</span></span>
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

  document.querySelectorAll('.oauth-copy-btn').forEach(btn => {
    btn.addEventListener('click', async (e) => {
      e.preventDefault();
      e.stopPropagation();
      const targetId = btn.getAttribute('data-target');
      const targetEl = document.getElementById(targetId);
      const textToCopy = targetEl ? targetEl.textContent : '';
      if (!textToCopy) return;

      try {
        if (navigator.clipboard && navigator.clipboard.writeText) {
          await navigator.clipboard.writeText(textToCopy);
        } else {
          const ta = document.createElement('textarea');
          ta.value = textToCopy;
          ta.style.position = 'fixed';
          ta.style.opacity = '0';
          document.body.appendChild(ta);
          ta.select();
          document.execCommand('copy');
          document.body.removeChild(ta);
        }
        const span = btn.querySelector('span');
        const origText = span ? span.textContent : '복사';
        if (span) span.textContent = '복사됨!';
        btn.classList.add('btn-primary');
        btn.classList.remove('btn-secondary');
        setTimeout(() => {
          if (span) span.textContent = origText;
          btn.classList.remove('btn-primary');
          btn.classList.add('btn-secondary');
        }, 1500);
      } catch (err) {
        console.error('클립보드 복사 실패:', err);
      }
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

    if (localStorage.getItem('anthropic_scope_v3') !== 'true') {
      localStorage.setItem('anthropic_scope_v3', 'true');
      window.location.href = 'http://ui-action/auth-logout?provider=anthropic';
    }

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

  // Aside Style Friendly Tool Name Mapping (No emojis, per AGENTS.md)
  const TOOL_NAME_KO = {
    browser_navigate: '웹페이지 이동',
    browser_get_page_content: '페이지 실시간 분석',
    browser_click_element: '요소 클릭',
    browser_type_text: '텍스트 입력',
    browser_scroll: '화면 스크롤',
    browser_autofill_login: '볼트 자동 로그인',
    browser_get_tabs: '탭 목록 조회',
    browser_switch_tab: '탭 전환',
    browser_reorder_tabs: '탭 순서 재배치 (그룹핑)',
    browser_get_tab_content: '탭 본문 내용 추출',
    browser_split_tabs: '분할 화면(Split View) 결합',
    browser_close_tabs: '탭 닫기',
    browser_open_tab: '새 탭 열기'
  };

  function appendToolCard(toolName, args, result) {
    const isError = Boolean(result?.isError || result?.status === 'error');
    const friendlyName = TOOL_NAME_KO[toolName] || toolName;
    const card = document.createElement('div');
    card.className = 'timeline-step';

    let argSummary = '';
    if (toolName === 'browser_navigate') argSummary = `URL: ${args.url || ''}`;
    else if (toolName === 'browser_open_tab') argSummary = `URL: ${args.url || '새 탭(lite://favorites)'}${args.activate === false ? ' [백그라운드]' : ''}`;
    else if (toolName === 'browser_click_element') argSummary = `클릭: ${args.text || args.selector || ''}`;
    else if (toolName === 'browser_type_text') argSummary = `입력: "${args.text || ''}" (${args.selector || ''})`;
    else if (toolName === 'browser_get_page_content') argSummary = `본문 추출 (형식: ${args.format || 'summary'})`;
    else if (toolName === 'browser_get_tabs') argSummary = `열린 탭 전체 목록 조회`;
    else if (toolName === 'browser_switch_tab') argSummary = `탭 #${args.tab_id} 화면 전환`;
    else if (toolName === 'browser_reorder_tabs') argSummary = `탭 재배치 순서: [${(args.tab_ids || []).join(' → ')}]`;
    else if (toolName === 'browser_get_tab_content') argSummary = `탭 본문 추출 (ID: ${args.tab_id})`;
    else if (toolName === 'browser_split_tabs') argSummary = `좌측(Tab ${args.left_tab_id}) | 우측(Tab ${args.right_tab_id}) 분할 화면`;
    else if (toolName === 'browser_close_tabs') argSummary = `탭 닫기 대상: [${(args.tab_ids || []).join(', ')}]`;
    else argSummary = JSON.stringify(args);

    let resultSummary = '';
    if (toolName === 'browser_get_page_content' && result?.title) {
      resultSummary = `[${result.title}] - ${result.url}\n${(result.bodySnippet || result.markdown || '').slice(0, 400)}...`;
    } else if (toolName === 'browser_get_tab_content' && result?.title) {
      resultSummary = `[Tab ${result.tab_id}: ${result.title}] - ${result.url}\n${(result.bodySnippet || '').slice(0, 400)}...`;
    } else if (toolName === 'browser_get_tabs' && result?.tabs) {
      resultSummary = `총 ${result.tabs.length}개 탭:\n` + result.tabs.map(t => `- [${t.tab_id}] ${t.title || '새 탭'} (${t.url})${t.is_active ? ' [활성]' : ''}${t.is_split ? ' [분할]' : ''}`).join('\n');
    } else if (toolName === 'browser_switch_tab' && result?.status === 'success') {
      resultSummary = `탭 #${args.tab_id} 화면으로 즉시 전환했습니다.`;
    } else if (toolName === 'browser_open_tab' && result?.status === 'success') {
      resultSummary = `새 탭(ID: ${result.tab_id}) 열기 완료: ${result.title || '새 탭'} (${result.url})`;
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
        <span class="step-status-icon">${isError ? '실패' : '완료'}</span>
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
    if (window.sendNativeLog) window.sendNativeLog(`[Sidepanel] User Prompt: "${prompt}"`);

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
        `5. 도구 실행 결과를 받으면 그 안의 실제 최신 URL과 본문 내용을 바탕으로 정확하게 답변하세요.\n` +
        `6. [탭 제어 핵심 기능 규칙]:\n` +
        `   - 새 탭 열기: 사용자가 '새 탭에 ~ 열어줘', '새로운 탭에서 검색해줘' 등을 요청하면 'browser_open_tab(url, activate)'을 호출하세요. 특정 URL이 없으면 url을 비워두거나 'lite://favorites'로 지정하고, 검색어나 키워드인 경우 구글 검색 URL('https://www.google.com/search?q=...')이나 해당 공식 사이트 주소를 지정하세요. 사용자가 '백그라운드로 열어줘'라고 명시한 경우에만 activate: false로 호출하고 그 외에는 activate: true(기본값)로 호출하세요. 생성 완료 후에는 결과 안내 메시지와 함께 [새 탭으로 이동](lite-tab://switch/새탭ID) 바로가기 링크 카드를 제공하세요.\n` +
        `   - 탭 찾기: 사용자가 특정 탭을 찾아달라고 하거나 이동을 요청하면 먼저 'browser_get_tabs'로 탭 목록을 조회하세요. 검색어로 1개 탭이 특정되면 'browser_switch_tab(tab_id)'으로 즉시 전환하고 어떤 탭으로 전환했는지 안내하세요. 일치하는 후보가 여러 개이면 전환하지 말고 후보 탭들의 목록을 [탭제목](lite-tab://switch/탭ID) 링크 형식으로 안내하세요.\n` +
        `   - 탭 그룹핑: 사용자가 탭 정리/그룹핑을 요청하면 'browser_get_tabs'로 전체 탭을 확인한 뒤, 성격/도메인/주제별로 분류하고 'browser_reorder_tabs(tab_ids)'를 호출하여 연관 탭들이 나란히 인접하도록 탭 순서를 정렬하세요. 이후 그룹 목록과 각 탭의 링크를 깔끔하게 브리핑하세요.\n` +
        `   - 탭 간 비교: 사용자가 탭 간 비교/대조를 요청하면 'browser_get_tabs'로 대상 탭을 파악하고, 각 대상 탭에 대해 'browser_get_tab_content(tab_id)'로 본문 내용을 추출하세요. 비교 대상이 2개 탭인 경우 'browser_split_tabs(left_tab_id, right_tab_id)'를 호출하여 분할 화면으로 띄워 시각적 대조를 지원하고, 사이드패널 답변에 스펙, 가격, 특징, 장단점을 명확하게 정리한 마크다운 비교표(Markdown Table)를 작성하세요.\n` +
        `   - 탭 닫기: 사용자가 탭을 닫아달라고 하면 'browser_get_tabs'로 대상 탭 ID를 확인한 뒤 'browser_close_tabs(tab_ids)'를 호출하세요. 사용자가 "확인 없이 닫아줘"라고 명시한 경우에만 require_confirmation: false로 호출하고, 그 외에는 require_confirmation: true(기본값)로 안전 확인 카드를 거치도록 하세요.\n\n`;
      const systemPrompt = baseDirective + (settings.systemPrompt || '') + memoryPrompt;
      const tools = window.taskRuntime.getAvailableTools();

      let loopCount = 0;
      const maxLoops = 8;
      const executedToolsInTurn = new Set();

      while (loopCount < maxLoops) {
        loopCount++;
        const bubble = createStreamingBubble();
        let toolCalls = [];

        // Filter out single-shot tools once executed in this turn to prompt final text answer
        let activeTools = tools;
        if (executedToolsInTurn.has('browser_get_page_content') && !executedToolsInTurn.has('browser_navigate')) {
          activeTools = activeTools.filter(t => t.name !== 'browser_get_page_content');
        }
        if (executedToolsInTurn.has('browser_reorder_tabs')) {
          activeTools = activeTools.filter(t => t.name !== 'browser_reorder_tabs');
        }
        if (executedToolsInTurn.has('browser_split_tabs')) {
          activeTools = activeTools.filter(t => t.name !== 'browser_split_tabs');
        }
        if (executedToolsInTurn.has('browser_close_tabs')) {
          activeTools = activeTools.filter(t => t.name !== 'browser_close_tabs');
        }
        if (executedToolsInTurn.has('browser_open_tab')) {
          activeTools = activeTools.filter(t => t.name !== 'browser_open_tab');
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
            if (window.sendNativeLog) window.sendNativeLog(`[Sidepanel] LLM onToolCall: ${tc.name} ${JSON.stringify(tc.args)}`);
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
          if (tc.name === 'browser_navigate' || tc.name === 'browser_open_tab') {
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

  // Global Tab Switch & Link Click Listener
  messagesList.addEventListener('click', (e) => {
    const tabEl = e.target.closest('[data-tab-id]');
    if (tabEl) {
      const tabId = tabEl.getAttribute('data-tab-id');
      if (tabId) {
        if (window.sendUiAction) {
          window.sendUiAction(`http://ui-action/switch-tab?id=${encodeURIComponent(tabId)}`);
        } else {
          window.location.href = `http://ui-action/switch-tab?id=${encodeURIComponent(tabId)}`;
        }
      }
    }
  });

  // Interactive Tab Close Confirmation Card
  window.confirmCloseTabsUI = function(tabIds, cachedTabs) {
    return new Promise((resolve) => {
      let tabsInfo = [];
      if (cachedTabs && Array.isArray(cachedTabs)) {
        tabsInfo = tabIds.map(id => cachedTabs.find(t => t.tab_id === id) || { tab_id: id, title: `탭 #${id}`, url: '' });
      } else {
        tabsInfo = tabIds.map(id => ({ tab_id: id, title: `탭 #${id}`, url: '' }));
      }

      const card = document.createElement('div');
      card.className = 'interactive-confirm-card';
      card.innerHTML = `
        <div class="confirm-header">
          <svg viewBox="0 0 24 24"><path d="m21.73 18-8-14a2 2 0 0 0-3.48 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3Z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>
          <span>탭 닫기 확인 (${tabIds.length}개)</span>
        </div>
        <p class="confirm-message">아래 탭들을 닫으시겠습니까? 저장되지 않은 작업 내용이 손실될 수 있습니다.</p>
        <ul class="confirm-tab-list">
          ${tabsInfo.map(t => `
            <li class="confirm-tab-item">
              <div class="confirm-tab-info">
                <span class="confirm-tab-title" title="${escapeHtml(t.title || '')}">${escapeHtml(t.title || '제목 없는 탭')}</span>
                ${t.url ? `<span class="confirm-tab-url" title="${escapeHtml(t.url)}">${escapeHtml(t.url)}</span>` : ''}
              </div>
              <span class="confirm-tab-id">#${t.tab_id}</span>
            </li>
          `).join('')}
        </ul>
        <div class="confirm-actions">
          <button class="confirm-btn danger" id="btn-confirm-close">닫기 승인</button>
          <button class="confirm-btn cancel" id="btn-cancel-close">취소</button>
        </div>
        <div class="confirm-status-resolved hidden" id="confirm-status-label"></div>
      `;

      messagesList.appendChild(card);
      chatContainer.scrollTop = chatContainer.scrollHeight;

      const btnConfirm = card.querySelector('#btn-confirm-close');
      const btnCancel = card.querySelector('#btn-cancel-close');
      const actionsDiv = card.querySelector('.confirm-actions');
      const statusLabel = card.querySelector('#confirm-status-label');

      btnConfirm.addEventListener('click', () => {
        actionsDiv.classList.add('hidden');
        statusLabel.classList.remove('hidden');
        statusLabel.className = 'confirm-status-resolved success';
        statusLabel.textContent = `✓ ${tabIds.length}개 탭 닫기가 승인되었습니다.`;
        resolve(true);
      });

      btnCancel.addEventListener('click', () => {
        actionsDiv.classList.add('hidden');
        statusLabel.classList.remove('hidden');
        statusLabel.className = 'confirm-status-resolved cancelled';
        statusLabel.textContent = '✕ 탭 닫기가 취소되었습니다.';
        resolve(false);
      });
    });
  };

  // Helpers
  function escapeHtml(str) {
    if (!str) return '';
    return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }

  function parseMarkdownTables(md) {
    const lines = md.split('\n');
    const out = [];
    let i = 0;
    while (i < lines.length) {
      const line = lines[i];
      if (line.trim().startsWith('|') && line.trim().endsWith('|')) {
        const tableLines = [];
        while (i < lines.length && lines[i].trim().startsWith('|') && lines[i].trim().endsWith('|')) {
          tableLines.push(lines[i].trim());
          i++;
        }
        if (tableLines.length >= 2) {
          const parseRow = (rowStr) => rowStr.slice(1, -1).split('|').map(c => c.trim());
          const headerCells = parseRow(tableLines[0]);
          let isTable = false;
          let bodyStartIndex = 1;
          if (tableLines.length >= 2 && /^\|(\s*:?-+:?\s*\|)+$/.test(tableLines[1])) {
            isTable = true;
            bodyStartIndex = 2;
          }
          if (isTable) {
            let html = '<div class="markdown-table-wrapper"><table class="markdown-table"><thead><tr>';
            for (const h of headerCells) {
              html += `<th>${escapeHtml(h)}</th>`;
            }
            html += '</tr></thead><tbody>';
            for (let b = bodyStartIndex; b < tableLines.length; b++) {
              const cells = parseRow(tableLines[b]);
              html += '<tr>';
              for (let c = 0; c < headerCells.length; c++) {
                html += `<td>${escapeHtml(cells[c] || '')}</td>`;
              }
              html += '</tr>';
            }
            html += '</tbody></table></div>';
            out.push(html);
            continue;
          }
        }
        out.push(...tableLines);
      } else {
        out.push(line);
        i++;
      }
    }
    return out.join('\n');
  }

  function formatMarkdown(text) {
    if (!text) return '';

    // 1. Extract code blocks
    const codeBlocks = [];
    text = text.replace(/```([\s\S]*?)```/g, (match, code) => {
      const idx = codeBlocks.length;
      codeBlocks.push(`<pre><code>${escapeHtml(code.trim())}</code></pre>`);
      return `__CODE_BLOCK_${idx}__`;
    });

    // 2. Extract inline code
    const inlineCodes = [];
    text = text.replace(/`([^`]+)`/g, (match, code) => {
      const idx = inlineCodes.length;
      inlineCodes.push(`<code>${escapeHtml(code)}</code>`);
      return `__INLINE_CODE_${idx}__`;
    });

    // 3. Parse Markdown Tables
    text = parseMarkdownTables(text);

    // 4. Parse Headers
    text = text.replace(/^### (.*$)/gim, '<h4 style="margin: 8px 0 4px; font-size: 13px; font-weight: 600; color: var(--text-primary);">$1</h4>');
    text = text.replace(/^## (.*$)/gim, '<h3 style="margin: 10px 0 6px; font-size: 14px; font-weight: 700; color: var(--text-primary);">$1</h3>');
    text = text.replace(/^# (.*$)/gim, '<h2 style="margin: 12px 0 8px; font-size: 15px; font-weight: 700; color: var(--text-primary);">$1</h2>');

    // 5. Bold & Italic
    text = text.replace(/\*\*([^*\n]+)\*\*/g, '<strong>$1</strong>');
    text = text.replace(/(?<!\*)\*([^*\n\s][^*\n]*[^*\n\s]|\S)\*(?!\*)/g, '<em>$1</em>');

    // 6. Links (including lite-tab://switch/...)
    text = text.replace(/\[([^\]]+)\]\(([^)]+)\)/g, (match, label, href) => {
      if (href.startsWith('lite-tab://switch/')) {
        const tabId = href.replace('lite-tab://switch/', '').trim();
        return `<a href="javascript:void(0)" class="tab-link" data-tab-id="${tabId}"><svg class="icon-inline" viewBox="0 0 24 24" style="width:12px;height:12px;vertical-align:-1px;margin-right:3px;stroke:currentColor;fill:none;"><polyline points="15 3 21 3 21 9"/><polyline points="9 21 3 21 3 15"/><line x1="21" y1="3" x2="14" y2="10"/><line x1="3" y1="21" x2="10" y2="14"/></svg>${escapeHtml(label)}</a>`;
      }
      return `<a href="${escapeHtml(href)}" target="_blank" rel="noopener noreferrer">${escapeHtml(label)}</a>`;
    });

    // 7. Restore code blocks
    codeBlocks.forEach((block, idx) => {
      text = text.replace(`__CODE_BLOCK_${idx}__`, block);
    });
    inlineCodes.forEach((code, idx) => {
      text = text.replace(`__INLINE_CODE_${idx}__`, code);
    });

    // 8. Line breaks (protect table wrappers, headers, and pre blocks from unnecessary <br>)
    const parts = text.split(/(<div class="markdown-table-wrapper">[\s\S]*?<\/div>|<pre><code>[\s\S]*?<\/code><\/pre>)/g);
    text = parts.map(part => {
      if (part.startsWith('<div class="markdown-table-wrapper">') || part.startsWith('<pre><code>')) {
        return part;
      }
      return part.replace(/<\/h[234]>\n?/g, (m) => m.slice(0, 5))
                 .replace(/\n/g, '<br>');
    }).join('');

    return text;
  }

  // Initial UI Setup
  loadSettingsIntoUI();
});
