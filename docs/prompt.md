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