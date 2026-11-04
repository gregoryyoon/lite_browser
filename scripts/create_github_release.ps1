# GitHub Release and Asset Upload Pipeline for Lite Browser
param(
    [string]$Token = "",
    [string]$TagName = "v0.2.9",
    [string]$ReleaseName = "Lite Browser v0.2.9",
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

#### 1. 서드파티 결제 모듈(네이버페이, 카카오페이, 토스 등) iframe 팝업 차단 해제 및 네이티브 팝업 가드 연동
- **`--disable-popup-blocking` 스위치 적용**: 아시아나항공 등 항공사 및 전자상거래 예매/결제 페이지에서 교차 출처(Cross-Origin) iframe 내의 `window.open()` 호출이 Chromium 내부 팝업 블로커에 의해 엔진 단에서 일방 차단되던 문제 해결
- **네이티브 팝업 라이프사이클 가드 연동**: Chromium 엔진의 획일적인 차단 대신 `life_span_handler_on_before_popup`으로 모든 팝업 요청을 안전하게 전달받아, 테마 및 DPI 스케일링이 적용된 전용 팝업 윈도우(`LiteBrowserPopupWnd`) 또는 새 탭으로 정밀 라우팅

#### 2. 타이핑 반응성 및 웹 에디터 성능 향상을 위한 맞춤법 검사기(Spellcheck) 전역 비활성화
- **`--disable-spell-checking` 스위치 적용**: 백그라운드 사전 대조 루프와 Hunspell 워커 스레드를 차단하여 타이핑 입력 지연(Input Latency / INP) 제거 및 RAM/CPU 리소스 최적화
- **시각적 노이즈 완전 제거**: TOAST UI Editor, 노션(Notion), 컨플루언스(Confluence), GitHub 등 웹 기반 에디터에서 한글 문장, 소스 코드, 기술 약어 입력 시 발생하던 오작동 붉은 물결선 밑줄 원천 차단

#### 3. 모달 및 권한 다이얼로그 위치 처리 리팩토링
- 다운로드 버블 및 모달 다이얼로그 윈도우 서브클래싱 처리 로직 개선으로 윈도우 스크린 좌표 안정성 확보

#### 4. 배포 문서 및 CEF 기본 정책 투명성 강화
- `README.md` 내에 Lite Browser에서 의도적으로 비활성화한 CEF 기본 기능(팝업 차단기, 맞춤법 검사기, HTTPS 자동 승격, 사이트 격리, 확장 프로그램 등) 및 맞춤형 정책을 투명하게 공개하는 신규 섹션 반영

#### 5. 배포 및 보안 파이프라인
- **Zero-UAC 무음 설치**: Per-User (`%LOCALAPPDATA%\Programs\LiteBrowser`) 아키텍처 및 UAC 승인 창 없는 원활한 백그라운드 업데이트
- **코드 서명 및 무결성 검증**: 최신 Authenticode SHA-256 전자 서명 및 DigiCert RFC 3161 공인 타임스탬프 적용
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