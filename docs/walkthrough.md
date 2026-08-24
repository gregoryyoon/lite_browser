# Lite Browser 전체 기능 & 시스템 구현 보고서 (Walkthrough)

본 문서는 **Lite Browser** 프로젝트의 전체 아키텍처 및 주요 기능별 구현 내역(기본 언어 설정, 다중 탭 및 윈도우 관리, 차세대 북마크 & 지능형 주소창, 커스텀 아이콘 리소스 자동화 파이프라인, 다운로드 관리자, 듀얼 탭/창 분할 시스템)을 통합하여 관리하는 전체 통합 기술 가이드입니다.

---

## 1. 기본 시스템 및 OS 언어 설정 (OS Default Language Sync)

### 1.1 개요
LiteBrowser 실행 시 Windows OS의 사용자 기본 로캘(UI Language)을 자동 감지하여 CEF의 내장 UI 및 웹페이지 언어 설정에 동적으로 반영하는 기능을 구현했습니다.

### 1.2 주요 구현 내용
1. **Windows OS 기본 언어 감지 (`GetUserDefaultLocaleName`)**:
   - Win32 API `GetUserDefaultLocaleName`을 호출하여 사용자 시스템의 기본 로캘(예: `ko-KR`)을 추출합니다. (실패 시 fallback: `ko-KR`)
2. **CEF 내장 UI 언어 설정 (`settings.locale`)**:
   - `settings.locale`에 OS 기본 로캘 문자열을 설정하여 패스워드 매니저, 컨텍스트 메뉴, DevTools, 경고창, PDF 뷰어 등 모든 CEF 내장 UI의 표기 언어를 시스템 언어로 자동 설정했습니다.
3. **웹 요청 및 JS 객체 언어 설정 (`settings.accept_language_list`)**:
   - HTTP Request Header의 `Accept-Language` 및 JavaScript `navigator.language` / `navigator.languages`에 언어 우선순위 목록(예: `ko-KR,ko,en-US,en`)을 전달하여 다국어 지원 웹사이트 접속 시 기본 한국어 페이지가 노출되도록 반영했습니다.
4. **메모리 관리**:
   - CEF 브라우저 프로세스 초기화(`cef_initialize`) 직후 동적 생성된 CEF 문자열 리소스(`settings.locale`, `settings.accept_language_list`)를 `cef_string_clear`로 안전하게 해제합니다.

