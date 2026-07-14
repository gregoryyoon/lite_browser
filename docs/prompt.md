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

# 브라우저 기능 관련 추가 확장 기능 (다중 탭, 드래그앤드롭 새 창 분리 및 메모리 안정화)

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

### 5. Chrome 스타일 메뉴 시스템 및 윈도우 잘림/먹통 해결
- **3점 드롭다운 네이티브 메뉴 연동**: 주소창 UI 브라우저(HWND, 높이 100px) 내부의 3점 버튼을 누를 때 HTML 상에서 드롭다운이 하단 콘텐츠 브라우저 HWND 영역에 가려 보이지 않던 문제를 근본적으로 해결했습니다. 버튼 클릭 시 뷰포트 절대 좌표(x, y)를 C API 백엔드로 전달(`http://ui-action/show-menu?x=...&y=...`)하고, 백엔드에서 `ClientToScreen`을 거쳐 Win32 **`TrackPopupMenu`**를 띄워 가려짐 없는 완벽한 네이티브 드롭다운 메뉴를 오버레이했습니다.
- **컨텍스트 메뉴 크래시 및 포커스 먹통 우회**: 마우스 우클릭 시 호출되는 CEF C API `run_context_menu` 콜백 연결 시 오프셋 및 Calling Convention 미스매치로 발생하던 접근 위반(Access Violation) 크래시와 마우스 캡처 락에 따른 브라우저 전체 UI 먹통(마비) 현상을 완전히 차단했습니다. 이를 우회하기 위해 우클릭 즉시 최초 호출되는 `context_menu_on_before_context_menu` 내부에서 CEF 기본 메뉴 모델을 비우는 동시에(`model->clear`), 동기식 Win32 `TrackPopupMenu`를 직접 팝업하여 명령(뒤로 가기, 앞으로 가기, 새로고침, 인쇄, 소스 보기, 검사)을 안정적으로 실행하도록 구성했습니다.
- **안전한 전체 종료 루틴 구현**: 메뉴의 '종료' 선택 시, 개별 핸들러 인스턴스를 소유한 하단 탭 콘텐츠 브라우저들이 유실되거나 상단 주소창만 닫히는 현상을 해결했습니다. 종료 이벤트 감지 즉시 상위 메인 윈도우(`win_ctx->main_hwnd`)에 Win32 **`WM_CLOSE`** 메시지를 전송하여 모든 브라우저 및 메인/자식 리소스들이 표준 소멸 파이프라인을 타고 크래시 없이 동시 정리되며 프로세스가 완전 종료되도록 처리했습니다.

### 6. 커스텀 타이틀바(Frameless Window) 및 창 드래그 이동 구현
- **타이틀바 제거 및 그림자 복원**: `simple_app.c` 의 `CreateWindowEx` 호출 스타일을 `WS_POPUP | WS_THICKFRAME` 등의 프레임리스 구조로 교체하여 네이티브 캡션 표시줄을 완전히 소거했습니다. `WS_THICKFRAME`을 탑재해 테두리를 통한 창 크기 조절(Resize)을 정상 보장하고, `DwmExtendFrameIntoClientArea` API를 호출하여 Windows 10/11 전용 둥근 모서리 및 외부 그림자 효과를 입혔습니다.
- **탭바 드래그 이동 (Drag-to-Move)**: 자식 CEF 브라우저가 포인터 입력을 독차지하여 부모 윈도우의 드래그가 차단되는 현상을 해결하기 위해, HTML 탭바 빈 구역 `mousedown` 시 `drag-window` IPC 액션을 백엔드로 발송합니다. C 백엔드 수신부에서 **`ReleaseCapture()`**를 호출하여 마우스 캡처를 일시 해제하고 메인 창에 **`SendMessage(win_ctx->main_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0)`**을 동기 전송함으로써 마우스 드래그를 따라 창이 완벽하게 끌려 이동하도록 처리했습니다.

