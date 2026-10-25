# 🌐 Lite Browser

<p align="center">
  <img src="docs/logo.png" width="108" height="108" alt="Lite Browser Logo" />
</p>

<p align="center">
  <b>Chromium Embedded Framework (CEF 154) 순수 Win32 C CAPI 기반의 초경량·초고속 하이브리드 데스크톱 웹 브라우저</b>
</p>

<p align="center">
  <a href="https://github.com/gregoryyoon/lite_browser/releases/latest"><img src="https://img.shields.io/badge/Release-v0.2.5-brightgreen.svg?logo=github&logoColor=white" alt="Latest Release v0.2.5" /></a>
  <a href="https://github.com/gregoryyoon/lite_browser"><img src="https://img.shields.io/badge/Platform-Windows%2064--bit-0078D4.svg?logo=windows&logoColor=white" alt="Platform Windows 64-bit" /></a>
  <a href="https://github.com/gregoryyoon/lite_browser"><img src="https://img.shields.io/badge/Language-C11%20%2F%20Pure%20C%20API-00599C.svg?logo=c&logoColor=white" alt="Language C11 / Pure Win32 C CAPI" /></a>
  <a href="https://github.com/gregoryyoon/lite_browser"><img src="https://img.shields.io/badge/CEF-154.0.34%20(Chromium%20154)-FF6F00.svg?logo=googlechrome&logoColor=white" alt="CEF 154.0.34 / Chromium 154" /></a>
  <a href="https://github.com/gregoryyoon/lite_browser"><img src="https://img.shields.io/badge/UI-Bento%20Grid%20%2F%20Lucide%20SVG-F7DF1E.svg?logo=javascript&logoColor=black" alt="Bento Grid & Lucide SVG" /></a>
  <a href="https://github.com/gregoryyoon/lite_browser"><img src="https://img.shields.io/badge/Optimization-ThinLTO%20%2F%20LTCG-orange.svg" alt="ThinLTO & LTCG Whole Program Optimization" /></a>
  <a href="https://github.com/gregoryyoon/lite_browser"><img src="https://img.shields.io/badge/Security-Authenticode%20SHA--256-success.svg" alt="Authenticode SHA-256 Code Signed" /></a>
  <a href="https://github.com/gregoryyoon/lite_browser"><img src="https://img.shields.io/badge/Updater-Zero--UAC%20Per--User-blueviolet.svg" alt="Zero-UAC Silent Updater" /></a>
  <a href="https://github.com/gregoryyoon/lite_browser"><img src="https://img.shields.io/badge/Build-CMake%20%2F%20MSVC-064F8C.svg?logo=cmake&logoColor=white" alt="Build CMake & MSVC" /></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-BSD%203--Clause-blue.svg" alt="License BSD-3-Clause" /></a>
</p>

---

## 📌 왜 Lite Browser인가요? (Why Lite Browser?)

현대의 웹 브라우저는 점점 더 무거워지고 있습니다. **Lite Browser**는 무거운 C++ 런타임 추상화 계층과 프레임워크 오버헤드를 과감히 걷어내고, **순수 C 언어(C11)**로 Chromium의 강력한 엔진(CEF)과 **Windows Win32 Native API**를 직결하여 탄생했습니다.

빠르고 쾌적한 반응성, 극상의 메모리 효율, 그리고 현대적인 **벤토 그리드(Bento Grid) 웹 UI**가 어우러져 **듀얼 화면 분할(Split-View)**, **실시간 탭 분리(Tear-off)**, **지능형 옴니박스 및 스마트 북마크(`lite://favorites`)**, **다운로드 관리자(`lite://downloads`)**, **상시 도킹 AI 에이전트 사이드패널**, **Zero-UAC 무음 자동 업데이트**까지 일상적인 웹 서핑부터 전문적인 멀티태스킹까지 가장 가볍고 강력한 브라우징 경험을 선사합니다.

---

## ✨ 핵심 장점 및 주요 기능 (Key Highlights)

### ⚡ 1. 압도적인 성능과 극상의 효율 (Pure C11 & ThinLTO / LTCG)
- **Zero C++ Overhead**: C++ 가상 함수 테이블이나 프레임워크 래퍼 없이, 순수 C11 C API와 원자적 연산(`stdatomic.h`)으로 CEF 코어를 직접 제어하여 눈부신 시동 속도를 자랑합니다.
- **맞춤형 런타임 성능 모드**:
  - **실행 속도 우선 (Speed Priority)**: GPU 하드웨어 가속과 Direct3D 래스터화로 최고 성능의 웹 렌더링을 제공합니다.
  - **메모리 절감 우선 (Memory Saver Priority)**: 렌더러 프로세스 및 V8 힙을 최적화하여 저사양 PC나 노트북에서도 RAM 점유율을 30~50% 대폭 줄여줍니다.
