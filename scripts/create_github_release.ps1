# GitHub Release and Asset Upload Pipeline for Lite Browser
param(
    [string]$Token = "",
    [string]$TagName = "v0.2.6",
    [string]$ReleaseName = "Lite Browser v0.2.6",
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

#### 1. 로그(.log) 및 개발/텍스트 파일 인라인 뷰어 & '다른 이름으로 링크 저장...' 대화상자
- **Windows 레지스트리 개발 확장자 MIME Type 자동 등록**: 브라우저 실행 시 UAC 권한 없이 `HKCU\Software\Classes`에 `.log`, `.out`, `.err`, `.diff`, `.patch`, `.ini`, `.cfg`, `.conf` 8개 확장자를 `text/plain`으로 자동 등록하고 셸 캐시 즉시 갱신
- **인라인 텍스트 뷰어 전환**: CI/웹 서버에서 전송되는 `.log` 파일 링크 클릭 시 강제 다운로드로 전환되지 않고 Microsoft Edge처럼 브라우저 탭 내에서 순수 텍스트(plain text)로 즉시 렌더링 (주소창에 원격 웹 URL 정상 표시 및 `Ctrl+S` 시 로그 전용 필터 적용)
- **링크 우클릭 '다른 이름으로 링크 저장...' 연동**: 링크 컨텍스트 메뉴 명칭을 엣지/크롬 표준인 '다른 이름으로 링크 저장...'으로 개선하고 Windows 네이티브 파일 저장 대화상자(`GetSaveFileNameW`) 연동
- **경량 Gzip 인플레이스 자동 압축 해제**: 서버가 `Content-Encoding: gzip`으로 전송한 파일도 C API 단일 모듈(`simple_gzip.c`)을 통해 다운로드 완료 시점에 인플레이스 자동 압축 해제하여 메모장에서 열었을 때 깨짐 없이 100% 정상 텍스트로 보존

#### 2. Chrome Gemini 스타일 5단계 본문 추출 파이프라인 & 비전(Vision) 하이브리드 폴백
- **인라인 <code> 마크다운 분절 해결 & 코드 블록 보존**: 문장 내 인라인 코드가 단락 분절 없이 단일 백틱으로 보존되고, `<pre><code>` 블록 코드는 언어 클래스 및 들여쓰기 원형 보존
- **YouTube 3-Tier 다중 수집 파이프라인 & 댓글 분석 도구**:
  - 타임스탬프(`[MM:SS]`) 기반 자막 대본(`timedtext`) 수집 및 0바이트 BotGuard PO-Token 회피 시 DOM 트랜스크립트 패널(Tier 2) 안전 전환
  - 단일 진실 공급원(SSOT) 메모리 객체 조회를 통해 10개 챕터 타임라인 및 3,500자 이상 설명글 100% 무손실 보존
  - AI 에이전트 전용 `youtube_get_comments` 도구 신설 (시청자 반응 및 여론 분석)
- **C 런타임 mtime 메모리 캐시 & 무중단 핫 리로딩**: 파편화된 DOM 추출 스크립트를 `ui/content_extractor.js`로 단일화하고, `_stat64` 기반 파일 수정 감지 시 런타임 재컴파일 없이 즉시 리로드
- **Mozilla Readability 휴리스틱 노이즈 필터링 & 16k 헤드-테일 청킹**: 비본문 광고/사이드바 제거, 링크 밀도 0.65 초과 클러스터 가지치기, 긴 문서의 결론 누락을 방지하는 스마트 청킹
- **차트/대시보드 페이지 감지 시 Win32 GDI+ 뷰포트 비전(Vision) 캡처**:
  - Canvas, WebGL, SVG 고밀도 페이지 자동 감지 (`detectVisualContext`)
  - 활성 탭 HWND 클라이언트 영역을 `BitBlt` 및 GDI+ 동적 로드로 80% 압축 품질 JPEG 인코딩
  - `browser_take_screenshot` 툴 및 Gemini / OpenAI / Claude 멀티모달 LLM 비전 페이로드 연동
  - 사이드패널 웰컴 카드 프롬프트 칩 개편 (`[현재 페이지 요약]`, `[화면 캡처 후 분석]`, `[열린 탭 비교]`)

#### 3. 팝업 및 새 탭 라이프사이클 안정성 강화
- **POST 페이로드 및 창 컨텍스트 보존**: `window.open` 새 탭 팝업 시 POST 요청 페이로드와 `window.opener` 컨텍스트가 유실되지 않도록 C API 라이프사이클 가드 완비

#### 4. 배포 및 업데이트 파이프라인
- **Windows Git Credential Manager (GCM) 전자동 연동**: Windows 자격 증명 관리자에 안전하게 저장된 GitHub 토큰을 감지하여 릴리즈 및 인스톨러 배포를 100% 무인 자동화
- **보안 및 무결성**: 최신 Authenticode SHA-256 전자 서명 및 DigiCert RFC 3161 공인 타임스탬프 적용
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