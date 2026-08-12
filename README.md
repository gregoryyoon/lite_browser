# 🌐 Lite Browser

<p align="center">
  <img src="cef_binary_149.0.6/tests/cefsimple_capi/win/cefsimple.ico" width="96" height="96" alt="Lite Browser Logo" />
</p>

<p align="center">
  <b>Chromium Embedded Framework (CEF 149.0.6) 순수 C API 기반의 차세대 초경량 하이브리드 웹 브라우저</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2064--bit-blue.svg" alt="Platform Windows" />
  <img src="https://img.shields.io/badge/Language-C11%20%2F%20Win32%20API-00599C.svg" alt="Language C11" />
  <img src="https://img.shields.io/badge/CEF-149.0.6%20C%20API-FF6F00.svg" alt="CEF Version" />
  <img src="https://img.shields.io/badge/UI-HTML5%20%2F%20Vanilla%20CSS%20%2F%20JS-F7DF1E.svg" alt="UI Stack" />
  <img src="https://img.shields.io/badge/Build-CMake%20%2F%20MSVC-064F8C.svg" alt="Build System" />
  <img src="https://img.shields.io/badge/Installer-NSIS%203.x-green.svg" alt="Installer NSIS" />
</p>

---

## 📌 개요 (Overview)

**Lite Browser**는 C++ 스마트 포인터 및 묵직한 프레임워크 클래스에 의존하지 않고, **순수 C 언어(C11)**로 작성된 CEF (Chromium Embedded Framework) C API와 Win32 Native API를 조합하여 극상의 렌더링 성능과 경량화를 달성한 하이브리드 웹 브라우저입니다.

HTML5/CSS3/JavaScript 기반의 현대적 라이트 테마 UI와 순수 Win32 기반의 멀티 브라우저 임베딩 아키텍처를 결합하여, **듀얼 화면 분할(Split-View)**, **실시간 탭 분리(Reparenting)**, **스마트 북마크/오옴니박스 UX** 및 **다운로드 관리자**를 제공합니다.

---

## ✨ 핵심 특징 (Key Features)

### 1. ⚡ Pure C CAPI 기반의 하이브리드 아키텍처
- C++ 클래스나 STL 사용을 배제하고 **순수 C11 C API**만으로 CEF 이벤트 포인터 수동 매핑 및 참조 카운트(`add_ref`/`release`)를 관리합니다.
- `GWLP_USERDATA` 및 구조체 동적 할당을 활용한 멀티 윈도우 지원과 비동기 메모리 소멸 시 Use-After-Free 방지 가드 탑재.

### 2. 🔲 프레임리스 커스텀 타이틀바 & 라이트 테마 UI
- Win32 `WS_POPUP | WS_THICKFRAME`과 Windows DWM (`DwmExtendFrameIntoClientArea`) 그림자 효과를 입힌 프레임리스 창.
- 탭바 공간을 통한 드래그 창 이동(`ReleaseCapture` + `WM_NCLBUTTONDOWN`)과 최대화/복원/최소화/닫기 커스텀 단추 구현.

### 3. ◫ 듀얼 화면 분할 (Dual Split-View)
- 단일 탭 내에서 좌/우 2개의 독립된 웹 브라우저 뷰를 동시 렌더링.
- 6px 호버 마우스 리사이저 바(`0.2f` ~ `0.8f` 비율 동적 조정).
- CEF 포커스 핸들러와 Win32 `WM_MOUSEACTIVATE` 센서를 연동하여 현재 선택된 뷰의 URL/제목/탐색 상태가 단일 주소창과 100% 동기화 (활성 뷰에 **2px 브랜드 블루 테두리** 피드백).
- 우클릭 컨텍스트 메뉴를 통한 "다른 분할 화면에서 열기" 기능 지원.

### 4. 🗂️ 다중 탭 (Multi-Tab) & 드래그 앤 드롭 새 창 분리 (Reparenting)
- 창당 최대 10개의 독립 탭 지원.
- 탭을 외부 바탕화면으로 드래그 시 **`setPointerCapture`**를 채택하여 OS 드롭 금지 제약을 우회하고, 마우스를 떼는 순간 새 독립 윈도우 생성.
- 웹 페이지 리로드 없이 Win32 `SetParent` API를 통해 네이티브 자식 브라우저 HWND를 즉각 이관.

### 5. 🔖 오옴니박스 북마크 UX & 다운로드 관리자
- 주소창 입력 시 북마크 및 방문 기록 동적 탐색 및 키워드 추출기 탑재.
- 백엔드 CEF 다운로드 핸들러 연동: 파일 중복 순서 자동 부여, 진행률 추적 및 JSON 기반 저장소 관리.

---

## 🏗️ 하이브리드 윈도우 구조 (Architecture)