- **Whole Program 최적화**: 배포 바이너리에 MSVC 링크 타임 코드 생성(`/LTCG`) 및 최대 속도 최적화(`/O2`)를 적용하여 타협 없는 실행 성능을 발휘합니다.

### 🍱 2. 세련된 벤토 그리드(Bento Grid) UI & 완벽한 다크/라이트 테마
- **감각적인 대시보드 디자인**: 설정(`lite://settings`), 북마크(`lite://favorites`), 다운로드(`lite://downloads`) 등 모든 내부 페이지에 심미성과 정보 밀도를 극대화한 **Bento Grid** 카드 UI를 적용했습니다.
- **화이트 플래시(White Flash) 제로**: Win32 네이티브 윈도우 배경 브러시와 DWM 다크 모드를 완벽히 연동하여, 창 전환이나 페이지 로딩 시 눈부심이 전혀 없는 쾌적한 다크 모드를 완성했습니다.
- **인라인 Lucide SVG 표준화**: 외부 웹폰트나 무거운 CDN 의존 없이 단독 오프라인에서도 아름답게 표시되는 순수 인라인 Lucide SVG 아이콘을 채택했습니다.

### ◫ 3. 프로페셔널 멀티태스킹: 듀얼 화면 분할 (Dual Split-View) & 자유로운 탭 관리
- **내장 듀얼 화면 분할 (Split-View)**: 별도 확장 프로그램 없이 클릭 한 번으로 단일 탭 내에서 좌/우 2개의 웹페이지를 동시에 비교하고 작업할 수 있습니다 (0.2 ~ 0.8 비율 자유 조절).
- **스마트 탭 매니지먼트**:
  - 탭이 많아져도 자동으로 균등 축소되어 한눈에 파악되는 모던 탭바.
  - 현재 보던 페이지 바로 옆에 새 탭을 열어주는 스마트 상대 위치 삽입.
  - 탭을 창 밖으로 드래그하면 즉시 독립 창으로 분리되고, 다른 창으로 드롭하면 매끄럽게 재결합되는 자유로운 탭 도킹.

### 🤖 4. 상시 대기 독립 도킹 AI 사이드패널 & 보안 볼트
- **작업 흐름을 방해하지 않는 AI 파트너**: 탭을 전환하거나 새 창을 열어도 대화 세션이 끊기지 않는 독립 네이티브 도킹 사이드패널을 제공합니다.
- **Claude & ChatGPT 유료 구독 모델 직접 연동**: 복잡하고 번거로운 종량제 API Key 발급 없이도, 기존에 사용 중인 **ChatGPT Plus/Pro** 및 **Claude Pro** 구독 계정을 원클릭으로 연동하여 최신 플래그십 AI 모델을 무제한 감각으로 활용할 수 있습니다. (Google Gemini 및 로컬 Ollama 모델도 완벽 지원)
- **실시간 웹페이지 인플레이스 번역**: 내장 번역 엔진이 없는 CEF 환경의 한계를 돌파하여, 해외 웹페이지의 DOM 구조를 보존한 채 자연스러운 한국어로 즉시 번역하고 원문/번역문을 양방향으로 실시간 토글합니다.
- **자율 탭 제어 & 스마트 브라우징 도구**: "새 탭에 깃허브 열어줘", "열린 탭 정리해줘", "탭 2개 분할해서 비교해줘" 등 자연어 명령으로 브라우저 탭을 능동적으로 생성, 전환, 재배치, 분할 탐색합니다.
- **Gemini 3.7 Flash 스마트 요약 & YouTube 분석**: 긴 기사, 블로그, 심지어 YouTube 동영상 콘텐츠까지 5단계 스마트 분석 엔진으로 핵심만 명쾌하게 요약해 줍니다.
- **Windows DPAPI 보안 볼트**: 사용자 로그인 세션 토큰 및 API 키를 일반 파일에 평문 저장하지 않고, Windows OS 수준의 하드웨어 암호화(`CryptProtectData`)로 안전하게 격리 보호합니다.

### 🔍 5. 지능형 옴니박스 (Smart Omnibox Search)
- **통합 스마트 검색 카드**: 주소창 입력 시 북마크(상단), 웹 검색(중단), 방문 기록(하단)을 한 화면에서 스마트 카드로 통합 탐색할 수 있습니다.
- **`#` 태그 숏컷 검색**: `#개발`, `#뉴스`, `#쇼핑` 등 해시태그를 입력하면 해당 태그가 지정된 북마크만 즉시 필터링하여 찾아줍니다.
- **방문 빈도 & 최신성 2단계 지능형 랭킹**: 최근 30일간의 방문 빈도와 최신 방문 시점을 정밀 가중 계산하여 가장 자주 찾는 웹페이지를 최상단에 자동 배치하며, 누적 방문 횟수(`👁️ N회 방문`)를 실시간으로 표시합니다.

