---
name: lite_browser_release
description: >-
  Official release runbook and checklist for LiteBrowser. Standardizes the complete release pipeline:
  Single Source of Truth (SSOT) version bumping, MSVC LTCG Release build, NSIS Per-User Zero-UAC packaging,
  Authenticode SHA-256 + DigiCert RFC 3161 signing, git commit history & walkthrough.md (read-only reference)
  release notes synthesis, promotional README.md synchronization, git tagging, and GitHub Release asset publication.
  Use this skill whenever the user requests an official release, version upgrade deployment, or GitHub Release creation.
---

# LiteBrowser 공식 릴리즈 런북 (Official Release Runbook)

이 런북은 LiteBrowser의 새 버전을 빌드, 패키징, 문서화하고 GitHub Release로 공식 배포할 때 사용하는 표준 작업 절차서입니다.

> [!CAUTION]
> **`docs/walkthrough.md` 읽기 전용(Read-Only) 원칙**:
> `docs/walkthrough.md`는 이전 릴리즈 이후 변경된 기술 내역과 마일스톤을 누락 없이 파악하기 위한 **순수 참고용 문서**입니다. 릴리즈 작업 과정에서 본 파일을 직접 편집하거나 수정해서는 안 됩니다.

---

## 📋 릴리즈 전제 조건 (Prerequisites)
1. **OS**: Windows 10 / 11 64-bit
2. **빌드 도구**: MSVC v143+ (x64), CMake 3.19+
3. **패키징 도구**: NSIS 3.x (`C:\Program Files (x86)\NSIS\makensis.exe`)
4. **서명 도구**: Windows SDK `signtool.exe`, 자체 서명 또는 공인 인증서 (`scripts/certs/`)
5. **스크립트 런타임**: PowerShell 5.1+
6. **인증 토큰**: GitHub Personal Access Token (`$env:GITHUB_TOKEN` 또는 사용자 전달)

---

## 🚀 릴리즈 파이프라인 (7-Phase Workflow)

### Phase 1: 단일 버전 관리 (SSOT) 일괄 동기화
브라우저 전역의 모든 버전 표기를 새 버전(예: `0.2.5`)으로 일괄 갱신합니다.