### 1.3 관련 소스 코드
- [`cef_binary_149.0.6/tests/cefsimple_capi/cefsimple_win.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/cefsimple_win.c)

---

## 2. 탭 및 윈도우 관리 아키텍처 (Tab & Window Subsystem)

### 2.1 하이브리드 브라우저 아키텍처 (Win32 + 이중 자식 브라우저)
1. **CEF Views 우회 및 순수 Win32 윈도우 임베딩**:
   - CEF Views 프레임워크의 다중 브라우저 뷰 바인딩 제약 및 렌더링 누락 문제를 방지하고자 `simple_app.c`에서 `use_views = 0`으로 지정하여 순수 Win32 메시지 프로시저 기반 분기를 기동합니다.
   - 메인 윈도우 클래스(`LiteBrowserMainWindowClass`)를 등록한 뒤 `CreateWindowEx`로 최상위 메인 윈도우(`g_main_hwnd`)를 띄우고, 그 하위에 두 개의 독립된 자식 브라우저(`WS_CHILD`)를 임베딩합니다:
     - **상단 주소창 UI 브라우저**: 로컬 HTML/CSS/JS (`ui/index.html`)를 로드하는 컨트롤 브라우저
     - **하단 웹 콘텐츠 브라우저**: 웹 페이지를 렌더링하는 실체 콘텐츠 브라우저
2. **C API 참조 관리 (Ref-Counting) 규칙**:
   - C++ 스마트 포인터가 없는 순수 C 환경이므로, CEF 전역 API(`cef_browser_view_get_for_browser` 등) 호출 시 포인터 소유권 마샬링으로 인한 레퍼런스 카운트 파손을 막고자 인자 전달 전 `browser->base.add_ref`를 수동 호출하여 메모리 안전성(Access Violation 예외 차단)을 보장합니다.

### 2.2 동적 컨텍스트 기반 다중 탭(Multi-Tab) 및 멀티 윈도우 관리
1. **창별 동적 컨텍스트 구조체 (`browser_window_t`)**:
   - 싱글 윈도우 전역 변수를 제거하고, 윈도우 인스턴스 생성 시마다 `browser_window_t` 구조체를 동적 할당하여 Win32 `GWLP_USERDATA` 및 전역 관리 배열(`g_windows`)에 바인딩했습니다.
   - 개별 윈도우마다 `tabs` 배열(최대 10개), `active_tab_index`, `tab_count`를 가지고 있어 탭 목록, URL, Title, HWND 등의 상태를 독립적으로 관리합니다.
2. **활성 탭 스위칭 및 Z-order HWND 숨김/노출 제어**:
   - 탭 전환(`switch-tab`)이나 탭 생성/삭제 시, 활성화 대상 탭을 제외한 **나머지 모든 백그라운드 탭 브라우저들의 윈도우 핸들을 명시적으로 `ShowWindow(hwnd, SW_HIDE)` 처리**합니다.
   - 선택된 활성 탭만 `ShowWindow(hwnd, SW_SHOW)` 및 `MoveWindow`를 수행하여 전면 윈도우 뒤에 숨겨진 백그라운드 탭들이 겹쳐 보이거나 잔상이 남는 HWND Z-order 포개짐 현상을 완벽 차단했습니다.
3. **현재 활성 탭 바로 우측 위치 새 탭 삽입 (Active Tab Relative Insertion)**:
   - 새 탭 생성 시 무조건 전체 탭의 맨 우측 끝에 추가되던 기존 방식을 개편하여, 크롬/엣지 표준 UX와 동일하게 **현재 활성화된 탭의 바로 오른쪽(`active_tab_index + 1`) 위치에 새 탭이 즉시 삽입(Insert)**되도록 `CreateNewTab` 공통 함수 내 탭 배열 시프트(Shift) 알고리즘을 구현했습니다.

### 2.3 비동기 탭 분리(Reparenting) 및 드래그앤드롭 새 창 생성
1. **새로고침 없는 탭 Reparenting**:
   - 활성화된 자식 콘텐츠 브라우저 창의 부모 윈도우를 Win32 `SetParent` API로 신규 메인 윈도우에 동적 연결합니다.
   - 스타일 비트(`WS_POPUP` 해제, `WS_CHILD` 적용) 갱신 후 `MoveWindow` 및 `host->was_resized(host)`, `host->set_focus(host, 1)`를 수행하여 기존 웹 세션을 새로고침 없이 실시간 새 창으로 이관합니다.
2. **PointerEvent 캡처 기법을 통한 이탈 감지 및 커서 동기화**:
   - HTML5 DnD API 사용 시 OS 제한으로 윈도우 외부 드롭 시 포인터가 🚫(드롭 금지)로 고착되는 현상을 해결하고자 **PointerEvent `setPointerCapture`**를 도입했습니다.
   - 탭바 이탈 시 포인터 커서를 복사/추가 기호가 결합된 **`copy` 커서**로 실시간 전환하고, 마우스를 놓는 순간 스크린 좌표(`GetCursorPos`)를 백엔드로 넘겨 새 메인 윈도우를 해당 좌표에 스냅 팝업시킵니다.

### 2.4 비동기 수명 주기 및 메모리 안정성 (Use-After-Free 크래시 방지)
1. **비동기 메모리 소멸 위임 (UAF 크래시 차단)**:
   - Win32 `WM_DESTROY` 메시지가 수신되었을 때 `free(win_ctx)`를 성급하게 수행하면, CEF 내부 스레드의 비동기 브라우저 소멸 과정(`life_span_handler_on_before_close`)에서 댕글링 포인터를 참조하여 세그멘테이션 크래시가 유발됩니다.
   - 이를 막고자 `WM_DESTROY`에서는 브라우저 closure만 요청하고, **해당 창 내부의 UI 브라우저 및 모든 자식 탭 브라우저들의 소멸 콜백(`on_before_close`)이 100% 완료되어 카운트가 `0`이 되는 최종 시점에 비로소 `free(win_ctx)`가 실행되도록 소멸 라이프사이클을 격리**했습니다.
2. **개별 창 종료 표준 가드 (`_WIN32`)**:
   - 플랫폼 빌드 매크로 가드를 표준 **`_WIN32`**로 보정하여, 다중 창 구동 중 개별 팝업/분리 창을 닫았을 때 전체 프로그램이 동시 종료되는 문제를 막고, 메인 창 카운트(`g_window_count == 0`)가 마지막 1개일 때만 CEF 메시지 루프(`cef_quit_message_loop`)가 이탈되도록 보장했습니다.

### 2.5 커스텀 프레임리스 윈도우, High DPI & 듀얼 모니터 최대화 보정
1. **커스텀 타이틀바 및 탭바 마우스 드래그 이동**:
   - `WS_POPUP | WS_THICKFRAME` 프레임리스 구조를 채택하고 `DwmExtendFrameIntoClientArea` 그림자 효과를 입혔습니다.
   - HTML 탭바 여백 구역 마우스 다운 시 `drag-window` IPC를 쏘아, 백엔드에서 **`ReleaseCapture()` 후 `SendMessage(win_ctx->main_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0)`**을 동기 호출하여 마우스 드래그로 자연스럽게 창이 이동되도록 구현했습니다.
2. **High DPI 배율 대응 주소창 동적 높이 스케일링**:
   - `GetDpiForWindow` API로 디스플레이 배율(`scale = DPI / 96.0`)을 동적 구하고, 주소창 표준 논리 높이(`72px`)에 배율을 곱한 물리 크기(`ui_height = (int)(72.0 * scale)`)로 하단 콘텐츠 브라우저 Y 좌표(`ui_height + 1`)를 반응형 산출하여 DPI 변경 시에도 주소창 잘림 현상이 전혀 없도록 일원화했습니다.
3. **듀얼/멀티 모니터 `WM_GETMINMAXINFO` 오프셋 상대 좌표 보정**:
   - Win32 `WM_GETMINMAXINFO` 의 `mmi->ptMaxPosition` 은 가상 화면 전역 절대 좌표가 아닌 **"해당 모니터 오리진 기준 상대 좌표(Relative Coordinates)"**를 요구하므로 `mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left` 및 `y = mi.rcWork.top - mi.rcMonitor.top` 공식을 적용하여 보조 모니터 위치와 관계없이 항상 해당 디스플레이 내에서 정밀하게 전체화면 최대화가 동작하도록 수정했습니다.

### 2.6 링크/팝업 가로채기, 비동기 레이스 차단 & UX 고도화
1. **네이티브 팝업 차단 후 비동기 리디렉션 (Chromium 네임드 팝업 크래시 근절)**:
   - `window.open(url, "name")` 기반 네임드 팝업 인입 시 `on_before_popup` 콜백 내에서 팝업 생성을 원천 거부(`return 1`)하고 가로챈 URL을 `CreateNewTab(win_ctx, target_url_str)`로 넘겨 안정적인 새 탭으로 오픈시켰습니다.
2. **비동기 탭-핸들러 일대일 포인터 매칭 (`void* tab_handler`)**:
   - `tab_info_t`에 `void* tab_handler` 필드를 신설하고, `on_after_created` 호출 시 자신의 parent 핸들러 주소와 일치하는 탭 슬롯을 찾아 일대일 정밀 할당하여 고속 연달아 탭을 누를 때 발생하던 레이스 컨디션을 해결했습니다.
3. **JSON 특수문자 이스케이프 (`EscapeJsonString`)**:
   - C++ 백엔드에 `EscapeJsonString` 이스케이프 헬퍼를 자체 구현해 탭 타이틀/URL 특수기호로 인한 `JSON.parse` 자바스크립트 크래시를 차단했습니다.
4. **탭바 및 웹 콘텐츠 우클릭 네이티브 컨텍스트 메뉴**:
   - `TrackPopupMenu`를 통해 **새 탭 / 구분선 / 탭 닫기 / 다른 탭 닫기 / 오른쪽 탭 닫기** 및 웹 콘텐츠 링크 우클릭 분기 메뉴를 구현했습니다.
5. **탭 삭제/종료 시 활성 탭 전환 및 HWND Z-order 노출 보정 (`RemoveTabAt`)**:
   - 탭 삭제/이관 전용 공통 헬퍼 `RemoveTabAt`을 도입하여 활성 탭 종료 시 좌측 이전 탭(`remove_idx - 1`)이 즉각 활성화되도록 보정하고, 비활성 탭 HWND `SW_HIDE` 및 활성 탭 HWND `SW_SHOW`를 보장하여 브라우저 회색 바탕화면 현상을 완전히 차단했습니다.
6. **주소창 URL 직접 입력 후 엔터 시 로딩 완료 URL 갱신 (`handleKey` blur)**:
   - 주소창 입력 후 Enter 키 입력 시 `event.target.blur()`를 호출하여 포커스를 해제함으로써, C 백엔드의 `on_address_change` 실행 시 `updateAddress` 가드 조건문이 정상 동작하여 이동 완료 후의 최종 전체 URL(`https://...`)로 화면 주소창이 즉시 갱신되도록 처리했습니다.
7. **웹페이지 링크 Ctrl+클릭 / 휠클릭 시 새 탭 생성 및 기본 새 창 차단 (`on_open_urlfrom_tab`)**:
   - CEF `cef_request_handler_t` 구조체에 `on_open_urlfrom_tab` 콜백 핸들러(`request_handler_on_open_urlfrom_tab`)를 추가 바인딩했습니다.
   - 사용자가 웹페이지 내 일반 링크를 **Ctrl + 좌클릭** 또는 **휠클릭(Middle-click)**하는 이벤트를 가로채 target URL을 `CreateNewTab(win_ctx, target_url_str)`로 넘겨 기존 메인 창의 새 탭으로 오픈하고, `return 1`을 반환하여 Chromium 디폴트 팝업/새 창 생성을 원천 차단했습니다.

### 2.7 긴 URL(네이버 리다이렉트 링크) 잘림 버그 수정 및 URL 버퍼 확장 (Long URL Buffer Expansion)
1. **문제 원인 분석**:
   - 네이버 검색 결과('한강밤핑' 등) 섬네일 클릭 시 호출되는 긴 리다이렉트/추적 URL(1,332자 이상) 인입 시, C API 내부 URL 버퍼 크기가 `1024`자로 제한되어 있어 쿼리 스트링의 리다이렉트 타겟 파라미터(`&u=https%3A%2F%2Fcontents...`)가 잘려(Truncated) 손상된 URL이 CEF에 전달됨.
   - 손상된 URL을 수신한 네이버 서버가 리다이렉트 목표 지점을 정상 해석하지 못하고 디폴트 메인 페이지(`https://www.naver.com`)로 302/JS 리다이렉트하는 현상 발생.
2. **구현 및 버퍼 확장**:
   - C API 데이터 구조체 및 주요 핸들러의 URL 버퍼 규격을 **`1024`자에서 `4096`자**로 일괄 확장:
     - [`browser_context.h`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/browser_context.h): `tab_info_t.url` 버퍼 크기 `4096`자로 확장.
     - [`simple_life_span_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_life_span_handler.c): `life_span_handler_on_before_popup` 내 `target_url_str[4096]` 버퍼 확장.
     - [`simple_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.c): `g_startup_url[4096]`, `on_open_urlfrom_tab` 내 `target_url_str[4096]`, `detach-tab`/`drag-end` 내 `target_url[4096]` 확장.
     - `update_ui_tabs` 및 `update_ui_nav_state` 내 `escaped_url[4096]`, `tab_str[5000]`, `json[65536]`, `js_code[70000]` 버퍼 크기를 확장하여 JSON 잘림 오작동 원천 차단.
     - [`simple_app.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_app.c): `create_browser_window` 내 `target_url[4096]` 확장.
3. **검증**:
   - 1,300자 이상의 긴 리다이렉트 URL 클릭 시에도 URL이 잘리지 않고 원래 연결 목적지(`https://contents.premium.naver.com/...`)로 정상 이동하는 것을 확인.

### 2.8 네이버 메일 삭제 UI 미갱신 버그 수정 (`disable-web-security` 제거)
1. **문제 원인 분석**:
   - 네이버 홈 `my-iframe`(`https://www.naver.com/my.html`) 메일 탭에서 메일 삭제 시, AJAX 요청으로 서버 상 메일은 정상 삭제되지만 `iframe` ↔ 메인 프레임 간 `window.postMessage` 비동기 이벤트를 통해 UI를 리렌더링하는 과정에서 이벤트가 무시되는 현상 발생.
   - 원인은 C 백엔드 `simple_app.c`의 `simple_app_on_before_command_line_processing` 수신기에서 `--disable-web-security` 커맨드 라인 플래그가 적용되어 있어, Chromium이 `postMessage` 이벤트의 `event.origin`을 오염/누락 전달함으로써 네이버 프론트엔드의 `if (event.origin !== 'https://www.naver.com') return;` 보안 검증을 통과하지 못한 것.
2. **구현 및 해결책**:
   - [`simple_app.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_app.c#L167-L175): `disable-web-security` 커맨드 라인 스위치 주입 코드 제거.
   - 로컬 UI 통신(`ui/index.html`, `ui/editor.html`)은 `http://ui-action/...` URL 가로채기 방식이므로 보안 플래그 제거 후에도 100% 정상 작동함을 확인.
3. **검증**:
   - 네이버 홈 메인 프레임 내 메일 탭에서 메일 삭제 시 `event.origin` 검증이 정상 통과되어, 새로고침 없이 메일 리스트 항목이 화면에서 즉시 제거되는 것을 검증 완료.

---

## 3. 차세대 북마크 & 지능형 주소창 UX (Smart Omnibox & Bookmark)

### 3.1 맥락 자동 추출 및 문장 점수 기반 본문 요약 엔진 (Context & Extractive Summarization Engine)
- **소스**: [`ui/extractor.js`](file:///c:/projects/lite_browser/ui/extractor.js)
- **심층 iframe 추출 (`getDeepSelectionText` & `getDeepBodyText`)**:
  - 네이버 블로그(`mainFrame`) 등 `iframe` 구조 페이지에서도 최상위 창과 자식 프레임을 심층 탐색하여 마우스 드래그 선택 문장 및 실체 본문 텍스트를 100% 수집.
- **UI 노이즈 영역 제거 & 확장 불용어 사전**:
  - `<nav>`, `<footer>`, `.pagination`, `button` 등 UI 노이즈 구역을 정제하고, `이전페이지`, `다음페이지`, `바로가기`, `페이지`, `보기`, `the`, `and` 등의 무의미한 UI 단어와 접속사를 원천 필터링.
- **제목/헤딩 가중치(x4) 기반 스마트 태그 추출 (`extractSmartTags`)**:
  - 문서 제목(`document.title`) 및 `<h1>`~`<h3>` 헤딩, `<strong>` 강조 어휘에 4배 가중치를 부여하여 핵심 주제어 태그 5개 정제.
- **문장 중요도 점수 기반 스마트 본문 요약 (`extractSmartSummary`)**:
  - 마침표/물음표/느낌표 기반 완전한 문장 분할 후 문단 위치, 제목 어휘, 스마트 태그 포함 여부 3가지 지표로 문장별 중요도 점수(Score)를 산출하여 상위 2~3개 고득점 문장을 이은 깔끔한 200자 요약문 생성.
- **저장 항목**: `og:image` 썸네일, `og:description` 또는 스마트 본문 요약 스니펫, `document.referrer` 기반 유입 검색어(Search Intent), 키워드 태그, 드래그 선택 문장 앵커(`selectedText`) 및 스크롤 위치 보존.

### 3.2 원클릭 추가/제거 토글 UX
- 별 버튼 클릭 시 팝업 모달 없이 `☆` ↔ `★` 아이콘 상태 변화만으로 북마크 추가 및 즉시 제거 토글 제공.

### 3.3 지능형 주소창 엔진 (Smart Omnibox Engine)
- **소스**: [`ui/app.js`](file:///c:/projects/lite_browser/ui/app.js), [`ui/index.html`](file:///c:/projects/lite_browser/ui/index.html), [`ui/style.css`](file:///c:/projects/lite_browser/ui/style.css)
- **다차원 검색 & `#` 태그 숏컷**: URL, 제목, 스니펫, 유입 검색어, 태그까지 통합 탐색 및 `#` 키워드 드롭다운 필터링.
- **동적 HWND 높이 계산 & 3행 인라인 카드**: 드롭다운 높이를 측정하여 네이티브 UI HWND를 동적 확장(`expandUI`)하고, 양옆 영역은 `transparent` 처리하여 웹 화면을 보호.
- **최근 30일 방문 빈도 & 최신성 2단계 정렬 알고리즘**:
  - **북마크**: 1순위(최근 30일간 다빈도 방문) ➔ 2순위(최근 추가된 북마크 생성일)
  - **방문 기록**: 1순위(최근 30일간 다빈도 접속) ➔ 2순위(최근 접속 시각)
- **방문 횟수(`👁️ N회 방문`) 드롭다운 표기 추가**:
  - 북마크: `📅 N일 전 저장 · 👁️ M회 방문`
  - 방문 기록: `📅 N일 전 방문 · 👁️ M회 방문`
  - URL 접속 시 `addHistoryEntry`에서 누적 방문 횟수(`visitCount`)를 1씩 동적 증가시키고 `history.json` / `bookmarks_v2.json` 파일에 영속화.
- **통합 검색 결과 순서**: 주소창 입력 시 북마크(상단 3개) ➔ 구글 검색(중간) ➔ 방문 기록(하단 3개) 순으로 인라인 카드 통합 렌더링.
- **키보드 드롭다운 선택 Enter 연동 보정 (`handleKey`)**:
  - 드롭다운이 열리고 방향키로 항목이 선택된 상태(`isDropdownOpen && omniSelectedIndex >= 0`) 시 `handleKey` 입력을 리턴 처리하여 구글 검색 오버라이드를 차단하고 선택된 북마크/방문기록 URL로 정합 이동.
- **백스페이스 입력 시 주소창 포커스 유지**: 주소창 텍스트를 백스페이스 키로 모두 지워 `query`가 빈 문자열(`""`)이 될 때 `closeOmniboxDropdown()` 내부의 자동 `addressBar.blur()`를 분리 제거하여, 텍스트가 모두 지워져도 주소창 키 포커스가 이탈하지 않고 연달아 입력할 수 있도록 보정.

### 3.4 방문 기록 (History) 영속성 & C 백엔드 IPC 보정
- **소스**: [`simple_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.c), [`ui/app.js`](file:///c:/projects/lite_browser/ui/app.js)
- **C 백엔드 `ExecuteJsOnBrowser` 전송 타겟 보정**: `load-history` 및 `load-bookmarks-v2` 액션 수신 시 읽어온 JSON 데이터를 활성 콘텐츠 탭이 아닌 주소창 UI 브라우저(`win_ctx->ui_browser`)로 우선 전달하도록 보정하여 앱 재기동 시에도 로컬 `history.json` 파일 데이터가 100% 정상 복원 복구됨.
- **시동 IPC 딜레이 부여**: `requestLoadBookmarks()` 시동 시 `load-history` 요청에 150ms 딜레이를 부여하여 내비게이션 간섭 방지.

### 3.5 북마크 전용 대시보드 (`lite://favorites`)
- **소스**: [`ui/manager.html`](file:///c:/projects/lite_browser/ui/manager.html), [`ui/manager.js`](file:///c:/projects/lite_browser/ui/manager.js), [`ui/manager.css`](file:///c:/projects/lite_browser/ui/manager.css)
- **기능**: 브라우저 디폴트 스타트업 URL, Ctrl+Shift+O 단축키 연결. 사이드바 태그 리스트, Temporal Slider 타임라인 필터, Card/List 뷰 스위처 제공.
- **탭 제목 일원화**: 새 탭 및 `lite://favorites` 접속 시 탭 제목을 `북마크 관리자`로 일원화 표기.
- **대시보드 정렬 알고리즘**: 전체 북마크 목록을 1순위(최근 30일간 다빈도 방문) ➔ 2순위(최근 추가된 북마크 생성일) 순으로 정렬하여 그리드/리스트 뷰에 우선 렌더링.
- **`✨ 하이라이트 앵커` 카드 & 리스트 뷰 통합 뱃지**: 본문 영역을 마우스 드래그한 문장이 함께 저장된 북마크에 대해 카드형 뷰(`card-highlight-box`) 및 리스트형 뷰(`list-row-highlight`) 모두 100% 동일한 디자인 시스템(황금색 엠버 `3px` 테두리, `#fffbe6` 배경, `#78350f` 글자색)으로 일원화하여 직관 노출.
- **카드 & 리스트 뷰 방문 횟수 표기**:
  - 카드형 뷰: 하단 영역에 저장 시점과 방문 횟수 병행 표시 (`14일 전 · 👁️ 5회 방문`).
  - 리스트형 뷰: 테이블 날짜 셀에 방문 횟수 표기 (`14일 전 · 👁️ 5회`) 및 레이아웃 너비 확장 (`min-width: 145px`).

### 3.6 동적 설치 경로 해결 (`ResolveUIFilePath`)
- **소스**: [`simple_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.c)
- `GetModuleFileNameA` 기반으로 실행 파일 상위 폴더 트리를 순회하여 `ui/manager.html` 등 에셋을 탐색하므로 어떠한 설치 경로에서도 하드코딩 에러 없이 100% 동작.

---

## 4. 아이콘 디자인 & 윈도우 리소스 자동 주입 파이프라인 (Icon & Resource Pipeline)

### 4.1 디자인 의도
 Notepads UX 컨셉의 초경량 브라우저 제품 철학을 담아 **깃털(Feather)**과 **슬림 브라우저 창(Slim Window)**이 결합된 현대적 플랫 아이콘 적용.

### 4.2 Windows 표준 멀티 프레임 ICO 규격
- **16x16, 24x24, 32x32, 48x48**: 32-bit Alpha Raw BMP (DIB)
- **256x256**: PNG 압축 인코딩
- **파일**: [`cefsimple.ico`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/win/cefsimple.ico)

### 4.3 Win32 Resource API 기반 자동 주입 엔진 (`inject_icon.py`)
- **소스**: [`cef_binary_149.0.6/tests/cefsimple_capi/win/inject_icon.py`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/win/inject_icon.py)
- `BeginUpdateResourceW`, `UpdateResourceW`, `EndUpdateResourceW` 함수를 Python `ctypes`로 바인딩하여, CEF 빌드 시 샌드박스 래퍼 바이너리 복사(`bootstrap.exe` -> `cefsimple_capi.exe`)로 인해 커스텀 아이콘 리소스가 누락되는 이슈를 원천 차단.
- CMake [`CMakeLists.txt`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/CMakeLists.txt)의 `POST_BUILD` 커스텀 명령으로 자동 통합되어 빌드 시 5개 해상도의 아이콘 리소스가 자동 주입됨.

---

## 5. Release 빌드 & NSIS 패키징 가이드 (Packaging & Distribution)

### 5.1 패키징 개요
- **최종 인스톨러**: [`LiteBrowserInstaller.exe`](file:///c:/projects/lite_browser/LiteBrowserInstaller.exe) (~173MB)
- **특징**: 64-bit 설치 지원(`$PROGRAMFILES64\LiteBrowser`), 시작 메뉴 및 바탕화면 바로가기 자동 생성, 제어판 프로그램 추가/제거 연동, UI 에셋(`ui\*.*`) 와일드카드 자동 동기화.

### 5.2 NSIS 인스톨러 스크립트
- **소스**: [`installer.nsi`](file:///c:/projects/lite_browser/installer.nsi)

### 5.3 원클릭 빌드 & 패키징 실행 명령
```powershell
# 1. CMake Debug 빌드 (개발 및 테스트용)
cmake --build c:\projects\lite_browser\cef_binary_149.0.6\build --config Debug --target cefsimple_capi

# 2. CMake Release 빌드 및 NSIS 인스톨러 패키징 (배포용)
cmake --build c:\projects\lite_browser\cef_binary_149.0.6\build --config Release --target cefsimple_capi
& "C:\Program Files (x86)\NSIS\makensis.exe" c:\projects\lite_browser\installer.nsi
```

---

## 6. 주요 소스 파일 맵 (File Directory Map)

- [`simple_app.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_app.c): Win32 메인 프로시저, `WM_GETMINMAXINFO` 보정, DPI 스케일링, 분할 레이아웃/리사이저, `WM_MOUSEACTIVATE` 포커스 감지
- [`simple_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.c): `RemoveTabAt` 탭 삭제, `CreateNewTab` 상대 위치 삽입, `ResolveUIFilePath` 동적 경로 탐색, 듀얼 탭 IPC, Focus Handler (`cef_focus_handler_t`), `update_ui_tabs` 콤보 제목 연동
- [`simple_life_span_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_life_span_handler.c): 팝업 가로채기 리디렉션, 비동기 소멸 UAF 가드, 듀얼 브라우저 등록 및 클린업
- [`simple_display_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_display_handler.c): 주소 변경, 타이틀 변경 이벤트 동기화 (우측 분할 브라우저 감지 연동)
- [`browser_context.h`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/browser_context.h): 동적 윈도우/탭 컨텍스트 구조체 정의 (`is_split`, `right_browser`, `right_hwnd`, `right_title`, `right_url`, `active_split`, `split_ratio`)
- [`ui/app.js`](file:///c:/projects/lite_browser/ui/app.js): 주소창 포커스/blur, Omnibox 통합 그룹화 검색 엔진, 듀얼 탭 토글 `toggleDualSplit`, `.tab-split-badge` 분할 뱃지 연동
- [`ui/index.html`](file:///c:/projects/lite_browser/ui/index.html) & [`ui/style.css`](file:///c:/projects/lite_browser/ui/style.css): 상단 주소창/탭바 네이티브 UI 레이아웃, 주소창 우측 듀얼 버튼 위치, 듀얼 분할 뱃지 스타일
- [`ui/manager.html`](file:///c:/projects/lite_browser/ui/manager.html) & [`ui/manager.js`](file:///c:/projects/lite_browser/ui/manager.js): `lite://favorites` 대시보드
- [`installer.nsi`](file:///c:/projects/lite_browser/installer.nsi): NSIS 설치 파일 빌드 스크립트
- [`simple_download_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_download_handler.c) & [`simple_download_handler.h`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_download_handler.h): CEF 다운로드 진행률 추적 핸들러, 파일 중복 자동 순서 번호 부여, 영속 JSON 관리, IPC 액션
- [`ui/downloads.html`](file:///c:/projects/lite_browser/ui/downloads.html), [`ui/downloads.js`](file:///c:/projects/lite_browser/ui/downloads.js), [`ui/downloads.css`](file:///c:/projects/lite_browser/ui/downloads.css): `lite://downloads` 다운로드 대시보드 UI

---

## 7. 다운로드 관리자 대시보드 시스템 (`lite://downloads`)

### 7.1 개요
Edge 브라우저의 `edge://downloads/` 디자인과 UX를 참고하여 다운로드 진행률 실시간 추적, 자동 다운로드 디렉토리 고정 및 중복 번호 부여, 영속 이력 관리, 파일 실존 검사 및 파일 관리 기능을 탑재했습니다.

### 7.2 주요 구현 내역
1. **사용자 기본 다운로드 폴더 고정 & 자동 다운로드**:
   - `SHGetSpecialFolderPathA`를 사용해 `%USERPROFILE%\Downloads`로 저장 경로를 고정하고 `show_dialog = 0` 처리하여 Save As 대화상자 없이 즉시 자동 저장됩니다.
2. **동일 파일명 자동 순서 번호 부여 (중복 방지)**:
   - 동일 파일 존재 시 `filename (1).ext`, `filename (2).ext` 로 순서 번호를 자동으로 부여해 파일 덮어쓰기를 원천 방지합니다.
3. **다운로드 진행 추적 및 영속성 (`downloads.json`)**:
   - CEF `cef_download_handler_t` C API 인터페이스를 구현하고 `cef_client_t`에 바인딩했습니다.
   - 전송 바이트, 총 바이트, 속도, 진행률(%), 상태(다운로드 중/완료/일시중지/취소/실패)를 `%USERPROFILE%\.lite-browser\downloads.json`에 기록하여 재기동 시에도 이력을 복원합니다.
4. **대시보드 UI (`lite://downloads`) 및 파일 관리**:
   - `lite://downloads`, `edge://downloads`, `chrome://downloads` 주소 이동 지원.
   - 확장자별 스타일 아이콘, 카테고리 탭, 파일실존 검사(파일 미존재 시 뱃지 표시 및 열기 비활성화), 파일 실행, 탐색기 폴더에서 보기, 파일 물리 삭제, 이력 정리, 파일 정보 확인 모달 제공.
5. **IPC 통신 및 실시간 푸시 엔진**:
   - 프론트엔드 UI(`ui/downloads.js`)에서 Lite Browser의 네이티브 프레임 가로채기 방식인 `window.location.href = 'http://ui-action/...'`을 통해 안전하게 C 백엔드 명령을 호출합니다.
   - 백엔드의 `BroadcastDownloadUpdate()`를 구동하여 다운로드 상태 갱신 시 대시보드 화면으로 최신 목록 데이터(`window.renderDownloads`)를 실시간 푸시(Push)합니다.
6. **새 탭 개설 연동**:
   - 주소창 우측 다운로드 버튼, `Ctrl+J` 단축키, 3점 메뉴 "다운로드 (Ctrl+J)" 실행 시 백엔드 `open-download-manager` IPC를 거쳐 `CreateNewTab`을 호출하여 활성 탭 바로 우측에 새로운 탭으로 다운로드 대시보드가 오픈됩니다.

---

## 8. 듀얼 탭 (창 분할) 시스템 (Dual Tab & Split Screen Subsystem)

### 8.1 개요
네이버웨일 브라우저의 듀얼 탭 UX를 기반으로, 한 탭 내에서 두 개의 페이지를 좌/우 가로 분할 탐색 및 제어할 수 있는 기능을 구현했습니다.

### 8.2 주요 구현 내역
1. **주소 표시창 우측 분할 버튼 위치 조정**:
   - 툴바 주소 표시창(`address-container`) 바로 오른편에 듀얼 분할 버튼(`dual-split-btn`)을 배치하여 1클릭 분할 토글 UX를 제공합니다.
2. **독립된 탭별 분할 상태 관리 (Per-Tab State Isolation)**:
   - `tab_info_t` 구조체에 `is_split`, `right_browser`, `right_hwnd`, `right_title`, `right_url`, `active_split` (0: 좌측, 1: 우측), `split_ratio` (기본 0.5f) 필드를 추가하여 탭별 독립적인 분할 뷰 생명주기를 관리합니다.
3. **단일 주소창 동기화 & 실시간 이중 포커스 센서**:
   - 주소창은 1개로 유지되며 현재 마우스로 선택된 분할 화면(좌/우)의 URL, 제목, 뒤로/앞으로 가기, 새로고침, 차세대 북마크 수집 대상이 100% 동기화됩니다.
   - Win32 `WM_MOUSEACTIVATE` / `WM_PARENTNOTIFY` 포인터 좌표 추적과 CEF `cef_focus_handler_t` (`on_set_focus`, `on_got_focus`) 콜백 센서를 이중 바인딩하여, 웹페이지 영역 마우스 클릭 시 **2px 브랜드 블루(`#0066cc`) 포커스 테두리**가 활성화된 화면으로 실시간 전환됩니다.
4. **마우스 리사이저 바 (Interactive Splitter Bar)**:
   - 6px 폭의 분할 바 호버 시 `IDC_SIZEWE` (↔) 마우스 커서로 전환되고, 드래그 시 분할 비율(`0.2f` ~ `0.8f`)을 자유롭게 조정할 수 있습니다.
5. **우클릭 컨텍스트 메뉴 '다른 분할 화면에서 열기'**:
   - 웹페이지 링크 우클릭 시 "다른 분할 화면에서 열기" 메뉴를 제공하며, 단일 탭 상태일 경우 자동으로 탭이 듀얼 분할 모드로 전환되며 오른쪽 화면에 링크를 로딩합니다.
6. **`[좌측 제목 | 우측 제목]` 콤보 제목 & 듀얼 뱃지 아이콘 (`◫`)**:
   - 탭이 분할되면 탭바에 `[좌측 페이지 제목 | 우측 페이지 제목]` 결합 텍스트가 표시되며, 탭 제목 좌측에 파란색 듀얼 분할 아이콘(`◫`) 뱃지가 노출됩니다.
7. **전역 비활성 탭 HWND 일괄 `SW_HIDE` 보정 (잔상 방지)**:
   - 탭 전환(`switch-tab`), 탭 삭제(`RemoveTabAt`), 로딩 상태 전환(`simple_load_handler`), 레이아웃 계산(`WM_SIZE`) 시 현재 선택되지 않은 모든 비활성 탭의 메인 HWND(`hwnd`) 및 우측 HWND(`right_hwnd`)를 일괄 `SW_HIDE` 처리하여 탭 전환 시 화면 잔상이 겹쳐 보이던 오작동을 원천 해결했습니다.

---

## 9. 빌드 및 테스트 가이드 (Development & Build Guide)

```powershell
# Debug 모드 빌드 (개발 및 기능 테스트)
cmake --build c:\projects\lite_browser\cef_binary_149.0.6\build --config Debug --target cefsimple_capi
```

---

## 10. 마크다운 에디터 기능 제거 (Markdown Editor Feature Removal)

### 10.1 개요
초기 아키텍처에 구현되어 있던 **마크다운 에디터(Markdown Editor)** 기능이 실사용성이 떨어지고 불필요한 스타트업 리소스(숨겨진 자식 브라우저 동시 기동)를 소비함에 따라, 툴바 UI, 팝업 메뉴, C 백엔드 구조체/IPC 및 스타트업 초기화 로직 전반에서 제거하였습니다.

### 10.2 주요 작업 내용
1. **HTML/JS UI 제거**:
   - [`ui/index.html`](file:///c:/projects/lite_browser/ui/index.html): 네비게이션 툴바 내 마크다운 에디터 토글 버튼 (`#editor-toggle-btn`) 삭제.
   - [`ui/app.js`](file:///c:/projects/lite_browser/ui/app.js): `toggleEditor()` 액션 함수 삭제.
   - [`ui/editor.html`](file:///c:/projects/lite_browser/ui/editor.html): 사용되지 않는 에디터 HTML 페이지 삭제.
2. **C 백엔드 아키텍처 정리**:
   - [`browser_context.h`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/browser_context.h): `browser_window_t` 내 `editor_browser`, `editor_hwnd`, `show_editor` 멤버 삭제.
   - [`simple_handler.h`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.h): `browser_type_t` 열거형 내 `BROWSER_TYPE_EDITOR` 항목 삭제.
   - [`simple_app.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_app.c): `ResolveEditorPath` 제거, 앱 기동 시 에디터 자식 브라우저 생성 로직(Step 3) 제거, `WM_SIZE` 레이아웃 분할 계산 제거, 윈도우 소멸 시 에디터 브라우저 닫기 처리 제거.
   - [`simple_life_span_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_life_span_handler.c): `on_after_created` 및 `on_before_close` 수명 주기 핸들러 내 `editor_browser` 참조/해제 코드 제거.
   - [`simple_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.c): `show-menu` 메뉴 내 "마크다운 에디터 토글" (cmd 1007) 항목 삭제, `toggle-editor` 및 모든 `editor-*` IPC 통신 액션 루틴 제거.
3. **검증**:
   - Debug 빌드 (`cmake --build c:\projects\lite_browser\cef_binary_149.0.6\build --config Debug --target cefsimple_capi`) 결과 정상 종료 (Exit code 0).

---

## 11. 실행 파일명 변경 (Executable Output Renaming: `cefsimple_capi.exe` -> `lite_browser.exe`)

### 11.1 개요
기존 CEF sample 바이너리 이름인 `cefsimple_capi.exe`에서 브라우저 고유 명칭인 **`lite_browser.exe`**로 실행 파일 및 DLL 출력 명칭을 변경하였습니다.

### 11.2 주요 작업 내용
1. **CMake 설정 수정**:
   - [`cef_binary_149.0.6/tests/cefsimple_capi/CMakeLists.txt`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/CMakeLists.txt): `set_target_properties(${CEF_TARGET} PROPERTIES OUTPUT_NAME "lite_browser")`를 추가하여 Debug 및 Release 모드 모두 `lite_browser.exe` 및 `lite_browser.dll`로 출력되도록 수정.
   - 부트스트랩 바이너리 복사(`COPY_SINGLE_FILE`) 및 아이콘 커스텀 주입(`inject_icon.py`) 대상을 `lite_browser.exe`로 업데이트.
2. **NSIS 인스톨러 스크립트 수정**:
- [`installer.nsi`](file:///c:/projects/lite_browser/installer.nsi): 설치 대상 바이너리를 `lite_browser.exe` / `lite_browser.dll`로 변경, 시작 메뉴 및 바탕화면 바로가기 target을 `lite_browser.exe`로 수정, 언인스톨 삭제 항목 갱신.
3. **VSCode 환경설정 수정**:
   - [`.vscode/launch.json`](file:///c:/projects/lite_browser/.vscode/launch.json): 디버거 실행 파일 경로를 `Debug/lite_browser.exe`로 반영.
4. **빌드 & 패키징 검증**:
   - Debug 빌드: `Debug/lite_browser.exe` 생성 확인 (Exit code 0).
   - Release 빌드: `Release/lite_browser.exe` 생성 확인 (Exit code 0).
   - NSIS 인스톨러: [`LiteBrowserInstaller.exe`](file:///c:/projects/lite_browser/LiteBrowserInstaller.exe) 재생성 완료 (Exit code 0).

---

## 12. AI 에이전트 브라우저 서브시스템 (AI Agent Browser Subsystem)

### 12.1 개요
사용자 맞춤형 실시간 상호작용 AI 에이전트, 메인 윈도우 우측 독립 네이티브 도킹 사이드패널, Rust 기반 MCP Server, 멀티 AI Provider 추상화(Gemini 3.7 Flash 기본 탑재), Task Runtime 상태 머신(자율 복구 및 수동 개입), Windows DPAPI 암호화 로컬 보안 볼트(Vault), 그리고 벡터 DB / 시맨틱 기억 엔진 및 프라이버시 데이터 컨트롤을 구축했습니다.

### 12.2 주요 구현 내역
1. **독립 네이티브 자식 브라우저(HWND) 도킹 사이드패널 (`simple_app.c`, `simple_life_span_handler.c`, `ui/sidepanel.*`)**:
   - **완전한 구조적 분리 (Architecture Decoupling)**: 듀얼 탭 분할 기능(`tabs[i].is_split`)에 종속되어 있던 구조에서 탈피하여, 메인 윈도우 레벨의 독립된 네이티브 자식 브라우저(`win_ctx->sidepanel_browser`, `BROWSER_TYPE_SIDEPANEL`)로 상시 도킹.
   - **탭 전환 시 대화 지속성 (Session Persistence)**: 상단 탭을 전환하거나 새 탭을 열어도 우측 AI 사이드패널이 닫히거나 초기화되지 않고, 진행 중인 대화 내역 및 실행 상태가 끊김 없이 유지됨.
   - **인터랙티브 스플리터 폭 조절 (Dynamic Resizing Splitter)**: 메인 콘텐츠 영역과 사이드패널 사이의 5px 스플리터 바 호버 시 `IDC_SIZEWE` 좌우 화살표 커서로 전환되고, 마우스 드래그를 통해 사이드패널 폭(기본 380px, 최소 260px ~ 최대 가용폭)을 자유롭게 조절.
   - **1클릭 토글 & 툴바 상태 동기화**: 상단 네비게이션 툴바의 AI 버튼(`✨`, `#ai-agent-btn`) 및 `Ctrl+Shift+A` 단축키로 On/Off 토글이 작동하며, 열림 상태에 따라 버튼의 `.active` 하이라이트 스타일이 실시간 동기화됨.
   - **실시간 사고 과정 및 상태 제어**: Thinking(CoT) 아코디언 뷰, 동작 스텝별 타임라인 카드, 실시간 상태 머신 뱃지(`IDLE`, `RUNNING`, `PAUSED`, `STUCK`, `WAITING`, `DONE`), 사용자 제어 버튼(일시정지/계속/중단/수동 직접 개입) 제공.

2. **듀얼 분할 화면 연동 및 활성 화면(Active Split) 동적 타겟팅 (`simple_handler.c`)**:
   - 듀얼 탭 분할 모드(`active_tab->is_split == 1`)에서 사용자가 마우스로 클릭하거나 포커스한 화면(`active_tab->active_split`: 0 좌측, 1 우측)을 동적으로 감지.
   - AI DOM 추출 및 상호작용 도구(`ai-get-dom-summary`, `ai-click-element`, `ai-type-element`, `ai-scroll`, `ai-highlight-element`, `vault-autofill`)가 현재 활성화된 화면(`cb`)의 URL과 본문을 정확하게 타겟팅하여 조작 및 요약 수행.

3. **Chrome Gemini 스타일 5단계 본문 파싱 & YouTube 특화 마크다운 추출 엔진 (`ui/content_extractor.js`, `simple_handler.c`)**:
   - **1단계 (Smart Frame Candidate Scoring)**: 최상위 `document` 및 네이버 블로그(`<iframe id="mainFrame">`), 다음 카페 등 중첩 `iframe` 구조를 재귀 탐색하여 특수 아티클 컨테이너(`.se-main-container`, `#postViewArea`, `#dic_area`, `#articleBody`, `.entry-content`, `article`, `main`) 매칭 점수로 1위 프레임 자동 선정.
   - **2단계 (Semantic & Visual Noise Filtering)**: DOM 복제본을 기반으로 `<nav>`, `<header>`, `<footer>`, `<aside>`, `<script>`, `<style>`, 광고 배너(`.ad`, `.banner`, `.sponsor`), 소셜 공유(`.sns-share`), 댓글창(`.comment`, `.reply`), 비가시 텍스트(`display: none`, `visibility: hidden`, `opacity < 0.05`)를 완벽 제거.
   - **3단계 (Readability & Viewport Heuristic Scoring)**: 텍스트 길이, 쉼표 빈도, 링크 밀도(역가중치), `getBoundingClientRect()` 기준 뷰포트 중앙 배치 가중치(+30%)를 결합하여 최적 본문 컨테이너(Core Article Block)를 격리.
   - **4단계 (Clean Markdown Serializer)**: 제목(`h1`~`h6`), 단락(`p`), 인용구(`blockquote`), 목록(`ul`/`ol`/`li`), 표(`table`/`tr`/`th`/`td`), 코드 블록(`pre`/`code`), 볼드/이탤릭 서식을 온전히 보존하는 경량 직렬화기 구동.
   - **5단계 (YouTube 특화 초경량 전처리 & 페이로드 패키징)**: `youtube.com` 접속 시 수십만 토큰에 달하는 추천 목록 및 댓글 노이즈를 원천 차단하고, 영상 제목, 채널명, 더보기 본문 설명, 챕터/타임라인(`00:00 - ...`), 조작 버튼만 500~1,500 토큰 수준의 초경량 Markdown으로 선별 추출하여 전송.

4. **AI Provider 플러그인 추상화 계층 & 429 내결함성 (`ui/ai_providers.js`)**:
   - 표준화된 `AIProviderInterface`를 바탕으로 다형성 어댑터 구현:
     - **Google Gemini**: 기본 모델로 **`gemini-3.7-flash`** 적용 (`gemini-3.6-flash`, `gemini-3.5-flash`, `gemini-3.1-pro-preview` 선택 지원), SSE 실시간 스트리밍 및 Function Calling 완벽 연동.
     - **Gemini 3.x 사고 상태(Stateful Reasoning) 연동**: 다중 턴 Function Calling 시 `thought_signature` 보존 및 에코백, 공식 `role: 'user'` 규격 적용으로 400 스키마 에러 원천 해결.
     - **429 Rate Limit 자동 지수 백오프 재시도 & 실시간 UI 안내**: API 호출 중 429(Rate Limit / Quota Exceeded) 발생 시 `Please retry in ~ms` 또는 지수 백오프(1.5초, 3.0초, 6.0초)로 최대 3회 자동 재시도하며, 실시간 안내 배너(⏳) 표시 및 중단 버튼(■) 즉시 취소 지원.
     - **OpenAI / Anthropic / Ollama**: `gpt-4o`, `claude-3-7-sonnet`, `llama3.2` 다형성 Provider 지원.
   - 사이드패널 설정 UI에서 API 키, 모델명, Base URL, 시스템 프롬프트를 자유롭게 구성/저장.

5. **Task Runtime & 간결한 사이드패널 UI (`ui/task_runtime.js`, `ui/sidepanel.*`)**:
   - 복잡한 사용자 요청을 브라우저 세부 동작(`browser_navigate`, `browser_get_page_content`, `browser_click_element`, `browser_type_text`, `browser_scroll`, `browser_autofill_login`)으로 분해 실행.
   - 불필요한 번잡한 버튼 바를 정리하고 프롬프트 입력창 우측의 원클릭 전송/중단(■) 토글 버튼으로 인터랙션을 단일화.
   - 동작 대상 DOM 요소에 반투명 파란색 펄스 링(Highlight Ring)을 렌더링하여 조작 위치를 시각적으로 안내.
   - 오류 발생 시 최대 2회 스크롤/대기 후 재시도하는 자율 복구 루프를 수행하며, 실패 시 `WAITING` 상태로 전환되어 사용자에게 개입 안내 카드 노출.

6. **보안 자격증명 로컬 볼트 (`simple_vault.c`, `simple_vault.h`)**:
   - Windows DPAPI(`CryptProtectData` / `CryptUnprotectData`) 기반으로 `%USERPROFILE%\.lite-browser\vault.dat`에 계정 암호화 보관.
   - **비밀번호 평문(Plaintext) 격리**: AI 엔진/프롬프트에는 비밀번호 텍스트가 절대 전달되지 않으며, C 백엔드가 웹 프레임에 직접 스크립트를 주입하여 로그인 폼을 대리 작성하고 LLM에는 성공/실패 상태값만 반환.

7. **기억 및 벡터 DB 메모리 엔진 (`ui/agent_memory.js`)**:
   - IndexedDB 기반으로 과거 방문 기록, 북마크, 대화 요약본을 n-gram 벡터로 인덱싱하고 코사인 유사도 검색을 통해 관련 컨텍스트를 AI 에이전트에 주입.
   - 설정 UI를 통해 '방문기록 자동 인덱싱 비활성화' 및 '기억 데이터 전체 삭제'를 1클릭으로 실행하는 데이터 제어권 보장.

8. **Rust 브라우저 제어 MCP Server (`mcp_server/`, `simple_mcp.c`, `simple_mcp.h`)**:
   - `mcp_server/` 디렉토리에 표준 JSON-RPC 2.0 MCP 서버 구현 (Cargo 프로젝트).
   - C 백엔드가 `lite_browser_mcp.exe` 프로세스를 관리하며, 바이너리 미존재 시에도 내장 네이티브 브릿지로 도구 호출을 중계하는 Fallback 설계 적용.

### 12.3 주요 소스 파일 맵
- [`ui/content_extractor.js`](file:///c:/projects/lite_browser/ui/content_extractor.js): 5단계 본문 파싱 & 마크다운 추출기 모듈
- [`ui/sidepanel.html`](file:///c:/projects/lite_browser/ui/sidepanel.html): AI 사이드패널 UI 마크업
- [`ui/sidepanel.css`](file:///c:/projects/lite_browser/ui/sidepanel.css): 사이드패널 다크/라이트 테마 및 타임라인 스타일
- [`ui/sidepanel.js`](file:///c:/projects/lite_browser/ui/sidepanel.js): 사이드패널 프론트엔드 오케스트레이터
- [`ui/ai_providers.js`](file:///c:/projects/lite_browser/ui/ai_providers.js): Gemini 3.7 Flash, OpenAI, Claude, Ollama 다형성 Provider
- [`ui/task_runtime.js`](file:///c:/projects/lite_browser/ui/task_runtime.js): 상태 머신 및 DOM 액션 실행 엔진
- [`ui/agent_memory.js`](file:///c:/projects/lite_browser/ui/agent_memory.js): IndexedDB 벡터 메모리 & 데이터 컨트롤
- [`simple_vault.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_vault.c) & [`simple_vault.h`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_vault.h): Windows DPAPI 로컬 보안 볼트
- [`simple_mcp.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_mcp.c) & [`simple_mcp.h`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_mcp.h): MCP Server 프로세스 수명주기 및 브릿지
- [`mcp_server/Cargo.toml`](file:///c:/projects/lite_browser/mcp_server/Cargo.toml) & [`mcp_server/src/main.rs`](file:///c:/projects/lite_browser/mcp_server/src/main.rs): Rust 브라우저 제어 MCP Server

### 12.4 빌드 및 패키징 검증
- Debug 빌드: `Debug/lite_browser.exe` 컴파일 및 링크 성공 (Exit code 0).
- Release 빌드: `Release/lite_browser.exe` 컴파일 및 링크 성공 (Exit code 0).
- NSIS 인스톨러: [`LiteBrowserInstaller.exe`](file:///c:/projects/lite_browser/LiteBrowserInstaller.exe) 패키징 완료 (Exit code 0).

---

## 13. 결제/인증 팝업창 및 링크 새 탭 분기 제어 (Popup Window vs New Tab Routing)

### 13.1 개요
네이버페이, 토스, PG 결제창, 소셜 로그인(OAuth) 등 웹페이지 스크립트가 명시적인 팝업 창(`window.open` 규격/치수 지정)을 요청할 때와 사용자가 일반 링크를 새 탭으로 열 때를 정확히 분기하여, 결제/인증 팝업은 부모 창(`window.opener`)과의 상호작용이 유지되는 독립 네이티브 팝업 창으로 띄우고 일반 링크는 LiteBrowser의 새 탭으로 열리도록 라우팅 로직을 고도화했습니다.

### 13.2 주요 구현 내역
1. **명시적 팝업 윈도우 판별 엔진 ([`simple_life_span_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_life_span_handler.c))**:
   - `life_span_handler_on_before_popup` 콜백에서 `target_disposition` (`CEF_WOD_NEW_POPUP`, `CEF_WOD_NEW_PICTURE_IN_PICTURE`) 및 `popupFeatures` (`isPopup`, `widthSet`, `heightSet`)를 종합 평가하여 실제 팝업 창 요청인지 검사.
   - **팝업 창인 경우**: 전용 클라이언트 핸들러(`BROWSER_TYPE_POPUP`)를 할당하고 `return 0`(false)을 반환하여 CEF가 표준 OS 윈도우 프레임 및 원래 요청된 가로/세로 크기를 가진 독립 팝업 윈도우를 생성하도록 허용 (`window.opener` 통신 및 `window.close()` 정상 동작 보장).
   - **일반 링크/새 탭인 경우**: `target_url`을 추출하여 LiteBrowser 메인 창의 새 탭(`CreateNewTab(win_ctx, target_url_str)`)으로 개설하고 `return 1`을 반환하여 불필요한 크로미움 기본 새 창 생성을 차단.
2. **독립 팝업 브라우저 생명주기 분리 ([`simple_handler.h`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.h), [`simple_display_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_display_handler.c), [`simple_load_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_load_handler.c))**:
   - `browser_type_t` 열거형에 `BROWSER_TYPE_POPUP`을 신설.
   - `on_after_created` 및 `on_before_close`에서 팝업 브라우저가 생성/종료될 때 메인 윈도우의 탭 배열(`win_ctx->tabs`) 슬롯을 덮어쓰거나 오염시키지 않도록 격리.
   - 팝업 브라우저의 타이틀/URL 변경이나 로딩 상태 변경 시 메인 윈도우의 탭 바 및 주소 표시창이 불필요하게 갱신되거나 활성 탭이 숨겨지는 부작용을 방지.
3. **전용 네이티브 팝업 윈도우 호스트 (`LiteBrowserPopupWnd` & `LiteBrowserPopupWndProc`)**:
   - CEF 기본 크로미움 브라우저 UI(탭바/주소창)가 뜨는 것을 원천 방지하기 위해, `on_before_popup` 콜백 내에서 직접 Win32 독립 윈도우 클래스(`LiteBrowserPopupWnd`)를 등록 및 생성(`CreateWindowExW`).
   - **웹페이지 요청 규격 완벽 반영 & DPI/타이틀바 외곽 자동 보정**: 웹페이지가 `window.open` 스크립트로 요청한 가로/세로 크기(`popupFeatures->width`, `popupFeatures->height`)를 웹 콘텐츠의 순수 Client 영역 크기로 인식하고, Windows OS의 `AdjustWindowRectExForDpi`를 통해 타이틀바와 테두리 두께만큼 외곽 윈도우 프레임을 추가 확보하여 웹페이지가 의도한 크기 그대로 1픽셀도 잘리지 않고 표시되도록 구현 (결제 인증 키패드 최소 480x750px 보장).
   - **JavaScript 동적 크기 변경 지원 (`on_contents_bounds_change`)**: 웹페이지 스크립트가 실행 도중 `window.resizeTo()` 또는 `window.resizeBy()`를 호출하여 창 크기를 동적으로 변경할 때 네이티브 팝업 윈도우가 실시간으로 확대/축소되도록 `on_contents_bounds_change` 콜백 구현.
   - 작업 표시줄(Taskbar) 영역을 제외한 모니터 작업 영역(`rcWork`)의 정중앙에 배치하고 화면 경계 밖으로 벗어나지 않도록 클램핑.
   - `windowInfo`에 새로 생성한 `popup_hwnd`를 `parent_window`로 지정(`WS_CHILD`)하여, CEF가 불필요한 크로미움 브라우저 윈도우 대신 컴팩트한 전용 팝업 창 내부에 웹 콘텐츠를 호스팅하도록 바인딩.
   - `WM_SIZE`로 팝업 창 리사이즈에 반응하고, `on_title_change`에서 `SetWindowTextW`를 통해 한글 깨짐 없이 `[웹페이지 제목] - Lite Browser` 형식으로 타이틀바 자동 동기화 및 브라우저 아이콘 주입.
   - **팝업 비동기 종료 격리 및 안전 파괴 (`WM_USER_CLOSE_POPUP`)**: 팝업 창이 닫힐 때 `on_before_close` 콜백 내부에서 `DestroyWindow`를 동기 호출하지 않고 비동기 메시지(`WM_USER_CLOSE_POPUP`)로 분리 전송하여, CEF의 내부 윈도우 객체 정리 루틴과 Win32 윈도우 파괴 순서 충돌(`this->window_ == nullptr` 액세스 위반)을 완벽 차단. 또한 `life_span_handler_do_close`에서 `return 1`을 반환하여 메인 윈도우로의 `WM_CLOSE` 전파를 원천 방지.

### 13.3 팝업창 종료 시 강제 종료/크래시 원인 분석 및 해결 요약

| 문제 지점 | 기존 원인 | 해결 코드 및 아키텍처 개선 |
| :--- | :--- | :--- |
| **1. 메인 창 `WM_CLOSE` 전파** | 팝업 생성 시 `hWndParent`로 메인 창을 지정하여 CEF `do_close` 기본 동작(`return 0`)에 의해 메인 창으로 `WM_CLOSE`가 자동 전달됨 | 팝업을 부모 없는 독립 최상위 윈도우(`hWndParent = NULL`)로 생성하고, `life_span_handler_do_close`에서 `return 1`을 반환하여 메시지 전파 차단 |
| **2. 브라우저 중복 해제 (댕글링 포인터)** | `on_after_created`와 `on_before_close`에서 콜백 인자로 전달된 브라우저를 임의로 `release`하거나 윈도우 소멸(`WM_NCDESTROY`) 시 이미 파괴된 브라우저 객체에 접근 | 팝업 컨텍스트(`pctx->browser`)를 Weak Reference로 전환하고 불필요한 `add_ref`/`release`를 완전히 제거하여 참조 카운트 1:1 보장 |
| **3. CEF 내부 객체 파괴 충돌 (`nullptr` 위반)** | `on_before_close` 콜백 내부에서 동기식 `DestroyWindow`를 호출하여 CEF가 미처 윈도우 객체를 정리하기 전에 Win32 윈도우가 파괴됨 | `PostMessage(parent_wnd, WM_USER_CLOSE_POPUP, 0, 0)` 비동기 메시지로 분리하여 CEF 정리 루틴이 완전히 끝난 후 안전하게 Win32 윈도우가 파괴되도록 보장 |