### 🔖 6. 지능형 북마크 관리자 대시보드 (`lite://favorites`)
- **맥락 추출 및 스마트 200자 자동 요약 엔진**: 북마크 저장 시 웹페이지의 핵심 내용을 지능 분석하여 200자 요약문과 주요 키워드 태그 5개를 자동으로 추출·생성합니다.
- **하이라이트 인용구 앵커 저장**: 웹 서핑 중 페이지 본문에서 중요한 텍스트를 드래그하여 북마크하면, 해당 인용구와 위치 앵커가 함께 영구 저장되어 언제든 원문의 맥락으로 즉시 복귀할 수 있습니다.
- **모던 벤토 대시보드 & 시각화 관리**: 타임라인 슬라이더, 태그 필터링, 카드/리스트 뷰 전환을 통해 방대한 북마크도 직관적이고 깔끔하게 관리할 수 있습니다.

### 📥 7. 다운로드 관리자 대시보드 (`lite://downloads`)
- **조용한 백그라운드 다운로드**: 시야를 가리는 번거로운 브라우저 팝업 버블을 배제하고 백그라운드에서 신속하고 조용하게 파일을 수신합니다.
- **실시간 툴바 원형 프로그레스 링**: 상단 툴바에 실시간 진행률(0%~100%) 원형 링 인디케이터가 표시되며, 완료 시 깔끔한 체크마크(✓) 펄스 애니메이션으로 완료를 알려줍니다.
- **중복 방지 & 안전한 유니코드 파일 관리**: 동일 파일 존재 시 자동 순서 번호 부여(`filename (1).ext`)로 파일 덮어쓰기를 방지하며, 한글/공백/특수문자가 포함된 경로도 Windows Unicode API를 통해 안전하게 파일 열기 및 폴더 열기를 지원합니다.

### 🔄 8. 무음 백그라운드 자동 업데이트 (Zero-UAC Silent Updater)
- **방해 없는 무음 업데이트**: Chrome 및 VS Code 표준의 Per-User (`%LOCALAPPDATA%\Programs\LiteBrowser`) 아키텍처를 채택하여, **설치부터 업데이트까지 번거로운 UAC(사용자 계정 컨트롤) 승인 창이 전혀 뜨지 않습니다.**
- **백그라운드 스트리밍 & 원클릭 재시작**: 브라우징 중 백그라운드에서 유휴 대역폭으로 최신 패키지를 안전하게 다운로드 및 디지털 서명 검증(`WinVerifyTrust`)한 후, 상단 툴바 알림 뱃지 클릭 한 번으로 수 초 만에 최신 버전을 적용합니다.

### 🪟 9. Windows 10 / 11과의 매끄러운 셸 일체감
- **원클릭 기본 브라우저 연동**: 원클릭으로 Windows 기본 브라우저로 설정하고, 시스템 설정 변경 시 브라우저 내 상태 배지가 실시간으로 자동 갱신됩니다.
- **네이티브 윈도우 조화**: 100% 알파 투명 HiDPI 아이콘, DWM 프레임리스 섀도우, Windows 셸 캐시 즉시 동기화로 운영체제와 완벽한 일체감을 제공합니다.

---

## 🏗️ 윈도우 레이아웃 및 아키텍처 (Architecture)

```text
+-----------------------------------------------------------------------------------------+
| Win32 Top-Level Window (Frameless, DWM Shadow, 0px Seamless Layout, Hit-Testing Subclass) |
|                                                                                         |
|  +-----------------------------------------------------------------------------------+  |
|  | [Top Child Browser] (Height: 72px * DPI Scale, Transparent Background)            |  |
|  | - Tab Bar: Multi-Tab, Auto-Shrinking, Relative Insertion, Drag-to-Separate       |  |
|  | - Navigation Bar: Back, Forward, Refresh, Omnibox, Split Button, Sidepanel Toggle |  |
|  +-----------------------------------------------------------------------------------+  |
|                                                                                         |
|  +-------------------------------------------------------------+-----+---------------+  |
|  | [Main Web Page / Split Left View]                           |     | [Docked AI    |  |
|  | (0px Seamless Fit / 2px Blue Selection Border in Split)     | 4px |  Sidepanel]   |  |
|  |                                                             | Res | (Independent  |  |
|  | ----------------- (4px Splitter Bar) ---------------------  | izer|  Native HWND, |  |
|  |                                                             | Bar |  Bento Chat & |  |
|  | [Split Right View (Optional)]                               |     |  DPAPI Vault) |  |
|  | (Edge-Style Rectangular Border, Shared Omnibox Sync)        |     |               |  |
|  +-------------------------------------------------------------+-----+---------------+  |
+-----------------------------------------------------------------------------------------+
```

