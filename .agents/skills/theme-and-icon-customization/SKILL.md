---
name: theme-and-icon-customization
description: >-
  Comprehensive runbook and guide for redesigning, migrating, and customizing UI themes
  (Bento Grid, Dark/Light mode tokens) and modernizing icon systems (Lucide SVGs) across
  LiteBrowser's Web UI and Win32 C CAPI native layers. Use this skill whenever the user asks
  to change themes, adjust color palettes, update icons, or fix dark/light mode synchronization.
---

# UI 테마 및 아이콘 변경/마이그레이션 런북 (Theme & Icon Customization Runbook)

이 런북은 LiteBrowser의 웹 UI(`ui/`)와 Win32 C CAPI 네이티브 계층(`tests/cefsimple_capi/`) 전반에 걸쳐 새로운 테마를 적용하거나 아이콘 세트를 교체할 때 사용하는 표준 작업 절차서입니다.

---

## Phase 1: 웹 UI 디자인 토큰 시스템 (`style.css` & `theme.js`)

1. **글로벌 디자인 토큰 정의 (`ui/style.css`)**:
   - `:root`(라이트 모드)와 `[data-theme="dark"]`(다크 모드)에 통일된 CSS 변수를 정의합니다:
     - **배경**: `--bg-app`(전체 배경), `--bg-card`(카드/컨테이너), `--bg-card-subtle`(서브 영역), `--bg-card-hover`(호버)
     - **보더**: `--border-subtle`, `--border-medium`, `--border-focus`
     - **텍스트**: `--text-primary`, `--text-secondary`, `--text-muted`, `--text-dimmed`
     - **강조**: `--accent-primary`, `--accent-focus-ring`, `--accent-mint`
     - **곡률/그림자**: `--bento-radius`, `--shadow-sm`, `--shadow-md`, `--shadow-lg`

2. **실시간 테마 브로드캐스트 (`ui/theme.js`)**:
   - `document.documentElement.setAttribute('data-theme', theme)` 적용.
   - 모든 서브 윈도우, 탭, 사이드패널 간 `BroadcastChannel('lite-browser-theme')` 및 `window.postMessage`로 테마 전환 신호 전파.

3. **서브 페이지 연동 원칙**:
   - 모든 서브 페이지(`settings.html`, `manager.html`, `downloads.html`, `sidepanel.html`)는 `<link rel="stylesheet" href="style.css">`와 `<script src="theme.js"></script>`를 로드.
   - 각 컴포넌트 CSS의 로컬 변수는 반드시 `var(--bg-card)`, `var(--text-primary)` 등으로 매핑하여 일괄 변경 가능하도록 유지.

---

## Phase 2: Lucide Icons 인라인 SVG 아이콘 표준화

1. **외부 의존성 제로 (Zero External Dependencies)**:
   - 보안 및 오프라인 환경을 위해 외부 웹폰트나 CDN을 일절 사용하지 않고, 표준 인라인 SVG를 직접 임베딩합니다.
2. **표준 SVG 속성 규격**:
   ```html
   <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
     <!-- Lucide Path -->
   </svg>
   ```
3. **이모지 전면 교체 원칙**:
   - UI 마크업(HTML) 및 동적 렌더링(JS)에서 유니코드 이모지(`🤖`, `🗑️`, `💳`, `🔑`, `⚠️`, `✅`, `⚙️`)를 제거하고 의미에 맞는 Lucide SVG 아이콘과 뱃지 컴포넌트로 대체합니다.

---

## Phase 3: Win32 네이티브 윈도우 및 CEF 엔진 동기화 (C CAPI)

1. **윈도우 클래스 배경 브러시 분기**:
   - `CreateWindowEx`를 호출하는 모든 윈도우(메인 창, 탭 분리 창, 팝업 창)는 `is_theme_dark()` 상태에 맞춰 배경 브러시(`RGB(13, 15, 21)` vs `RGB(228, 228, 231)`)를 설정하여 화이트 플래시를 차단합니다.
2. **`WM_ERASEBKGND` 동적 채색**:
   - 윈도우 생성 및 리사이즈 시 실시간 테마 브러시로 `FillRect` 수행.
3. **테마 변경 시 클래스 브러시 갱신**:
   - 테마가 변경되면 `SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, ...)`로 즉시 교체.
4. **DWM 다크 타이틀바 연동**:
   - `DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode))`를 호출하여 타이틀바 프레임 색상 동기화.
5. **CEF 자식 브라우저 배경색 일치**:
   - `settings->background_color = is_dark ? 0xFF0D0F15 : 0xFFFFFFFF` 지정.

---

## Phase 4: 리스트 뷰 및 카드 레이아웃 불변식 (Text Alignment Invariants)

1. **북마크/히스토리 등 단일행 리스트 뷰 (`.list-row`)**:
   - **제목 컬럼 고정 너비**: `.list-row-title`에 고정 너비(예: `flex: 0 0 300px; width: 300px`)를 부여하여, 모든 행의 설명/스니펫 텍스트 시작 위치를 균일하게 정렬.
   - **단일행 말줄임표 필수**: 제목, 하이라이트 뱃지, 스니펫 텍스트에 반드시 `white-space: nowrap; overflow: hidden; text-overflow: ellipsis;` 적용.
   - **우측 메타/액션 고정**: 날짜, 방문수, 삭제 버튼은 우측 정렬 및 `flex-shrink: 0` 유지.
2. **다운로드/상세 정보 카드 뷰 (`.dl-card`)**:
   - **메타 행 줄바꿈 방지**: `.dl-meta-row`에 `white-space: nowrap`을 유지하고, 긴 URL(`.dl-url`)에 `max-width` 및 말줄임표 처리.
   - **액션 버튼 우측 정렬**: 액션 버튼 그룹(`.dl-actions`)을 우측에 일관되게 배치.

---

## Phase 5: 빌드 및 검증 체크리스트

1. **디버그 빌드 검증**:
   ```powershell
   cmake --build c:\projects\lite_browser\cef_binary_151.3.24\build --config Debug --target cefsimple_capi
   ```
   - Exit Code 0 확인.
2. **다크/라이트 실시간 토글 테스트**:
   - 설정 페이지에서 테마 전환 시 탭바, 주소창, 사이드패널, 팝업 윈도우가 흰색 번쩍임 없이 실시간 동기화되는지 확인.
3. **텍스트 정렬 검증**:
   - 긴 텍스트 입력 시 레이아웃이 깨지거나 다단 줄바꿈이 발생하지 않고 말줄임표로 단정하게 유지되는지 확인.
