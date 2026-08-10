# Lite Browser 탭 및 윈도우 관리 아키텍처 & 기능 구현 보고서 (Walkthrough)

본 문서는 순수 C API 기반 Win32/CEF 브라우저 `cefsimple_capi` 프로젝트의 **다중 탭(Multi-Tab) 관리**, **다중 윈도우(Multi-Window) 관리**, **탭 드래그앤드롭 분리(Reparenting)**, **팝업 가로채기 및 새 탭 전환**, **메모리 안정성(Use-After-Free 및 UAF 크래시 방지)**, **High DPI 및 듀얼 모니터 최대화 대응**에 이르기까지 지금까지 진행된 전체 탭/윈도우 서브시스템의 개발 내역 및 기술 아키텍처를 총망라하여 정리합니다.

---

## 1. 하이브리드 브라우저 아키텍처 (Win32 + 이중 자식 브라우저)

### 1) CEF Views 우회 및 순수 Win32 윈도우 임베딩
- CEF Views 프레임워크의 다중 브라우저 뷰 바인딩 제약 및 렌더링 누락 문제를 방지하고자 `simple_app.c`에서 `use_views = 0`으로 지정하여 순수 Win32 메시지 프로시저 기반 분기를 기동합니다.
- 메인 윈도우 클래스(`LiteBrowserMainWindowClass`)를 등록한 뒤 `CreateWindowEx`로 최상위 메인 윈도우(`g_main_hwnd`)를 띄우고, 그 하위에 두 개의 독립된 자식 브라우저(`WS_CHILD`)를 임베딩합니다:
  - **상단 주소창 UI 브라우저**: 로컬 HTML/CSS/JS (`ui/index.html`)를 로드하는 컨트롤 브라우저
  - **하단 웹 콘텐츠 브라우저**: 웹 페이지를 렌더링하는 실체 콘텐츠 브라우저

### 2) C API 참조 관리 (Ref-Counting) 규칙
- C++ 스마트 포인터가 없는 순수 C 환경이므로, CEF 전역 API(`cef_browser_view_get_for_browser` 등) 호출 시 포인터 소유권 마샬링으로 인한 레퍼런스 카운트 파손을 막고자 인자 전달 전 `browser->base.add_ref`를 수동 호출하여 메모리 안전성(Access Violation 예외 차단)을 보장합니다.

---

## 2. 동적 컨텍스트 기반 다중 탭(Multi-Tab) 및 멀티 윈도우 관리

### 1) 창별 동적 컨텍스트 구조체 (`browser_window_t`)
- 싱글 윈도우 전역 변수를 제거하고, 윈도우 인스턴스 생성 시마다 `browser_window_t` 구조체를 동적 할당하여 Win32 `GWLP_USERDATA` 및 전역 관리 배열(`g_windows`)에 바인딩했습니다.
- 개별 윈도우마다 `tabs` 배열(최대 10개), `active_tab_index`, `tab_count`를 가지고 있어 탭 목록, URL, Title, HWND 등의 상태를 독립적으로 관리합니다.

### 2) 활성 탭 스위칭 및 Z-order HWND 숨김/노출 제어
- 탭 전환(`switch-tab`)이나 탭 생성/삭제 시, 활성화 대상 탭을 제외한 **나머지 모든 백그라운드 탭 브라우저들의 윈도우 핸들을 명시적으로 `ShowWindow(hwnd, SW_HIDE)` 처리**합니다.
- 선택된 활성 탭만 `ShowWindow(hwnd, SW_SHOW)` 및 `MoveWindow`를 수행하여 전면 윈도우 뒤에 숨겨진 백그라운드 탭들이 겹쳐 보이거나 잔상이 남는 HWND Z-order 포개짐 현상을 완벽 차단했습니다.

### 3) 현재 활성 탭 바로 우측 위치 새 탭 삽입 (Active Tab Relative Insertion)
- 새 탭 생성 시 무조건 전체 탭의 맨 우측 끝에 추가되던 기존 방식을 개편하여, 크롬/엣지 표준 UX와 동일하게 **현재 활성화된 탭의 바로 오른쪽(`active_tab_index + 1`) 위치에 새 탭이 즉시 삽입(Insert)**되도록 `CreateNewTab` 공통 함수 내 탭 배열 시프트(Shift) 알고리즘을 구현했습니다.
- 상단 `+` 버튼, 주소창/탭바/웹페이지 컨텍스트 메뉴, 그리고 웹페이지 내부 링크 클릭(`target="_blank"`, `window.open` 팝업 우회 등) 등 **모든 새 탭 생성 경로에 100% 일괄 적용**됩니다.