---

## 🛠️ 기술 스택 (Tech Stack)

| 계층 (Layer) | 구성 요소 (Components) | 세부 기술 사양 (Specification) |
| :--- | :--- | :--- |
| **코어 브라우저 엔진** | CEF (Chromium Embedded Framework) | **CEF 154.0.34** (Chromium 154.0.8037.98), Pure C CAPI (`cefsimple_capi`) |
| **시스템 백엔드 언어** | C 언어 (MSVC Compiler) | **C11** (`/std:c11`, `/experimental:c11atomics`), Pure Win32 API, GDI, DWM |
| **빌드 최적화 (LTO)** | MSVC Whole Program Optimization | `/GL`, `/LTCG`, `/O2`, `/Oi`, `/Ot`, `/Gy`, `/GF`, `/OPT:REF`, `/OPT:ICF` |
| **보안 & 암호화** | Windows Security APIs | **Windows DPAPI** (`CryptProtectData`), Authenticode **SHA-256 + RFC 3161 Timestamp** |
| **자동 업데이트** | Zero-UAC Silent Updater Engine | **WinHTTP 백그라운드 스트리밍**, `WinVerifyTrust` 전자 서명 검증 |
| **웹 프론트엔드 UI** | Chrome Controls & Dashboards | **HTML5, Vanilla CSS3 (Bento Grid Tokens), ES6+ JavaScript, Inline Lucide SVG** |
| **빌드 시스템** | CMake & Visual Studio | CMake 3.19+, Visual Studio 2022/2026 MSVC C/C++ x64 |
| **배포 패키징 도구** | NSIS (Nullsoft Scriptable Install System) | **NSIS 3.x 64-bit Unicode (Per-User `%LOCALAPPDATA%\Programs\LiteBrowser`, Zero-UAC)** |

---

## 🚀 빠른 시작 (Getting Started)

