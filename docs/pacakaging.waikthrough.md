# Lite Browser Release 빌드 & NSIS 패키징 워크스루 (Packaging Walkthrough)

본 문서는 `docs/prompt.md` 사양에 따라 개발된 **Lite Browser**의 Release 모드 컴파일 및 **NSIS (Nullsoft Scriptable Install System)**를 활용한 인스톨러 패키징(`LiteBrowserInstaller.exe`) 워크플로우 전체 내역을 정리한 종합 가이드입니다.

---

## 1. 개요 및 핵심 특징

- **목적**: 개발 환경(Debug)에서 완성된 하이브리드 C CAPI 브라우저 바이너리, CEF 149.0.6 런타임 DLL/리소스, 그리고 웹 UI 에셋 전체를 단일 설치 실행 파일(`LiteBrowserInstaller.exe`)로 패키징.
- **최종 결과물**: `c:\projects\lite_browser\LiteBrowserInstaller.exe` (약 173MB)
- **주요 포함 기능**:
  - Windows 64-bit 지원 (`$PROGRAMFILES64\LiteBrowser`)
  - 시작 메뉴 및 바탕화면 바로가기 자동 생성
  - 제어판(프로그램 추가/제거) 연동 및 깔끔한 언인스톨러 탑재
  - UI 에셋 자동 동기화 (`ui\*.*` 와일드카드 패키징)

---

## 2. 빌드 환경 및 출력 구조

### 빌드 스택
- **컴파일러/빌드 시스템**: CMake 4.x + Visual Studio 2022/2026 MSVC (Multi-Configuration Generator)
- **인스톨러 도구**: NSIS 3.x (`C:\Program Files (x86)\NSIS\makensis.exe`)
- **타겟 프로젝트**: `cefsimple_capi`

### Release 빌드 출력 디렉토리
- `C:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\`
  - `cefsimple_capi.exe` / `cefsimple_capi.dll` (메인 바이너리)
  - `libcef.dll` (CEF 핵심 C API 런타임 DLL, ~270MB)
  - `chrome_elf.dll`, `d3dcompiler_47.dll`, `dxcompiler.dll`, `dxil.dll`, `libEGL.dll`, `libGLESv2.dll`, `vk_swiftshader.dll`, `vulkan-1.dll`
  - `icudtl.dat`, `v8_context_snapshot.bin`, `vk_swiftshader_icd.json`
  - `chrome_100_percent.pak`, `chrome_200_percent.pak`, `resources.pak`
  - `locales/` (다국어 `.pak` 데이터 폴더)

---

## 3. NSIS 인스톨러 디자인 (`installer.nsi`)

프로젝트 루트의 [installer.nsi](file:///c:/projects/lite_browser/installer.nsi) 스크립트는 다음 규칙에 따라 구성되어 있습니다.

```nsis
Unicode true
!include "MUI2.nsh"

Name "Lite Browser"
OutFile "LiteBrowserInstaller.exe"
InstallDir "$PROGRAMFILES64\LiteBrowser"

RequestExecutionLevel admin

; 페이지 및 언어 구성 (한국어/영어 지원)
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Korean"

