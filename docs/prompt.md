# 빌드 및 실행 환경
- **실행 환경**: 64-bit Windows, CEF binary distribution 149.0.6
- **빌드 환경**: Visual Studio 2022/2026 IDE (CMake 사용)
- **빌드 결과물 디렉토리**: `C:\projects\lite_browser\cef_binary_149.0.6\build`

# cefsimple_capi 구조 분석 및 C API 제약 사항
- `cefsimple_capi`는 C++ 스마트 포인터와 클래스 대신 **순수 C 언어로 구현된 CEF 예제**입니다.
- **순수 C API 제약**: C++ 문법은 절대 금지되며, CEF의 함수 호출이나 참조 관리는 수동 매핑(`ref_count` 관리 및 함수 포인터 필드 호출) 방식으로 처리해야 합니다.

# 구현된 하이브리드 브라우저 아키텍처 (순수 Win32 + 이중 자식 브라우저)
CEF Views 프레임워크의 다중 브라우저 뷰 바인딩 한계와 초기 화면 렌더링 누락 문제를 완벽하게 회피하기 위해, 순수 Win32 메인 창 구조 아래 두 개의 네이티브 자식 브라우저를 임베딩하는 방식으로 설계 및 검증이 완료되었습니다.

### 1. 윈도우 관리 및 레이아웃
- **CEF Views 비활성화**: `simple_app.c`에서 `use_views = 0`으로 고정하여 항상 네이티브 Win32 분기가 실행되도록 합니다.
- **메인 윈도우 생성**: `simple_app.c`에서 커스텀 윈도우 클래스(`LiteBrowserMainWindowClass`)를 등록하고, `CreateWindowEx`를 호출하여 상위 메인 윈도우(`g_main_hwnd`)를 띄웁니다.
- **이중 자식 브라우저 임베딩**: 메인 윈도우 생성 직후 두 개의 자식 브라우저를 독립적으로 생성합니다.
  - **상단 주소창 UI 브라우저**: 로컬 HTML 주소창(`ui/index.html`)을 로드하며, `{0, 0, width, 80}` 크기의 자식 창(`WS_CHILD`)으로 생성됩니다.
  - **하단 웹 콘텐츠 브라우저**: 메인 웹페이지(`g_startup_url`)를 로드하며, `{0, 80, width, height - 80}` 크기의 자식 창(`WS_CHILD`)으로 생성됩니다.

### 2. 윈도우 메시지 프로시저 (`LiteBrowserMainWndProc`)
- **`WM_SIZE`**: 상단 주소창 브라우저(높이 80 고정)와 하단 콘텐츠 브라우저(나머지 영역)의 윈도우 핸들을 획득하여 Win32 `MoveWindow` API로 크기를 실시간 조정하고 다시 그립니다.
- **`WM_CLOSE` / `WM_DESTROY`**: 윈도우 종료 신호 시 자식 창들과 브라우저 리소스를 해제하고, 메시지 루프를 안전하게 이탈(`cef_quit_message_loop()`)시킵니다.

### 3. 주요 연동 제어 및 이벤트 처리
- **비동기 생성 즉시 렌더링**: 자식 브라우저 생성이 비동기로 완료되는 시점(`simple_life_span_handler.c`의 `life_span_handler_on_after_created`)에 캐시된 브라우저 포인터(`g_ui_browser`, `g_content_browser`)를 통해 메인 윈도우에 `WM_SIZE` 메시지를 강제로 전송(`PostMessage`)합니다. 이를 통해 기동 즉시 주소창이 깔끔하게 렌더링됩니다.
- **주소창 입력 연동**: 주소창의 입력 폼에서 `http://ui-action/load?url=...` 프로토콜로 요청이 오면, `on_before_browse` 콜백에서 파싱하여 하단 콘텐츠 브라우저(`g_content_browser`)에 해당 URL을 `load_url` 합니다.
- **타이틀 바 동기화**: 콘텐츠 브라우저 로딩 중 타이틀 변경 이벤트(`display_handler_on_title_change` -> `simple_handler_platform_title_change`)가 오면, 상위 윈도우인 `g_main_hwnd`에 `SetWindowTextW`를 호출하여 실시간 동기화합니다.
- **C API 메모리 참조 관리 (중요)**: `cef_browser_view_get_for_browser`와 같은 CEF 전역 함수에 포인터를 인자로 전달할 경우 API 마샬링에 의해 소유권이 회수되므로, 전달 전 반드시 `browser->base.add_ref`를 호출하여 레퍼런스 카운트 붕괴(Access Violation)를 막아야 합니다.

