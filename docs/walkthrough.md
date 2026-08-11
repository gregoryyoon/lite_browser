# Lite Browser 전체 기능 & 시스템 구현 보고서 (Walkthrough)

본 문서는 **Lite Browser** 프로젝트의 전체 아키텍처 및 주요 기능별 구현 내역(기본 언어 설정, 다중 탭 및 윈도우 관리, 차세대 북마크 & 지능형 주소창, 커스텀 아이콘 리소스 자동화 파이프라인, NSIS 인스톨러 패키징)을 통합하여 관리하는 전체 통합 기술 가이드입니다.

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
- **대시보드 정렬 알고리즘**: 전체 북마크 목록을 1순위(최근 30일간 다빈도 방문) ➔ 2순위(최근 추가된 북마크 생성일) 순으로 정렬하여 그리드/리스트 뷰에 우선 렌더링.
- **`✨ 하이라이트 앵커` 카드 UI 뱃지**: 본문 영역을 마우스 드래그한 문장이 함께 저장된 북마크 카드 상단에 황금색 뱃지 박스(`card-highlight-box`)를 노출하여 시각적 직관성 강화.

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
# 1. CMake Release 빌드
cmake --build c:\projects\lite_browser\cef_binary_149.0.6\build --config Release --target cefsimple_capi

# 2. NSIS 인스톨러 생성
& "C:\Program Files (x86)\NSIS\makensis.exe" c:\projects\lite_browser\installer.nsi
```

---

## 6. 주요 소스 파일 맵 (File Directory Map)

- [`simple_app.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_app.c): Win32 메인 프로시저, `WM_GETMINMAXINFO` 보정, DPI 스케일링, 디폴트 URL
- [`simple_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.c): `RemoveTabAt` 탭 삭제, `CreateNewTab` 상대 위치 삽입, `ResolveUIFilePath` 동적 경로 탐색, 컨텍스트 메뉴
- [`simple_life_span_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_life_span_handler.c): 팝업 가로채기 리디렉션, 비동기 소멸 UAF 가드
- [`simple_display_handler.c`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_display_handler.c): 주소 변경, 타이틀 변경 이벤트 동기화
- [`browser_context.h`](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/browser_context.h): 동적 윈도우/탭 컨텍스트 구조체 정의
- [`ui/app.js`](file:///c:/projects/lite_browser/ui/app.js): 주소창 포커스/blur, Omnibox 통합 그룹화 검색 엔진, History 자동 트래킹
- [`ui/index.html`](file:///c:/projects/lite_browser/ui/index.html) & [`ui/style.css`](file:///c:/projects/lite_browser/ui/style.css): 상단 주소창/탭바 네이티브 UI 레이아웃
- [`ui/manager.html`](file:///c:/projects/lite_browser/ui/manager.html) & [`ui/manager.js`](file:///c:/projects/lite_browser/ui/manager.js): `lite://favorites` 대시보드
- [`installer.nsi`](file:///c:/projects/lite_browser/installer.nsi): NSIS 설치 파일 빌드 스크립트