1. **[`version.h`](file:///c:/projects/lite_browser/cef_binary_154.0.34/tests/cefsimple_capi/version.h)**:
   ```c
   #define LITE_BROWSER_VERSION_MAJOR 0
   #define LITE_BROWSER_VERSION_MINOR 2
   #define LITE_BROWSER_VERSION_PATCH 5
   #define LITE_BROWSER_VERSION_STRING "0.2.5"
   #define LITE_BROWSER_VERSION_RESOURCE_CSV 0,2,5,0
   ```
2. **[`installer.nsi`](file:///c:/projects/lite_browser/installer.nsi)**:
   ```nsis
   !define PRODUCT_VERSION "0.2.5"
   ```
3. **[`cef_binary_154.0.34/tests/cefsimple_capi/win/cefsimple.rc`](file:///c:/projects/lite_browser/cef_binary_154.0.34/tests/cefsimple_capi/win/cefsimple.rc)**:
   - `FILEVERSION 0,2,5,0`, `PRODUCTVERSION 0,2,5,0`
   - `VALUE "FileVersion", "0.2.5.0"`, `VALUE "ProductVersion", "0.2.5.0"`
4. **[`ui/settings.html`](file:///c:/projects/lite_browser/ui/settings.html)**:
   - `<span id="browser-version">0.2.5 64-bit</span>`

> [!IMPORTANT]
> 인스톨러는 Chrome/VS Code 표준의 **Per-User Zero-UAC 무음 설치**(`$LOCALAPPDATA\Programs\LiteBrowser`, `RequestExecutionLevel user`, `HKCU`) 원칙을 엄수해야 합니다.

---

### Phase 2: LTCG 최적화 릴리즈 모드 빌드
Release 빌드는 사용자의 명시적 요청 시에만 수행합니다:

```powershell
cmake --build cef_binary_154.0.34\build --config Release --target cefsimple_capi
```
- **검증**: `cef_binary_154.0.34\build\tests\cefsimple_capi\Release\lite_browser.exe` 생성 확인 (Exit code 0).
- **PE 리소스 주입 확인**: `inject_icon.py` 후처리 및 버전 리소스 반영 여부 검증.

---

### Phase 3: NSIS 인스톨러 패키징 & Authenticode 전자 서명
단일 독립 설치 파일(`LiteBrowserInstaller.exe`)을 컴파일하고 디지털 서명을 부여합니다:

1. **NSIS 컴파일**:
   ```powershell
   & "C:\Program Files (x86)\NSIS\makensis.exe" installer.nsi
   ```
2. **Authenticode SHA-256 + DigiCert RFC 3161 공인 타임스탬프 서명**:
   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts\sign_installer.ps1
   ```
3. **서명 무결성 검증**:
   ```powershell
   & "$env:ProgramFiles (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe" verify /pa /v LiteBrowserInstaller.exe
   ```

---

### Phase 4: 릴리즈 노트 완전성 합성 (Changelog Synthesis)
누락되는 변경 사항이 없도록 아래 두 가지 출처를 교차 검증하여 GitHub 릴리즈 본문을 작성합니다:

1. **Git Commit History**:
   ```powershell
   git log <이전_버전_태그>..HEAD --oneline
   ```
2. **[`docs/walkthrough.md`](file:///c:/projects/lite_browser/docs/walkthrough.md) (참고 전용)**:
   - 이전 릴리즈 이후 추가된 모든 주요 기능, UI 개선, 버그 수정 내역을 교차 확인하여 누락 방지. (※ 파일 수정 금지)
3. **릴리즈 노트 표준 포맷**:
   - `## Lite Browser vX.Y.Z`
   - `### 주요 변경 사항`
     - 1. 웹페이지 저장 및 컨텍스트 메뉴 확장 (신규 기능)
     - 2. 탭 및 윈도우 관리 안정성 강화 (단축키, Z-order, 타이틀 동기화)
     - 3. 웹 권한 제어 및 보안 강화 (Permission Handler)
     - 4. 배포 및 업데이트 파이프라인 (Zero-UAC 무음 설치, 코드 서명)

---

### Phase 5: `README.md` 프로젝트 소개 문서 동기화 원칙
GitHub 메인 페이지에 노출되는 대표 문서는 프로젝트 홍보 및 핵심 장점 소개용이므로 자잘한 버그 수정 기록을 배제하고 깔끔하게 유지합니다:

1. **[`README.md`](file:///c:/projects/lite_browser/README.md) 업데이트 항목**:
   - 최신 릴리즈 뱃지 갱신 (`Release-vX.Y.Z`)
   - 최신 버전 다운로드 링크 확인
   - 브라우저의 핵심 가치 및 6대 주요 장점(성능, 벤토 UI, 듀얼 스플릿 뷰, AI 사이드패널, Zero-UAC 무음 업데이트, Windows 셸 연동)에 부합하는 대형 신규 기능이 있을 경우에만 핵심 장점 섹션에 간결히 반영
   - 세부 버그 수정, 단축키 추가 등 자잘한 수정 사항은 `README.md`에 추가하지 않고 **GitHub Release Notes에만 기록**
2. **`docs/walkthrough.md` 수정 금지**:
   - `docs/walkthrough.md`는 개발 기록 보존 문서이므로 릴리즈 스킬에서 수정하지 않습니다.

---

### Phase 6: Git 커밋, 태깅 및 원격 푸시
버전 갱신과 `README.md` 동기화 내역을 원격 리포지토리에 반영합니다:

```powershell
# 1. 파일 스테이징 및 커밋
git add cef_binary_154.0.34/tests/cefsimple_capi/version.h installer.nsi cef_binary_154.0.34/tests/cefsimple_capi/win/cefsimple.rc ui/settings.html README.md scripts/create_github_release.ps1
git commit -m "release: vX.Y.Z"

# 2. 버전 태그 생성
git tag -a vX.Y.Z -m "Lite Browser vX.Y.Z"

# 3. 원격 main 브랜치 및 태그 동시 푸시
git push origin main
git push origin vX.Y.Z
```

---

### Phase 7: GitHub Release 자동 배포 및 인스톨러 업로드
[`scripts/create_github_release.ps1`](file:///c:/projects/lite_browser/scripts/create_github_release.ps1)을 호출하여 GitHub Release를 발행하고 서명된 인스톨러를 첨부합니다.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\create_github_release.ps1 `
    -Token $Token `
    -TagName "vX.Y.Z" `
    -ReleaseName "Lite Browser vX.Y.Z" `
    -InstallerPath "LiteBrowserInstaller.exe" `
    -ReleaseBody $ReleaseNotes
```

> [!TIP]
> `create_github_release.ps1`은 반드시 **UTF-8 with BOM**으로 저장되어 있어야 한글 릴리즈 본문이 깨지지 않고 정상 전송됩니다.