# 추가 확장 기능 (다중 탭, 드래그앤드롭 새 창 분리 및 메모리 안정화)

### 1. 다중 탭(Multi-Tab) 및 멀티 윈도우 동적 컨텍스트
- **동적 창 컨텍스트 (`browser_window_t`)**: 싱글 윈도우 전역 변수를 완전히 걷어내고, 창 생성 시마다 독립적인 `win_ctx` 구조체를 동적 할당하여 Win32 `GWLP_USERDATA` 및 전역 추적 배열(`g_windows`)에 바인딩했습니다.
- **탭별 리소스 관리**: 윈도우마다 `tabs` 배열(최대 10개) 및 `active_tab_index`, `tab_count`를 갖추어 독립적인 탭 목록과 URL/Title 상태 동기화를 보장합니다.

### 2. 비동기 탭 분리(Reparenting) 및 드래그앤드롭 새 창 생성
- **탭 Reparenting**: 기존 활성화된 자식 콘텐츠 브라우저 창의 부모 윈도우를 `SetParent` Win32 API로 신규 메인 윈도우에 동적 연결하고, 스타일 비트 수정(`WS_POPUP` 해제, `WS_CHILD` 적용) 및 `MoveWindow`를 호출하여 리프레시 없는 완벽한 이관을 처리합니다.
- **PointerEvent 캡처 기법 우회**: 외부 바탕화면으로 드래그 시 마우스 커서가 OS 제한으로 드롭 금지(🚫) 모양으로 바뀌는 한계를 해결하기 위해, HTML5 DnD API 대신 **`setPointerCapture`**를 채택했습니다. 탭 바 이탈 시 복사/추가 커서(`copy` 스타일, + 기호)로 실시간 전환되고, 마우스를 떼는 순간 캡처된 전역 스크린 좌표(`GetCursorPos`)를 파싱하여 해당 드롭 좌표에 오프셋 보정된 새 메인 윈도우가 즉시 팝업되도록 보정했습니다.

### 3. 비동기 수명 주기 및 Use-After-Free 크래시 예방 (중요)
- **비동기 메모리 소멸 위임**: 창 닫기(`WM_DESTROY`) 시점에 `free(win_ctx)`를 성급하게 수행하면, 뒤이어 CEF의 비동기 브라우저 소멸 과정에서 `life_span_handler_on_before_close` 콜백이 이미 해제된 주소를 건드려 세그멘테이션 크래시가 유발됩니다. 이를 방지하고자 `WM_DESTROY`에서는 브라우저만 닫고, **해당 창 내의 UI 브라우저 및 모든 탭 브라우저의 소멸 콜백(`on_before_close`)이 100% 완료되어 활성 브라우저 카운트가 `0`이 된 시점에만 비로소 `free(win_ctx)`가 실행**되도록 라이프사이클을 안전하게 격리했습니다.
- **개별 창 종료 가드**: CEF 메시지 루프 이탈 조건식에 표준 매크로인 **`_WIN32`** 가드를 보정 적용하여, 여러 개의 팝업 창 중 하나를 닫았을 때 전체 프로그램이 오작동으로 다 같이 종료되지 않고 활성 메인 창 카운트(`g_window_count == 0`)가 마지막이 될 때에만 루프가 안전하게 이탈하도록 보장했습니다.

### 4. 세련된 라이트 테마(Light Theme) 및 윈도우 테두리 연동
- **네이티브 오프셋 고정**: 탭 및 주소 검색창의 세로 레이아웃 잘림 현상을 해결하기 위해 네이티브 UI 높이를 **100px**로 일괄 확장 튜닝했습니다.
- **웹 UI 라이트 테마**: [ui/style.css](file:///c:/projects/lite_browser/ui/style.css)를 맑고 현대적인 화이트/라이트 그레이 디자인 테마로 전면 갱신했습니다.
- **하단 브라우저 영역 테두리 연동**: 메인 윈도우 클래스의 배경 브러시를 탭 바 배경색인 **라이트 그레이(RGB 228, 228, 231 / #e4e4e7)**로 단일화하고, 하단 콘텐츠 브라우저 크기를 배치할 때 사방에 1px 마진을 두는 **`x=1, y=101, w-2, h-102`** 공식을 적용해 메인 창 배경이 테마 일체형 테두리 선으로 정교하게 비치도록 연동했습니다.