```text
+-------------------------------------------------------------------------+
| Win32 Top-Level Window (Frameless + DWM Shadow / Border)                |
|                                                                         |
|  +-------------------------------------------------------------------+  |
|  | [Top Child Browser] (Height: 100px)                               |  |
|  | - Tab Bar (Multi-Tab, Drag-to-Move, Window Controls)              |  |
|  | - Navigation Bar (Back, Forward, Refresh, Omnibox, Split Toggle)  |  |
|  +-------------------------------------------------------------------+  |
|                                                                         |
|  +-----------------------------------+-------------------------------+  |
|  | [Left Content Child Browser]      | [Right Content Child Browser] |  |
|  | - Main Web Page / Split Left View | - Split Right View (Optional) |  |
|  +-----------------------------------+-------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 🛠️ 기술 스택 (Tech Stack)

| 구분 | 사용 기술 |
| :--- | :--- |
| **백엔드 언어** | C11 (MSVC Compiler, Pure C API) |
| **코어 브라우저 엔진** | CEF 149.0.6 (Chromium Embedded Framework C API) |
| **OS API** | Windows 64-bit Win32 API, GDI, DWM API |
| **웹 프론트엔드 UI** | HTML5, Vanilla CSS3 (Custom Light Theme), JavaScript (ES6+) |
| **빌드 시스템** | CMake 3.19+, Visual Studio MSVC Generator |
| **패키징 도구** | NSIS (Nullsoft Scriptable Install System) 3.x |

---

## 🚀 빌드 및 실행 가이드 (Build & Packaging)

### 1. 빌드 전제 조건
- Windows 10/11 64-bit OS
- Visual Studio 2022 또는 2026 (C/C++ MSVC 툴셋)
- CMake 3.19 이상
- (선택 사항) NSIS 3.x (인스톨러 패키징 시 필요)

### 2. 컴파일 및 빌드 (Release / Debug)

PowerShell 터미널에서 프로젝트 루트 경로를 기준으로 아래 명령어를 실행합니다:

```powershell
# 1) Release 모드 빌드 (권장)
cmake --build c:\projects\lite_browser\cef_binary_149.0.6\build --config Release --target cefsimple_capi

# 2) Debug 모드 빌드 (개발 및 디버깅용)
cmake --build c:\projects\lite_browser\cef_binary_149.0.6\build --config Debug --target cefsimple_capi
```

빌드가 성공하면 실행 파일 및 DLL이 아래 디렉토리에 생성됩니다:
- `c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\lite_browser.exe`
- `c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Debug\lite_browser.exe`

---

### 3. NSIS 인스톨러 생성 (`LiteBrowserInstaller.exe`)

독립 실행 가능한 단일 설치 파일(`LiteBrowserInstaller.exe`)을 패키징하려면 다음 명령을 실행합니다:

```powershell
& "C:\Program Files (x86)\NSIS\makensis.exe" c:\projects\lite_browser\installer.nsi
```

패키징이 완료되면 프로젝트 루트 경로에 인스톨러가 생성됩니다:
- **결과물**: [`LiteBrowserInstaller.exe`](file:///c:/projects/lite_browser/LiteBrowserInstaller.exe) (~173.9MB)

---

## 📂 프로젝트 구조 (Project Structure)

```text
c:\projects\lite_browser\
├── README.md                      # GitHub 프로젝트 안내 문서
├── installer.nsi                  # NSIS 인스톨러 스크립트
├── LiteBrowserInstaller.exe       # 배포용 인스톨러 (빌드 결과물, .gitignore 등록)
├── ui\                            # HTML5/CSS/JS 기반 UI 에셋 폴더
│   ├── index.html                 # 메인 탭바 및 네비게이션 주소창 UI
│   ├── style.css                  # 커스텀 라이트 테마 스타일시트
│   ├── app.js                     # 탭 관리, UI 이벤트 및 백엔드 IPC 처리
│   ├── manager.html / css / js    # 북마크 & 오옴니박스 매니저
│   └── extractor.js               # 웹 키워드 및 북마크 추출기
├── cef_binary_149.0.6\            # CEF 149.0.6 바이너리 배포본
│   └── tests\cefsimple_capi\      # C CAPI 메인 백엔드 소스 디렉토리
│       ├── CMakeLists.txt         # C CAPI CMake 타겟 및 빌드 설정
│       ├── cefsimple_win.c        # Win32 메인 진입점 (wWinMain)
│       ├── simple_app.c / h       # Win32 메인 창 생성, 메시지 프로시저, 탭 배치
│       ├── simple_handler.c / h   # IPC 액션 핸들러, 브라우저 수명 주기 관리
│       ├── browser_context.h      # 동적 윈도우/탭 컨텍스트 구조체 정의
│       └── win\inject_icon.py     # 바이너리 아이콘 리소스 자동 주입 스크립트
└── docs\                          # 상세 개발 및 워크스루 문서
    ├── prompt.md                  # 아키텍처 사양 및 제약 사항 문서
    └── walkthrough.md             # 상세 개발 및 기능 구현 워크스루
```

---

## 📄 라이선스 (License)

본 프로젝트는 CEF (Chromium Embedded Framework) BSD 라이선스 및 내부 오픈소스 규칙을 준수합니다. 자세한 내용은 `cef_binary_149.0.6/LICENSE.txt`를 참조하세요.