### 7. HTML 윈도우 제어 버튼 및 실시간 최대화 아이콘 동기화
- **윈도우 제어 버튼 탑재**: 상단 탭바 우측 끝에 최소화, 최대화(복원), 닫기 버튼(`.window-controls`)을 배치하고, 닫기 버튼 오버 시 전형적인 윈도우 단추 스타일인 적색 배경 피드백 스타일을 바인딩했습니다.
- **실시간 최대화 상태 피드백**: 사용자가 최대화 버튼을 직접 누르거나, 창 여백을 더블 클릭하고 Windows 창 스냅 단축키(Win + ↑, Win + ↓)를 사용해 최대화 상태를 바꿀 때 실시간으로 아이콘을 동기화하기 위해, `WM_SIZE` 메시지 수신부에서 `IsZoomed(hwnd)` 상태값을 읽어 UI 브라우저로 `window.updateMaximizeState` 자바스크립트를 전송합니다. JS에서는 이 상태를 받아 메모장/탐색기와 같은 **최대화(사각형 하나 `ㅁ`)** 또는 **이전 크기로 복원(겹친 두 개의 사각형)** Windows 표준 아이콘과 툴팁을 실시간 교체 렌더링합니다.

### 8. 새 탭 단추 우측 밀착 정렬 및 닫기 버튼 버블링 차단
- **새 탭 단추 우측 밀착**: 새 탭 추가(`+`) 버튼이 열려 있는 마지막 탭 우측에 자연스럽게 동행하도록 flex 레이아웃 구조를 리팩토링했습니다. 탭 컨테이너의 가로 확장 방식(`flex: 1`) 대신 **`flex: 0 1 auto`**로 탭 개수만큼 수축하도록 수정하고, 그 뒤에 남는 여백 공간을 채우며 창 드래그 영역 기능도 함께 하는 **`.tabs-spacer` (flex: 1)** 영역을 덧대어 반응형 정렬 미학을 구현했습니다.
- **닫기 단추 이벤트 버블링 차단**: 탭의 개별 닫기 버튼(`.tab-close`, `&times;`)을 누를 때 포인터 이벤트가 부모인 탭 엘리먼트(`.tab`)로 그대로 버블링되어 마우스 포인터 캡처 루프에 잡히거나 탭 전환/드래그가 일어나는 오작동을 해결하기 위해, `x` 버튼의 `pointerdown` 이벤트 발생 즉시 **`e.stopPropagation()` (이벤트 버블링 차단)**을 걸어 닫기 기능만 정확히 단독 트리거되도록 예외 격리 조치를 적용했습니다.



# MD파일 에디터 기능 구현 완료

LLM 서비스(Gemini, ChatGPT, Claude 등)의 입력창에 에디터에서 작성한 Markdown 프롬프트 콘텐츠를 버튼 하나로 자동 주입하기 위해, 순수 Win32 C CAPI 아키텍처 및 HTML5 기술을 결합하여 고품질의 MD 파일 에디터 및 분할 레이아웃 시스템을 완벽하게 구현했습니다.

### 1. MD 에디터 브라우저 및 기동 생명 주기
- **비가시 상태 프리로딩(Pre-loading)**: 에디터 기동 시의 네트워크 딜레이(TOAST UI Editor CDN js 로딩 지연 등) 및 화면 깜빡임을 방지하여 네이티브 앱 수준의 스냅 UX를 보장하고자, 메인 윈도우 생성(`create_browser_window`) 및 새 창 팝업(`create_browser_window_for_detached`) 시점에 MD 에디터 브라우저(`editor_browser`)를 비가시 상태(`SW_HIDE`)로 백그라운드에 미리 만들어 둡니다.
- **초기 오버랩(잔상) 방지**: 기동 시 에디터 브라우저의 초기 bounds를 `(0, 0, 0, 0)`으로 초기화하고, `life_span_handler_on_after_created` 시점에 `ShowWindow(hwnd, SW_HIDE)`를 강제 호출하여 렌더링 동기화 중 발생할 수 있는 미세한 화면 겹침 현상을 원천 방지했습니다.

