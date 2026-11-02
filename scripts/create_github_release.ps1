# GitHub Release and Asset Upload Pipeline for Lite Browser
param(
    [string]$Token = "",
    [string]$TagName = "v0.2.7",
    [string]$ReleaseName = "Lite Browser v0.2.7",
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

#### 1. 오즈리포트뷰어/전자민원 인쇄 대화상자 미표시 해결 및 Chromium 내장 PDF 뷰어 연동
- **Chromium/Edge 내장 스킴 전체 허용**: 전자가족관계등록시스템(`efamily.scourt.go.kr`) 등 공공기관 웹사이트에서 오즈리포트뷰어(OZ Report HTML5 Viewer)의 인쇄 아이콘 클릭 시, PDF 뷰어 확장 스킴(`chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/...`)이 외부 프로토콜로 오인되어 네비게이션이 취소되던 문제를 해결
- **Chromium 인쇄 미리보기 (Print Preview) 전면 활성화**: `enable-print-preview` 플래그 활성화를 통해 크롬 브라우저와 동일한 직관적인 인쇄 미리보기 화면(사이드바 인쇄 옵션, 레이아웃/컬러/배율 설정, 대상 프린터 선택 및 PDF 저장) 제공
- **전역 `Ctrl + P` 인쇄 단축키 완벽 연동**: 일반 웹페이지 본문, 팝업창, 상단 주소창 포커스 상태 어디서든 `Ctrl + P` 입력 시 인쇄 미리보기가 즉시 실행되도록 단축키 파이프라인 보강
- **팝업창 부모 HWND 보정 및 단축키 지원**: 팝업창(`BROWSER_TYPE_POPUP`)에서 모달 부모 윈도우를 팝업 창 자체로 지정하여 인쇄/저장 다이얼로그가 팝업 뒤로 가려지는 현상 방지 및 `Ctrl + W` / `Ctrl + F4` 팝업 닫기 지원

#### 2. Windows 기본 PDF 뷰어 등록 연동 (PDF File Associations)
- **인스톨러(.nsi) 및 런타임 PDF 기본 앱 연동**: 인스톨러 설치 시 `HKCU\Software\Classes\.pdf`를 `LiteBrowserHTML`로 정식 등록하고 `OpenWithProgids` 및 `FileExts` 레지스트리를 설정하여 윈도우 탐색기 우클릭 '연결 프로그램' 및 Windows 기본 앱 목록에 LiteBrowser가 즉시 노출
- 프로그램 제거 시 등록된 PDF 레지스트리 키를 깨끗하게 원복하도록 언인스톨러 스크립트 보강

#### 3. AI 사이드패널 Gemini 스타일 사고 과정(Thinking) UI 고도화
- **CoT 사고 과정 스트리밍 아코디언 UI**: 최신 LLM(Thinking 모델)의 사고 과정을 실시간으로 확인할 수 있는 접기/펼치기 아코디언 컴포넌트 탑재
- **반응형 펄스 인디케이터**: 모델 응답 생성 중 실시간 상태를 안내하는 펄스 애니메이션 도트 적용 및 스트리밍 완료 후 버튼 상태 동기화 개선

#### 4. 다운로드 서브시스템 간소화 및 안정성 극대화
- 임시 인라인 캐싱 및 복잡한 Gzip 인플레이스 디컴프레서 모듈을 정규 CEF 다운로드 스트림 파이프라인으로 일원화하여 다운로드 신뢰성 및 시스템 안정성 대폭 향상

#### 5. 배포 및 업데이트 파이프라인
- **Windows Git Credential Manager (GCM) 전자동 연동**: Windows 자격 증명 관리자에 안전하게 저장된 GitHub 토큰을 자동 감지하여 100% 무인 자동 배포
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