# HTML5 기반 UI 아키텍처 구현 완료 보고서 (Walkthrough)

본 보고서는 `cefsimple_capi` C 애플리케이션에 HTML5 기반 브라우저 UI 아키텍처를 성공적으로 적용하고 구현을 마무리한 내역을 정리한 문서입니다.

---

## 1. 구현된 주요 변경 사항

### A. 웹 UI 컴포넌트 신규 작성
사용자에게 미려하고 premium한 조작 감성을 선사하기 위해, 다크 아크릴 테마(Glassmorphism)를 채택한 웹 GUI 요소를 추가했습니다.
- [index.html](file:///c:/projects/lite_browser/ui/index.html): 상단 바 영역(뒤로가기, 앞으로가기, 새로고침, 주소창, 로더 애니메이션)의 HTML 마크업.
- [style.css](file:///c:/projects/lite_browser/ui/style.css): 글래스모피즘(backdrop-filter), 인디고 컬러 하이라이트, 호버 트랜지션, 비활성 버튼 투명도 처리 등이 적용된 미려한 CSS 스타일.
- [app.js](file:///c:/projects/lite_browser/ui/app.js): 뒤로/앞으로/새로고침 버튼 클릭 시 C 백엔드로 가상 URL 요청(IPC) 전송, 주소 입력창 엔터 감지(검색 및 자동 포맷팅), C 백엔드로부터 주입되는 전역 상태 동기화 함수 구현.

### B. C 백엔드 (CEF C API) 핵심 로직 수정
- **다중 브라우저 관리**:
  - `simple_handler.h`, `simple_handler.c`에서 `ui_browser`와 `content_browser` 인스턴스 멤버 포인터를 추가하여 상단 UI 영역과 하단 콘텐츠 영역 브라우저를 개별 식별하고 생명주기를 통제합니다.
- **수직 박스 레이아웃 (BoxLayout)**:
  - `simple_views.c`에서 최상위 Views 윈도우에 `cef_box_layout_t`를 수직 배향(vertical)으로 설정했습니다.
  - UI 뷰(`is_ui_view = 1`)는 고정된 높이(80px)의 preferred size를 가지며 flex weight는 `0`입니다.
  - 콘텐츠 뷰(`is_ui_view = 0`)는 남은 전체 영역을 채우도록 flex weight를 `1`로 배치했습니다.
- **웹 UI ↔ C 백엔드 양방향 통신 (IPC)**:
  - **JS → C (요청 인터셉트)**: `simple_handler.c`에 `cef_request_handler_t::on_before_browse` 콜백을 구현해 `http://ui-action/`으로 오는 모든 가상 경로를 인터셉트하여 콘텐츠 브라우저의 API(`go_back`, `go_forward`, `reload`, `load_url`)를 유발하고 페이지 이동은 차단하였습니다.
  - **C → JS (자바스크립트 주입)**: `simple_load_handler.c` (`on_loading_state_change`) 및 `simple_display_handler.c` (`on_address_change`)에서 콘텐츠 브라우저의 로딩/URL 상태 변경을 감지 시 UI 브라우저의 메인 프레임에 `execute_java_script`를 호출하여 UI 컨트롤의 활성화/비활성화 및 주소 표시를 실시간 동기화시킵니다.
- **팝업 및 네이티브 모드 방어 코드**:
  - 팝업 창 생성(`on_popup_browser_view_created`)이나 `--use-native` 실행처럼 UI 뷰가 생성되지 않고 콘텐츠 뷰만 단독 생성되는 예외적인 시나리오에서도 널 포인터 크래시가 나지 않고 정상 동작하도록 안전 장치를 레이아웃 로직에 적용했습니다.