### 2. 가로 분할(50/50 Split) 화면 및 토글 처리
- **가로 분할 레이아웃**: `LiteBrowserMainWndProc`의 `WM_SIZE` 프로시저 내에서 `show_editor` 플래그가 활성화되어 있고 에디터 HWND가 존재할 때, 하단 클라이언트 영역을 정밀하게 50%씩 가로로 분할 배분하여 좌측은 콘텐츠 브라우저, 우측은 에디터 브라우저를 배치합니다.
- **비활성 상태 격리**: 에디터 비활성화 상태(`show_editor` 가 거짓)일 때는 `WM_SIZE` 계산 루프에서 에디터 HWND 크기를 즉시 `(0, 0, 0, 0)`으로 축소하고 `SW_HIDE` 처리하여 웹 화면 렌더링에 간섭을 미치지 못하도록 완벽하게 격리했습니다.
- **다중 인터셉트 레이아웃 동기화**: 탭 전환(`switch-tab`), 탭 삭제(`close-tab`), 탭 분리(`detach-tab`) 시 하드코딩된 위치 대신 `PostMessage(main_hwnd, WM_SIZE)`를 전송하여 화면 비율 동기화 로직을 일원화했습니다.
- **토글 경로 다각화**: 주소창 UI 버튼(`.nav-btn` 형태의 마크다운 아이콘), 페이지 우클릭 컨텍스트 메뉴(`1007` 커맨드), 상단 3점 드롭다운 네이티브 메뉴 등을 통해 에디터를 유연하게 여닫을 수 있습니다.

### 3. TOAST UI Editor 연동 및 파일 관리 제어판 (`ui/editor.html`)
- **TOAST UI Editor 임베딩**: 외부 CDN으로부터 공식 `toastui-editor-all.min.js` 및 `toastui-editor.min.css` 라이브러리를 안전하게 로드하고, 로컬 DOM 컨테이너(`div#editor-container`) 내에 직접 에디터 인스턴스를 동적으로 생성하여 렌더링합니다.
- **WYSIWYG 모드 기본 구동**: 기동 시 사용자가 시각적으로 편리하게 포맷팅할 수 있도록 WYSIWYG 모드를 디폴트 에디터 형식(`initialEditType: 'wysiwyg'`)으로 로드합니다.
- **커스텀 파일 관리 단추**: 에디터 툴바의 가장 첫 번째 항목에 폴더 모양(Folder SVG)의 커스텀 버튼을 구성하고 마우스 오버 시 **"파일 관리"** 툴팁을 표시합니다.
- **제어판 토글 연동**: 에디터 편집 화면 내 불필요한 시야 가림을 최소화하기 위해, 평소에는 Glassmorphism 스타일의 하단 파일 제어 카드(Floating Card)를 완전히 숨겨두고(`display: none`), 툴바의 **"파일 관리"** 단추를 누를 때마다 화면 하단 중앙에 부드럽게 토글(`display: flex`)되도록 설계했습니다.
- **제어 기능 연동**:
  - **새 파일**: 에디터 초기화 및 새 문서 작성 준비. `editor.setMarkdown(DEFAULT_TEMPLATE)`를 사용해 즉각적으로 편집기를 리셋합니다.
  - **저장**: UTF-8 및 한글 깨짐 방지를 위해 마크다운 텍스트를 base64 마샬링하여 C 백엔드(`editor-save-file?name=...&content=...`)로 전송, `C:\projects\lite_browser\documents` 특정 지정 폴더에 안전하게 저장합니다.
  - **불러오기**: 실시간으로 백엔드의 documents 폴더 내 `*.md` 파일 목록을 조회(`editor-list-files`)하여 제어판의 드롭다운 선택 상자에 실시간 갱신하고, 파일을 선택하면 그 즉시 데이터를 백엔드에서 읽어와(`editor-load-file`) `editor.setMarkdown(...)`으로 에디터 화면에 동적으로 주입합니다.

### 4. 로컬 웹 보안 제약의 유연성
- TOAST UI Editor는 로컬 윈도우 컨텍스트 내에서 직접 작동하므로 과거 iframe(StackEdit) 사용 시 발생하던 교차 도메인 CORS 및 `postMessage` 보안 통신 차단 우려가 근본적으로 존재하지 않습니다. 다만 다른 백엔드 주입 편의성을 위해 기존 추가된 `--disable-web-security` 및 `--allow-file-access-from-files` 기동 플래그는 안정적으로 보존됩니다.

