# GitHub Release and Asset Upload Pipeline for Lite Browser
param(
    [string]$Token = "",
    [string]$TagName = "v0.2.5",
    [string]$ReleaseName = "Lite Browser v0.2.5",
    [string]$InstallerPath = "LiteBrowserInstaller.exe",
    [string]$ReleaseBody = ""
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Lite Browser GitHub Release Pipeline                   " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 0. Resolve GitHub Token (Parameter -> $env:GITHUB_TOKEN -> Git Credential Manager)
if (-not $Token -and $env:GITHUB_TOKEN) {
    $Token = $env:GITHUB_TOKEN
}

if (-not $Token) {
    try {
        $pinfo = New-Object System.Diagnostics.ProcessStartInfo
        $pinfo.FileName = "git.exe"
        $pinfo.Arguments = "credential fill"
        $pinfo.RedirectStandardInput = $true
        $pinfo.RedirectStandardOutput = $true
        $pinfo.UseShellExecute = $false
        $pinfo.CreateNoWindow = $true

        $proc = [System.Diagnostics.Process]::Start($pinfo)
        $proc.StandardInput.WriteLine("protocol=https")
        $proc.StandardInput.WriteLine("host=github.com")
        $proc.StandardInput.WriteLine("")
        $proc.StandardInput.Close()

        $output = $proc.StandardOutput.ReadToEnd()
        $proc.WaitForExit(3000)

        $tokenLine = ($output -split "`r?`n") | Where-Object { $_ -match "^password=" }
        if ($tokenLine) {
            $Token = $tokenLine -replace "^password=", ""
        }
    } catch {
        # ignore error and fallback to check
    }
}

if (-not $Token) {
    throw "GitHub Personal Access Token is required. Please set GITHUB_TOKEN or configure Git Credential Manager."
}

$Repo = "gregoryyoon/lite_browser"
$ApiBase = "https://api.github.com/repos/$Repo"

$Headers = @{
    "Authorization" = "Bearer $Token"
    "Accept" = "application/vnd.github+json"
    "X-GitHub-Api-Version" = "2022-11-28"
    "User-Agent" = "LiteBrowserReleasePipeline"
}

# 1. Check if release already exists for this tag
Write-Host "[1/4] Checking existing releases for tag '$TagName'..." -ForegroundColor Yellow
$ExistingRelease = $null
try {
    $ExistingRelease = Invoke-RestMethod -Uri "$ApiBase/releases/tags/$TagName" -Headers $Headers -Method Get -ErrorAction SilentlyContinue
} catch {
    # 404 is expected if not created yet
}

if (-not $ReleaseBody) {
    $ReleaseBody = @"
## Lite Browser $TagName

### 주요 변경 사항

#### 1. AI 에이전트 실시간 웹페이지 인플레이스 번역 시스템
- **CEF 내장 번역 부재 한계 극복**: Chromium 컴포넌트의 Google 독점 번역 엔진 제약을 해소하고, 사이드패널 AI Provider(Gemini 4 Argon, 3.8 Flash, ChatGPT, Claude 등)를 연동한 실시간 인플레이스 번역 파이프라인 구축
- **DOM 구조 보존 및 양방향 원문/번역문 토글**: 하이퍼링크, 레이아웃 서식을 보존하며 인플레이스 치환하고, 도구 완료 카드의 `[원문 복원]` 및 `[번역문 다시 적용]` 버튼을 통해 실시간 양방향 전환 지원
- **사이드패널 웰컴 카드 '한국어로 번역' 칩 연동**: 원클릭으로 현재 웹페이지 본문 전체를 번역하고 핵심 내용 3줄 요약 및 주요 용어 정리를 마크다운으로 브리핑
- **다국어 번역 및 심층 컨테이너 순회**: 영어 외 일본어, 중국어 등 다국어 번역 지원 및 SPA 리프 노드/iframe 컨테이너 텍스트 추출 정밀화

#### 2. AI 에이전트 자율 탭 제어 서브시스템 & 라이브 도구 UX
- **자율 브라우저 탭 제어 도구군 추가**:
  - `browser_get_tabs`: 열려 있는 모든 탭의 ID, 제목, URL, 활성/분할 상태 조회
  - `browser_open_tab`: 자연어 명령으로 새 탭 생성 및 백그라운드 탐색 제어
  - `browser_switch_tab` / `browser_reorder_tabs`: 탭 포커스 이동 및 인덱스 재배치
  - `browser_split_tabs`: 2개 탭을 0.5f 듀얼 분할 화면으로 즉시 결합
  - `browser_close_tabs`: 확인 카드 팝업 후 안전한 탭 일괄 종료
  - `browser_get_tab_content`: 탭 본문 DOM 추출 및 텍스트/마크다운 분석
- **지연 탐색 파이프라인 (PostDeferredNavigation)**: CEF `on_before_browse` 내부의 동기 URL 로드로 인한 Chromium NavigationThrottle 스택 오버플로우(0xC00000FD) 재진입 크래시를 원천 방지
- **사이드패널 히든 iframe 통신 브릿지 (`sendUiAction`)**: 프레임 내비게이션으로 인한 LLM Fetch 스트리밍 취소 및 렌더러 오염 방지
- **전 도구 실시간 진행(Running) UX 파이프라인**: 도구 호출 즉시 채팅창에 진행 카드를 표시하고(약 2초 시점), 세부 작업 단계(추출 → 분석/번역 → DOM 적용)를 실시간 반영한 뒤 제자리에서 완료 카드로 매끄럽게 전환
- **Gemini 최신 모델 라인업 최신화**: `gemini-4-argon`, `gemini-3.8-flash` 지원 및 429 Quota Exceeded 발생 시 `RetryInfo` 기반 자동 지수 백오프 복원 파이프라인 적용

#### 3. Win32 CJK IME 및 사이드패널 포커스 안정성 강화
- **Chrome_WidgetWin_0 네이티브 포커스 동기화**: 상단 윈도우와 네이티브 CEF 윈도우 간의 포커스 전달을 정규화하여 한국어, 일본어, 중국어(CJK) IME 조합 중 글자 잘림 및 포커스 이탈 문제 해결
- **프롬프트 실행 중 채팅창 포커스 지속 유지**: AI 응답 스트리밍 및 도구 실행 중에도 채팅창 입력 포커스를 지속 유지하여 연속적인 프롬프트 입력 편의 제공

#### 4. 브라우저 안정성 및 배포 메타데이터 개선
- **'다른 이름으로 저장' (Ctrl+S) 단축키 입력 필드 충돌 방지**: input, textarea 등 편집 필드 및 내부 시스템 페이지에서 불필요한 파일 저장 대화상자 호출 억제
- **미등록 프로토콜 방어**: Windows 레지스트리에 미등록된 외부 스키마 호출 시 `ERR_UNKNOWN_URL_SCHEME` 에러 화면 억제
- **PE 리소스 RT_VERSION 주입**: 빌드 후처리 파이프라인에서 `lite_browser.exe`에 정식 `RT_VERSION` 리소스 주입 및 버전 일관성 확보
"@
}

$Release = $null
if ($ExistingRelease) {
    Write-Host "      Found existing release (ID: $($ExistingRelease.id)). Using existing release." -ForegroundColor Green
    $Release = $ExistingRelease
} else {
    Write-Host "[2/4] Creating new GitHub Release '$ReleaseName' ($TagName)..." -ForegroundColor Yellow
    $Payload = @{
        tag_name = $TagName
        target_commitish = "main"
        name = $ReleaseName
        body = $ReleaseBody
        draft = $false
        prerelease = $false
    } | ConvertTo-Json -Compress

    $Release = Invoke-RestMethod -Uri "$ApiBase/releases" -Headers $Headers -Method Post -Body ([System.Text.Encoding]::UTF8.GetBytes($Payload)) -ContentType "application/json; charset=utf-8"
    Write-Host "      Successfully created release: $($Release.html_url) (ID: $($Release.id))" -ForegroundColor Green
}

# 2. Check if asset already uploaded and delete if present
$AssetFileName = [System.IO.Path]::GetFileName($InstallerPath)
Write-Host "[3/4] Checking existing assets for '$AssetFileName'..." -ForegroundColor Yellow
if ($Release.assets) {
    foreach ($asset in $Release.assets) {
        if ($asset.name -eq $AssetFileName) {
            Write-Host "      Deleting existing asset (ID: $($asset.id))..." -ForegroundColor DarkYellow
            Invoke-RestMethod -Uri "$ApiBase/releases/assets/$($asset.id)" -Headers $Headers -Method Delete | Out-Null
        }
    }
}

# 3. Upload Installer Asset
Write-Host "[4/4] Uploading $AssetFileName ($([math]::Round((Get-Item $InstallerPath).Length / 1MB, 2)) MB)..." -ForegroundColor Yellow

$UploadUrl = $Release.upload_url -replace '\{\?name,label\}', "?name=$AssetFileName"
$UploadHeaders = @{
    "Authorization" = "Bearer $Token"
    "Accept" = "application/vnd.github+json"
    "X-GitHub-Api-Version" = "2022-11-28"
    "User-Agent" = "LiteBrowserReleasePipeline"
    "Content-Type" = "application/octet-stream"
}

$UploadResult = Invoke-RestMethod -Uri $UploadUrl -Headers $UploadHeaders -Method Post -InFile $InstallerPath

Write-Host "==========================================================" -ForegroundColor Green
Write-Host "  Release Published Successfully!                         " -ForegroundColor Green
Write-Host "  URL: $($Release.html_url)" -ForegroundColor Green
Write-Host "  Asset Download: $($UploadResult.browser_download_url)" -ForegroundColor Green
Write-Host "  Size: $([math]::Round($UploadResult.size / 1MB, 2)) MB" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green