Section "Install"
  SetOutPath "$INSTDIR"
  
  ; 1. 실행 바이너리 및 CEF 핵심 DLL
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\cefsimple_capi.exe"
  File /nonfatal "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\cefsimple_capi.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\libcef.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\chrome_elf.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\d3dcompiler_47.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\dxcompiler.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\dxil.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\icudtl.dat"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\libEGL.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\libGLESv2.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\v8_context_snapshot.bin"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\vk_swiftshader.dll"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\vk_swiftshader_icd.json"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\vulkan-1.dll"
  
  ; 2. 리소스 패키지
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\chrome_100_percent.pak"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\chrome_200_percent.pak"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\resources.pak"
  
  ; 3. Locales 데이터
  SetOutPath "$INSTDIR\locales"
  File "c:\projects\lite_browser\cef_binary_149.0.6\build\tests\cefsimple_capi\Release\locales\*.*"
  
  ; 4. 웹 UI 폴더 (와일드카드 동적 패키징)
  SetOutPath "$INSTDIR\ui"
  File "c:\projects\lite_browser\ui\*.*"
  
  ; 5. 바로가기 및 언인스톨러 생성
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateShortcut "$SMPROGRAMS\Lite Browser.lnk" "$INSTDIR\cefsimple_capi.exe"
  CreateShortcut "$DESKTOP\Lite Browser.lnk" "$INSTDIR\cefsimple_capi.exe"
  
  ; 6. 제어판 (프로그램 추가/제거) 레지스트리 등록
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" "DisplayName" "Lite Browser"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" "QuietUninstallString" "$\"$INSTDIR\uninstall.exe$\" /S"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" "InstallLocation" "$\"$INSTDIR$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" "Publisher" "Lite Browser Developer"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LiteBrowser" "NoRepair" 1
SectionEnd
```

### UI 폴더 동적 포함 (`ui\*.*`)
`ResolveUIPath` C 백엔드는 실행 파일 기준 상대 경로 `ui/index.html`, `ui/editor.html`, `ui/manager.html` 등을 로드합니다. 인스톨러 생성 시 `File "c:\projects\lite_browser\ui\*.*"` 구문을 적용하여 현재 포함된 8개 에셋을 모두 자동으로 설치 경로(`$INSTDIR\ui`)에 보급합니다:
- **메인 주소창/탭 UI**: `index.html`, `style.css`, `app.js`
- **MD 에디터**: `editor.html`
- **북마크 & 오옴니박스 UX**: `manager.html`, `manager.css`, `manager.js`, `extractor.js`

---

## 4. 대용량 파일 관리 전략 (GitHub 100MB Limit)

CEF 인스톨러(`LiteBrowserInstaller.exe`)는 컴파일 및 압축 후 용량이 **약 173MB**입니다. GitHub은 단일 파일 100MB 제한이 있으므로 Git 레포지토리에 direct push가 불가능합니다.

### 1) `.gitignore` 설정 (완료)
빌드된 인스톨러 및 아카이브 파일이 Git 커밋 대상에 포함되지 않도록 `.gitignore`에 등록되어 있습니다:
```gitignore
# Ignore installer build outputs
LiteBrowserInstaller.exe
LiteBrowserInstaller.zip
```

### 2) GitHub Releases를 통한 배포 (권장 방식)
- 소스 코드 커밋만 GitHub에 push.
- GitHub 웹사이트의 **Releases -> Create a new release** 메뉴를 통해 태그 생성 후 `LiteBrowserInstaller.exe` 파일을 직접 첨부 파일(Asset)로 업로드 (최대 2GB 지원).

### 3) Git LFS (Large File Storage) 사용 (선택 사항)
만약 저장소 내에 바이너리를 유지해야 하는 경우:
```powershell
git lfs install
git lfs track "LiteBrowserInstaller.exe"
git add .gitattributes LiteBrowserInstaller.exe
git commit -m "Add installer via Git LFS"
git push origin main
```

---

## 5. 원클릭 빌드 & 패키징 재시행 명령 가이드

소스를 수정하거나 새로운 UI 파일을 추가한 후 인스톨러를 다시 생성할 때는 PowerShell 환경에서 아래 명령을 차례대로 실행합니다:

```powershell
# 1. CMake Release 빌드
cmake --build c:\projects\lite_browser\cef_binary_149.0.6\build --config Release --target cefsimple_capi

# 2. NSIS 인스톨러 생성
& "C:\Program Files (x86)\NSIS\makensis.exe" c:\projects\lite_browser\installer.nsi
```

성공적으로 빌드되면 프로젝트 루트 경로에 최신 [LiteBrowserInstaller.exe](file:///c:/projects/lite_browser/LiteBrowserInstaller.exe)가 갱신됩니다.