### 5. LLM 서비스 프롬프트 원클릭 주입 시스템
- 작성된 에디터의 마크다운 콘텐츠를 base64 인코딩하여 백엔드의 `send-prompt` 명령으로 발송합니다.
- C 백엔드는 전달받은 프롬프트 데이터를 복원한 뒤, 현재 활성화된 좌측 콘텐츠 브라우저의 메인 프레임에 자바스크립트를 주입(`execute_java_script`)합니다.
- **범용 LLM 타겟팅 인젝터**: ChatGPT(`#prompt-textarea`), Claude & Gemini(`div[contenteditable="true"]` 또는 `[contenteditable="true"]`), 그리고 일반 웹 표준 `textarea` 및 `input`을 포괄적으로 자동 스캔하여 텍스트를 채우고, 웹앱 상태 갱신을 위해 브라우저 가상 이벤트(`input` 및 `change` 이벤트 버블링)를 트리거하여 완벽하게 프롬프트가 주입 및 전송 대기 상태에 도달하도록 구현했습니다.

# 창 위치/크기 영속 복원 및 작업 표시줄 최적화

### 1. 사용자 홈 디렉토리 영속화
- **경로 정의**: `SHGetSpecialFolderPathA` (CSIDL_PROFILE)를 사용하여 사용자 프로필 경로(`C:\Users\<Username>`)를 획득하고, 그 하위에 `.lite-browser` 폴더를 자동 생성하여 `window_config.txt` 텍스트 포맷 설정 파일로 창 좌표를 영속화합니다.
- **예외 복원**: 윈도우가 최소화 상태에서 종료되더라도 다음 기동 시 최소화로 고착되는 것을 막고자 최소화 감지 시 일반 화면(`SW_SHOWNORMAL`) 복원 코드를 구현했습니다.

### 2. 잔상 없는 스냅 기동 (Flicker-free Startup)
- `CreateWindowEx` 시점에 창 스타일에서 `WS_VISIBLE`을 제외하여 숨김 창으로 띄우고, 저장된 이전 WINDOWPLACEMENT 복원 처리를 마친 후에 `UpdateWindow`로 스냅 노출하여 기동 즉시 크기가 뒤바뀌며 발생할 수 있는 레이아웃 번쩍임 현상을 완전 차단했습니다. 설정 파일이 없는 최초 1회 기동 시에는 전체 화면 최대화(`SW_SHOWMAXIMIZED`)로 실행됩니다.

### 3. 최대화 시 작업 표시줄 보호 및 자동 숨기기(Auto-hide Taskbar) 팝업 보장
- **`WM_GETMINMAXINFO` 메시지 하이재킹**: 프레임리스(`WS_POPUP`) 창 최대화 시 작업 표시줄이 뒤로 가려지거나, 반대로 자동 숨기기 모드에서 마우스를 가져다 대도 작업 표시줄이 위로 올라오지 않는 프레임리스의 한계를 극복했습니다.
- **자동 숨기기 대응**: `SHAppBarMessage(ABM_GETSTATE)`로 자동 숨기기(`ABS_AUTOHIDE`) 여부를 실시간 파악하여, 자동 숨기기가 켜져 있으면 `ABM_GETTASKBARPOS`로 작업 표시줄의 숨김 방향(Edge)을 알아낸 뒤, 해당 경계 좌표를 딱 **1픽셀 차감**하여 최대화 크기(`ptMaxSize`/`ptMaxPosition`)를 세팅합니다. 이 미세한 1px 핫존 개방 덕분에 OS 셸이 마우스 호버 이벤트를 차단하지 않고 작업 표시줄을 안정적으로 팝업시킵니다. 자동 숨기기가 꺼져 있을 때는 일반 작업 영역(`rcWork`)으로 창 크기를 제한하여 작업 표시줄을 가리지 않게 작동합니다.
