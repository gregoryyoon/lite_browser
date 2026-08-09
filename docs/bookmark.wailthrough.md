# 🚀 Lite Browser 차세대 북마크 & 지능형 주소창 (Smart Omnibox) UX 개발 완료 보고서

본 문서는 Lite Browser에 구현된 차세대 북마크 시스템, 지능형 주소창(Smart Omnibox) 엔진, `lite://favorites` 관리자 대시보드, 동적 설치 경로 탐색 및 백엔드 C CAPI 아키텍처 연동 내역을 종합 정리한 기술 개발 보고서입니다.

---

## 📌 1. 프로젝트 개요 및 목표 (Overview & Objectives)

### 1.1 기존 북마크 시스템의 문제점 (Pain Points)
1. **과도한 수동 정리 피로감 (Cognitive Load)**: 저장 시 사용자가 폴더를 직접 만들고 분류해야 해서 결국 정리를 포기하는 '북마크 공동묘지' 현상 발생.
2. **저장 당시 맥락(Context) 상실**: 단순 URL과 제목만 저장되어 '내가 이 페이지를 왜 저장했는지', '어떤 키워드로 찾았었는지' 잊어버림.
3. **낮은 가시성 및 재방문율**: 텍스트 리스트 형태의 좁은 드롭다운으로 탐색 유용성이 크게 감소함.

### 1.2 차세대 시스템 혁신 목표
- **수동 정리 Zero화**: 저장 시 본문 요약, OG 썸네일, 유입 검색어, 키워드 태그를 자동으로 추출 및 분류.
- **맥락(Context) 완전 보존**: 유입 검색 Intent, 드래그 텍스트 앵커 및 스크롤 위치 보존.
- **주소창(Omnibox) 탐색 엔드포인트 격상**: 주소창 입력 시 본문 스니펫/태그까지 포함한 실시간 다차원 매칭 및 Hover Popover 미리보기.
- **`lite://favorites` 커스텀 스키마 에코시스템**: 엣지 브라우저의 `edge://favorites` 스타일을 계승한 전용 대시보드 탑재.

---

## 🛠️ 2. 주요 시스템 구성 요소 및 상세 구현