---

## 3. 비동기 탭 분리(Reparenting) 및 드래그앤드롭 새 창 생성

### 1) 새로고침 없는 탭 Reparenting
- 활성화된 자식 콘텐츠 브라우저 창의 부모 윈도우를 Win32 `SetParent` API로 신규 메인 윈도우에 동적 연결합니다.
- 스타일 비트(`WS_POPUP` 해제, `WS_CHILD` 적용) 갱신 후 `MoveWindow` 및 `host->was_resized(host)`, `host->set_focus(host, 1)`를 수행하여 기존 웹 세션을 새로고침 없이 실시간 새 창으로 이관합니다.

### 2) PointerEvent 캡처 기법을 통한 이탈 감지 및 커서 동기화
- HTML5 DnD API 사용 시 OS 제한으로 윈도우 외부 드롭 시 포인터가 🚫(드롭 금지)로 고착되는 현상을 해결하고자 **PointerEvent `setPointerCapture`**를 도입했습니다.
- 탭바 이탈 시 포인터 커서를 복사/추가 기호가 결합된 **`copy` 커서**로 실시간 전환하고, 마우스를 놓는 순간 스크린 좌표(`GetCursorPos`)를 백엔드로 넘겨 새 메인 윈도우를 해당 좌표에 스냅 팝업시킵니다.

---

## 4. 비동기 수명 주기 및 메모리 안정성 (Use-After-Free 크래시 방지)

### 1) 비동기 메모리 소멸 위임 (UAF 크래시 차단)
- Win32 `WM_DESTROY` 메시지가 수신되었을 때 `free(win_ctx)`를 성급하게 수행하면, CEF 내부 스레드의 비동기 브라우저 소멸 과정(`life_span_handler_on_before_close`)에서 댕글링 포인터를 참조하여 세그멘테이션 크래시가 유발됩니다.
- 이를 막고자 `WM_DESTROY`에서는 브라우저 closure만 요청하고, **해당 창 내부의 UI 브라우저 및 모든 자식 탭 브라우저들의 소멸 콜백(`on_before_close`)이 100% 완료되어 카운트가 `0`이 되는 최종 시점에 비로소 `free(win_ctx)`가 실행되도록 소멸 라이프사이클을 격리**했습니다.

### 2) 개별 창 종료 표준 가드 (`_WIN32`)
- 플랫폼 빌드 매크로 가드를 표준 **`_WIN32`**로 보정하여, 다중 창 구동 중 개별 팝업/분리 창을 닫았을 때 전체 프로그램이 동시 종료되는 문제를 막고, 메인 창 카운트(`g_window_count == 0`)가 마지막 1개일 때만 CEF 메시지 루프(`cef_quit_message_loop`)가 이탈되도록 보장했습니다.

---

## 5. 커스텀 프레임리스 윈도우, High DPI & 듀얼 모니터 최대화 보정

### 1) 커스텀 타이틀바 및 탭바 마우스 드래그 이동
- `WS_POPUP | WS_THICKFRAME` 프레임리스 구조를 채택하고 `DwmExtendFrameIntoClientArea` 그림자 효과를 입혔습니다.
- HTML 탭바 여백 구역 마우스 다운 시 `drag-window` IPC를 쏘아, 백엔드에서 **`ReleaseCapture()` 후 `SendMessage(win_ctx->main_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0)`**을 동기 호출하여 마우스 드래그로 자연스럽게 창이 이동되도록 구현했습니다.

### 2) High DPI 배율 대응 주소창 동적 높이 스케일링
- `GetDpiForWindow` API로 디스플레이 배율(`scale = DPI / 96.0`)을 동적 구하고, 주소창 표준 논리 높이(`72px`)에 배율을 곱한 물리 크기(`ui_height = (int)(72.0 * scale)`)로 하단 콘텐츠 브라우저 Y 좌표(`ui_height + 1`)를 반응형 산출하여 DPI 변경 시에도 주소창 잘림 현상이 전혀 없도록 일원화했습니다.