### 1. 다운로드 및 설치 (Download)
GitHub 최신 릴리즈 페이지에서 공식 인스톨러를 다운로드하여 바로 실행할 수 있습니다:
- **[최신 버전 다운로드 (GitHub Releases)](https://github.com/gregoryyoon/lite_browser/releases/latest)**
- Authenticode SHA-256 디지털 서명 및 DigiCert RFC 3161 공인 타임스탬프가 적용되어 있어 안전하게 설치할 수 있습니다.

---

### 2. 소스코드 빌드 (Build from Source)

#### 전제 조건 (Prerequisites)
- Windows 10 또는 Windows 11 (64-bit)
- Visual Studio 2022 또는 2026 (C/C++ MSVC x64)
- CMake 3.19 이상

#### 빌드 명령어 (PowerShell)
```powershell
# Debug 모드 빌드 (개발 및 디버깅용)
cmake --build cef_binary_154.0.34\build --config Debug --target cefsimple_capi

# Release 모드 빌드 (LTCG 최적화 적용 배포용)
cmake --build cef_binary_154.0.34\build --config Release --target cefsimple_capi
```

빌드가 완료되면 실행 파일이 아래 경로에 생성됩니다:
- `cef_binary_154.0.34\build\tests\cefsimple_capi\Release\lite_browser.exe`

---

## 📂 프로젝트 구조 (Project Structure)

```text
lite_browser\
├── README.md                                  # GitHub 프로젝트 대표 안내 문서
├── LICENSE                                    # BSD 3-Clause 오픈소스 라이선스
├── installer.nsi                              # NSIS 64-bit Per-User Zero-UAC 인스톨러 스크립트
├── scripts\                                   # 빌드, 서명, 배포 자동화 파이프라인
│   ├── create_github_release.ps1              # GitHub Release 자동 생성 및 패키지 배포
│   └── sign_installer.ps1                     # signtool 기반 SHA-256 + DigiCert RFC 3161 코드 서명기
├── ui\                                        # 벤토 그리드 Web UI 에셋 (HTML5 / CSS3 / ES6+)
│   ├── index.html / style.css / app.js        # 상단 프레임리스 탭바 & 지능형 주소창 컨트롤 UI
│   ├── settings.html / css / js               # lite://settings 전용 설정 대시보드
│   ├── manager.html / css / js                # lite://favorites 차세대 북마크 대시보드
│   ├── downloads.html / css / js              # lite://downloads 다운로드 관리자 대시보드
│   ├── sidepanel.html / css / js              # 독립 도킹 AI 에이전트 사이드패널 UI
│   └── theme.js                               # 벤토 그리드 다크/라이트 테마 전역 브로드캐스트 엔진
├── cef_binary_154.0.34\                       # CEF 154.0.34 바이너리 배포본 (Chromium 154)
│   └── tests\cefsimple_capi\                  # 순수 C11 C API 메인 백엔드 소스 디렉터리
│       ├── version.h                          # 브라우저 전역 단일 버전 관리 (SSOT)
│       ├── cefsimple_win.c                    # Win32 메인 진입점 (wWinMain), 로캘 감지, root_cache 격리
│       ├── simple_updater.c / h               # 백그라운드 무음 업데이트 스트리밍 및 WinVerifyTrust 검증
│       ├── default_browser.c / h              # Windows 10/11 기본 브라우저 레지스트리 연동
│       ├── simple_optimization.c / h          # 속도 우선 vs 메모리 절감 런타임 최적화 엔진
│       ├── simple_app.c / h                   # Win32 메인 프로시저, 서브클래싱, 0px 심리스 레이아웃
│       ├── simple_handler.c / h               # IPC 액션 라우터, 탭 제어, 옴니박스 동기화
│       ├── simple_life_span_handler.c         # 팝업 가로채기 및 UAF 방지 비동기 수명 주기 가드
│       ├── simple_download_handler.c / h      # CEF 다운로드 추적, 자동 중복 순번 및 상태 동기화
│       └── simple_vault.c / h                 # Windows DPAPI (CryptProtectData) 보안 볼트
└── docs\                                      # 프로젝트 문서 및 에셋
    ├── logo.png                               # 256x256 HiDPI 투명 브라우저 로고
    └── walkthrough.md                         # 전체 기능 및 개발 마일스톤 통합 기술 보고서
```

---

## ⚠️ 알려진 제한 사항 및 호환성 안내 (Known Limitations & Compatibility)

### 🔒 국내 금융·증권사 보안 프로그램 (Kings Online Security 등) 호환성
- **현상**: 한국투자증권 등 일부 국내 금융 및 증권 포털 접속 시 `Kings Online Security 작동 중 오류가 발생했습니다. (ERROR_PROTECTION_FAILED)` 경고창이 발생하며 로그인/거래 페이지 진입이 불가합니다.
- **원인 (바이너리 분석 결과)**:
  - 금융권 키보드 보안 및 프로세스 보호 솔루션인 **Kings Online Security (K-Defense, 킹스정보통신)**의 백그라운드 서비스(`KOSinj.exe`의 `fnMultiBrowserCheck`)는 보호 대상 브라우저 목록을 메이저 브라우저 실행 파일명(`chrome.exe`, `msedge.exe`, `whale.exe`, `opera.exe`, `vivaldi.exe`, `firefox.exe`, `safari.exe`, `msedgewebview2.exe` 등)으로 하드코딩(화이트리스트)하여 검사합니다.
  - Lite Browser의 실행 파일명인 `lite_browser.exe`는 미지원 브라우저(`Unknown browser type`)로 판정되어 보안 모듈 주입(`fnMultiBroInjection`) 및 메모리 보호 처리가 차단됩니다.
- **안내 사항**:
  - Lite Browser는 가볍고 빠른 웹 서핑과 안전한 브라우징 환경을 지향하며, 프로세스 위장이나 비인가 DLL 강제 주입 허용 등 보안 안정성을 해치는 우회 처리는 수행하지 않습니다.
  - 따라서 해당 보안 프로그램(KOS) 구동이 필수적인 금융 거래 및 증권사 로그인 업무 시에는 **Microsoft Edge** 또는 **Google Chrome** 등 공식 지원 브라우저를 이용해 주시기 바랍니다.

---

## 📄 라이선스 (License)

본 프로젝트는 Chromium 및 CEF (Chromium Embedded Framework) 오픈소스 라이선스 규정을 준수합니다.

- **Chromium / CEF**: [BSD 3-Clause License](https://opensource.org/licenses/BSD-3-Clause)
- **Lite Browser Source Code**: [BSD 3-Clause License](https://opensource.org/licenses/BSD-3-Clause)
- 자세한 라이선스 조항은 [`LICENSE`](LICENSE) 및 [`cef_binary_154.0.34/LICENSE.txt`](cef_binary_154.0.34/LICENSE.txt)를 참조하십시오.