### 2.1 맥락 자동 추출 엔진 (Context Ingestion Engine)
- **파일**: [ui/extractor.js](file:///c:/projects/lite_browser/ui/extractor.js)
- **동작 원리**: 별 버튼(`★`) 클릭 시 C 백엔드가 활성 웹페이지 프레임에 자바스크립트를 주입하여 DOM 데이터를 자동 파싱.
- **수집 항목**:
  - `og:image` 썸네일 이미지 URL.
  - `og:description` 또는 본문 상위 200자 텍스트 요약 스니펫.
  - `document.referrer` 파싱을 통한 **유입 검색어 (Search Intent)** 역추적 (`q=`, `query=`, `search_query=`).
  - 본문 텍스트 단어 빈도 분석(TF-IDF) 기반 **상위 명사 태그 3~5개 자동 정제**.
  - 드래그 선택 텍스트(`window.getSelection()`) 및 스크롤 위치 (`window.scrollY`) **앵커 데이터 수집**.

### 2.2 원클릭 추가/제거 토글 UX & 팝업 억제
- **파일**: [ui/app.js](file:///c:/projects/lite_browser/ui/app.js)
- **토글 동작 (Toggle Delete)**:
  - 북마크되지 않은 페이지에서 별 버튼 클릭(`☆`) → 메타데이터 수집 후 추가(`★`).
  - 북마크된 페이지에서 별 버튼 재클릭(`★`) → 북마크 즉시 제거(`☆`).
- **팝업 억제 (Clean UX)**:
  - 불필요한 저장 완료 모달 팝업을 노출하지 않고, 별 아이콘의 골드 채우기/외곽선 변화만으로 북마크 추가/제거 상태를 직관적으로 제공.

### 2.3 지능형 주소창 엔진 (Smart Omnibox Engine)
- **파일**: [ui/app.js](file:///c:/projects/lite_browser/ui/app.js), [ui/index.html](file:///c:/projects/lite_browser/ui/index.html), [ui/style.css](file:///c:/projects/lite_browser/ui/style.css)
- **다차원 실시간 검색**: 주소창 입력 시 URL, 제목뿐만 아니라 **본문 요약 스니펫, 유입 검색어(`q=`), 추출 태그**까지 통합 탐색.
- **`#` 태그 숏컷**: `#` 입력 시 자동 태그 드롭다운 노출 및 태그별 북마크 필터링.
- **드롭다운 동적 높이 계산 (Dynamic HWND Resizing)**:
  - 렌더링된 드롭다운 DOM 높이(`offsetHeight`)를 실시간 측정하여 필요한 높이만큼만 네이티브 UI 브라우저 HWND(`ui_hwnd`)를 동적으로 확장 (`updateOmniboxHeight`).
  - 결과 항목 수(1개~5개)에 따라 높이가 최적 축소되어 하단 웹 브라우저 화면 가림 현상을 완전 방지.
- **양옆 투명 배경 최적화 (Transparent Side Overlay)**:
  - 확장 영역 중 드롭다운 박스 폭(주소창과 1:1 맞춤) 외 좌/우측 영역의 UI 배경을 `transparent` 처리하여 양옆 웹 브라우저 화면 보존.
- **3행 인라인 카드 레이아웃 & Popover 제거 (Clean Inline UX)**:
  - 기존 우측 팝업 Popover 카드를 제거하고 단일 드롭다운 항목 내 3행 구조로 직관적 정보 통합 노출:
    - **1행**: `🔖 [북마크] {제목}` + `📅 {N일/시간/분 전 저장}` (타임스탬프 자동 계산)
    - **2행**: `🏷️ #태그 목록` + `💡 검색어: "{유입 검색어}"` (수집된 검색 맥락)
    - **3행**: `📄 요약: {본문 스니펫}` (본문 요약)
- **하단 구글 검색 연동**:
  - 드롭다운 최하단에 `🔍 구글 검색: "{입력 키워드}"` 항목 기본 노출 및 키보드(↑/↓)/클릭 통합 탐색 제공.

### 2.4 북마크 전용 관리자 대시보드 (`lite://favorites`)
- **파일**: [ui/manager.html](file:///c:/projects/lite_browser/ui/manager.html), [ui/manager.css](file:///c:/projects/lite_browser/ui/manager.css), [ui/manager.js](file:///c:/projects/lite_browser/ui/manager.js)
- **접속 방식**: 주소창에 `lite://favorites` 입력, 북마크 버튼 클릭, 또는 단축키 `Ctrl+Shift+O` 입력 시 전용 탭 개설.
- **주요 UI 구성**:
  - **Left Sidebar**: Quick Access (전체, 최근 3일, 하이라이트 앵커) 및 스마트 태그별 수량 카운트 리스트.
  - **Top Control Bar**: 통합 키워드/태그 검색창, **Temporal Slider** (날짜 범위 타임라인 필터), **Layout Switcher** (Rich Card Grid ↔ List View).
  - **Main View**: 썸네일, 파비콘, 스니펫, 유입 키워드 뱃지, 태그 칩을 보여주는 카드 그리드 및 테이블.

### 2.5 동적 설치 경로 해결 (Dynamic Path Resolution)
- **파일**: [simple_handler.c](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.c)
- **구현 방식**: `ResolveUIFilePath` 동적 탐색 헬퍼 개발.
  - Windows API `GetModuleFileNameA`를 통해 실행 파일(`.exe`) 경로를 획득.
  - 실행 파일의 상위 폴더 트리를 동적 순회(최대 8단계)하며 `ui/manager.html` 및 `ui/extractor.js`의 실제 위치를 실시간 탐색.
  - **효과**: 개발 환경(`C:\projects\...`), 포터블 환경, 커스텀 설치 디렉토리(`C:\Program Files\LiteBrowser\...`) 등 어떠한 설치 경로에서도 하드코딩 오류 없이 100% 정상 작동.

### 2.6 기본 스타트업 접속 URL 통일 (`lite://favorites`)
- **파일**: [simple_app.c](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_app.c), [simple_handler.c](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.c)
- **변경 사항**: 브라우저 최초 시동, `Ctrl+T` 새 탭 생성, `Ctrl+N` 새 창 생성 시 기본 접속 디폴트 URL을 기존 `https://gemini.google.com/`에서 **`lite://favorites`**로 일괄 변경.

### 2.7 C CAPI 백엔드 영속성 및 IPC 연동
- **파일**: [simple_handler.c](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.c)
- **데이터 저장 경로**: `C:\Users\<Username>\.lite-browser\bookmarks_v2.json`
- **IPC Action 핸들러**:
  - `extract-and-save-bookmark`: 활성 웹 프레임에 `ui/extractor.js` 동적 주입.
  - `save-contextual-bookmark?data=BASE64`: Base64 V2 북마크 수신 및 파일 저장.
  - `open-bookmark-manager`: `lite://favorites` 새 탭 개설.
  - `load-bookmarks-v2`: V2 북마크 데이터 파일 읽기 및 JS In-Memory 전달.

### 2.8 방문 기록(Browsing History) 자동 수집 & 주소창 연동 Engine
- **파일**: [simple_handler.c](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.c), [ui/app.js](file:///c:/projects/lite_browser/ui/app.js), [ui/style.css](file:///c:/projects/lite_browser/ui/style.css)
- **저장 경로**: `C:\Users\<Username>\.lite-browser\history.json`
- **백그라운드 자동 수집 & 중복 타임스탬프 갱신 (Option A)**:
  - 사용자가 웹페이지 탐색 시 URL과 Title을 자동으로 감지하여 `history.json`에 영속화.
  - 동일한 URL 재방문 시 중복 항목을 생성하지 않고 최근 방문 시각(`visitedAt`)과 타이틀만 업데이트(Option A).
- **드롭다운 노출 순서 & 가변 높이 (Option B UI)**:
  - 1순위: 🔖 **[북마크 목록]** (최대 3개)
  - 2순위: 🔍 **[구글 검색]**: `"입력 키워드"`
  - 3순위: 🌐 **[방문 기록 목록]**: `🌐 [방문 기록] 제목 - URL` + `📅 N시간/일 전 방문` (최대 3개)
  - 전체 항목 수에 맞춰 스크롤바(`overflow-y: hidden`) 없이 딱 들어맞게 HWND 동적 확장.

---

## 📂 3. 생성 및 수정된 파일 요약 (File Inventory)

| 구분 | 파일 경로 | 주요 변경 내용 |
| :--- | :--- | :--- |
| **C Backend** | [simple_handler.c](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_handler.c) | `ResolveUIFilePath` 동적 경로 탐색, `lite://favorites` 커스텀 스키마 핸들러, V2 IPC 연동, 디폴트 URL 변경 |
| **C Backend** | [simple_app.c](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/simple_app.c) | `is_ui_expanded` Z-Order 팝업 레이아웃 처리, 스타트업 디폴트 URL `lite://favorites` 설정 |
| **C Backend** | [browser_context.h](file:///c:/projects/lite_browser/cef_binary_149.0.6/tests/cefsimple_capi/browser_context.h) | `browser_window_t` 구조체에 `is_ui_expanded`, `ui_expanded_height` 필드 추가 |
| **UI Main** | [ui/index.html](file:///c:/projects/lite_browser/ui/index.html) | 별 아이콘 버튼, Smart Omnibox 드롭다운 컨테이너 배치 |
| **UI Main** | [ui/style.css](file:///c:/projects/lite_browser/ui/style.css) | Omnibox 드롭다운, Popover 카드, 태그 칩 스타일링 |
| **UI Main** | [ui/app.js](file:///c:/projects/lite_browser/ui/app.js) | 원클릭 토글(추가/제거), Smart Omnibox 실시간 엔진, `lite://favorites` 주소창 표시 |
| **UI Extractor** | [ui/extractor.js](file:///c:/projects/lite_browser/ui/extractor.js) (신규) | DOM 파싱, OG 썸네일/요약, 유입어, TF-IDF 태그, 앵커 자동 수집기 |
| **UI Dashboard**| [ui/manager.html](file:///c:/projects/lite_browser/ui/manager.html) (신규) | `lite://favorites` 대시보드 마크업 구조 |
| **UI Dashboard**| [ui/manager.css](file:///c:/projects/lite_browser/ui/manager.css) (신규) | 대시보드 사이드바, 카드 그리드, 날짜 슬라이더 디자인 시스템 |
| **UI Dashboard**| [ui/manager.js](file:///c:/projects/lite_browser/ui/manager.js) (신규) | 대시보드 필터링, 카드/리스트 전환, 스마트 태그 카운트 인터랙션 |

---

## 🧪 4. 빌드 및 동작 검증 결과

1. **C CAPI 빌드 완료**: `cmake --build C:\projects\lite_browser\cef_binary_149.0.6\build --config Debug` 성공 (0 Error).
2. **원클릭 토글 검증**: 미등록 페이지 별 클릭 시 `★` 추가, 등록 페이지 별 클릭 시 `☆` 제거 확인.
3. **`lite://favorites` 및 경로 동적 해결 검증**: 설치 폴더 위치와 무관하게 `GetModuleFileNameA` 기반 동적 탐색으로 대시보드 정상 접속 확인.
4. **시동 디폴트 URL 검증**: 브라우저 최초 시동, `Ctrl+T`, `Ctrl+N` 실행 시 `lite://favorites` 관리자 대시보드가 디폴트 노출됨 확인.