### 3) 듀얼/멀티 모니터 `WM_GETMINMAXINFO` 오프셋 상대 좌표 보정
- 프레임리스 창을 보조 모니터(2번째/3번째 모니터)로 이동 후 최대화 클릭 시 윈도우가 가상 스크린 밖(예: 3840px 이격 영역)으로 날아가 화면에서 사라져 보이던 버그를 해결했습니다.
- Win32 `WM_GETMINMAXINFO` 의 `mmi->ptMaxPosition` 은 가상 화면 전역 절대 좌표가 아닌 **"해당 모니터 오리진 기준 상대 좌표(Relative Coordinates)"**를 요구합니다.
- `mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left` 및 `y = mi.rcWork.top - mi.rcMonitor.top` 공식을 적용하여 보조 모니터 위치와 관계없이 항상 해당 디스플레이 내에서 정밀하게 전체화면 최대화가 동작하도록 수정했습니다.
- 작업 표시줄 자동 숨기기(`ABS_AUTOHIDE`) 켜짐 시에도 1px 핫존 차감 연산을 유지하여 마우스 호버 시 작업 표시줄 팝업이 원활하게 작동합니다.

---

## 6. 링크/팝업 가로채기, 비동기 레이스 차단 & UX 고도화

### 1) 네이티브 팝업 차단 후 비동기 리디렉션 (Chromium 네임드 팝업 크래시 근절)
- 네이버 메인 탭(블로그, 카페, 메일 등)처럼 자바스크립트(`window.open(url, "name")`) 기반 네임드 팝업 인입 시, 기존의 `WS_CHILD` 개조 방식은 Chromium 내부 창 이름 매핑 테이블과 Win32 윈도우 소유권 마찰을 일으켜 libcef 스레드 크래시를 발생시켰습니다.
- 이를 해결하기 위해 `on_before_popup` 콜백 내에서 팝업 생성을 **원천 거부(`return 1` 리턴)**하도록 처리하여 Chromium 스레드 마찰을 100% 차단하고, 가로챈 URL을 자체 헬퍼인 **`CreateNewTab(win_ctx, target_url_str)`**로 비동기 넘겨 안정적인 새 탭으로 오픈시켰습니다.

### 2) 비동기 탭-핸들러 일대일 포인터 매칭 (`void* tab_handler`)
- 새 탭 생성 요청 시점과 CEF의 실제 윈도우 실체화(`on_after_created`) 시점 간의 딜레이 도중 탭을 고속 연달아 누를 때 선착순 슬롯 가로채기로 탭 정보가 뒤틀리던 레이스 컨디션을 해결했습니다.
- `tab_info_t`에 `void* tab_handler` 필드를 신설하고, `on_after_created` 호출 시 자신의 parent 핸들러 주소와 일치하는 탭 슬롯을 찾아 일대일 정밀 할당하도록 구현했습니다.

### 3) 백그라운드 로드 버퍼링을 통한 깜빡임(White Flash) 방지
- 새 탭 생성 시 빈 하얀색 윈도우 잔상을 거르고자 기동 시에는 `SW_HIDE`로 숨겨두고, `on_loading_state_change`에서 최초 로딩 시작(`isLoading == 1`) 신호가 떨어지는 즉시 `SW_SHOW`로 전환하여 즉각적인 로딩 반응성과 깜빡임 방지를 동시에 달성했습니다.

### 4) JSON 특수문자 이스케이프 (`EscapeJsonString`)
- 기사 제목이나 URL에 큰따옴표(`"`), 백슬래시(`\`) 등 특수 기호가 포함되면 `update_ui_tabs`의 JSON 문자열이 파괴되어 프론트엔드 `JSON.parse` 자바스크립트 크래시 및 타이틀 '새 탭' 고착 현상이 발생했습니다.
- C++ 백엔드에 `EscapeJsonString` 이스케이프 헬퍼를 자체 구현해 직렬화 전 텍스트를 인코딩함으로써 탭바 UI 렌더링 파싱 에러를 종식시켰습니다.

### 5) 주소 표시창 최초 포커스 자동 선택 UX
- 주소 표시창 미포커스 상태에서 최초 1회 마우스 클릭 시 주소 전체가 자동 선택(`select()`)되도록 `focus`, `mouseup`, `blur` 이벤트를 정밀 조합했습니다. (포커스 획득 후 추가 클릭/드래그 편집 동작은 온전히 유지하는 크롬/엣지 표준 UX).

### 6) 탭바 및 웹 콘텐츠 우클릭 네이티브 컨텍스트 메뉴
- **탭바 우클릭 메뉴**: `show-tab-menu` IPC 수신 시 `TrackPopupMenu`를 통해 **새 탭 / 구분선 / 탭 닫기 / 다른 탭 닫기 / 오른쪽 탭 닫기** 메뉴를 제공하며, 최적화 헬퍼(`CloseTab`, `CloseOtherTabs`, `CloseTabsToRight`)로 안전 처리합니다.
- **웹 콘텐츠 조건부 우클릭 메뉴**: `params->get_link_url()` 조회를 통해 일반 영역 우클릭 시(뒤로/앞으로/새로고침/인쇄/소스보기/검사)와 하이퍼링크 영역 우클릭 시(**새 탭에서 링크 열기**, **링크 페이지 저장** - `start_download`, **링크 복사** - `CF_UNICODETEXT`, **검사**)를 분기 렌더링합니다.

### 7) 탭 삭제/종료 시 활성 탭 전환 및 HWND Z-order 노출 버그 수정 (`RemoveTabAt`)
- **원인 분석**:
  - 기존 `CloseTab`, `detach-tab`, `drag-end` 시 활성 탭(`found_idx`)이 종료되면 탭 배열 시프트 전에 `new_active`가 `found_idx`로 결정되어 이전(좌측) 탭 대신 우측 탭이 활성화되는 문제가 있었습니다.
  - 또한 배열 시프트 **전에** `ShowWindow(tabs[new_active].hwnd, SW_SHOW)`가 호출되어 닫히는 HWND에 보여주기 명령이 들어가고, 정작 시프트 후 새로 활성화된 탭의 HWND는 `SW_HIDE` 상태로 유지되어 브라우저 영역에 회색 바탕만 뜨는 버그가 발생했습니다.
- **해결 방안 (`RemoveTabAt` 공통 헬퍼 구현)**:
  - 탭 삭제/이관 전용 공통 헬퍼 `RemoveTabAt(win_ctx, remove_idx, close_cef_browser)`를 신설했습니다.
  - 활성 탭 삭제 시 이전(좌측) 탭(`remove_idx - 1`)을 활성 인덱스로 지정하고(`remove_idx == 0`일 경우 0번), 탭 배열 시프트 및 `tab_count`, `active_tab_index`를 먼저 갱신합니다.
  - 남은 모든 탭들의 HWND를 루프 순회하여 비활성 탭은 `SW_HIDE`, 새로 활성화된 탭은 `SW_SHOW` 및 `MoveWindow`, `was_resized`, `set_focus`를 호출하여 전환 반응성과 브라우저 렌더링 노출을 100% 보장하도록 처리했습니다.

### 8) 주소창 URL 직접 입력 후 엔터 시 로딩 완료 URL 갱신 버그 수정
- **원인 분석**:
  - 주소창(`addressBar`)에 URL(예: `github.com`) 입력 후 엔터를 눌렀을 때 `handleKey`에서 주소창 포커스 해제(`blur`) 처리가 누락되어 `document.activeElement`가 주소창으로 계속 유지되었습니다.
  - 이로 인해 C 백엔드에서 `on_address_change` 콜백이 실행되어 `window.updateAddress("https://github.com/")`를 호출하더라도, `ui/app.js` 내부의 `if (document.activeElement !== addressBar)` 가드 조건문에 걸려 주소창 텍스트 갱신이 차단되고 입력 텍스트(`github.com`)가 그대로 남아있던 버그가 발생했습니다.
- **해결 방안**:
  - `ui/app.js`의 `handleKey` 함수 내에서 Enter 키 입력 시 `event.target.blur()` 및 `closeOmniboxDropdown()`을 호출하여 포커스를 즉시 해제하도록 보정했습니다.
  - 이에 따라 CEF 페이지 이동 및 로딩 완료 후 `updateAddress`가 차단 없이 정상 작동하여 최종 리디렉션된 전체 URL(`https://github.com/`)로 화면에 즉시 갱신됩니다.

---

## 7. 관련 주요 구성 파일 안내

- [browser_context.h](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/browser_context.h): 동적 창/탭 컨텍스트 구조체 및 `tab_handler` 포인터 정의
- [simple_app.c](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_app.c): Win32 메인 프로시저, `WM_GETMINMAXINFO` 듀얼 모니터 보정 및 DPI 스케일링
- [simple_handler.c](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.c): `CreateNewTab` 상대 위치 삽입, `CloseTab` 계열 헬퍼, `EscapeJsonString`, 네이티브 컨텍스트 메뉴
- [simple_life_span_handler.c](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_life_span_handler.c): `on_before_popup` 리디렉션, `on_after_created` 탭-핸들러 매칭 및 `on_before_close` 수명 주기 제어
- [ui/app.js](file:///c:/projects/lite_browser/ui/app.js): 주소창 포커스 전체 선택, PointerEvent 캡처 탭 드래그, 창 드래그/최대화 상태 동기화
