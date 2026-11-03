# GitHub Release and Asset Upload Pipeline for Lite Browser
param(
    [string]$Token = "",
    [string]$TagName = "v0.2.8",
    [string]$ReleaseName = "Lite Browser v0.2.8",
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

#### 1. 내장 마크다운(Markdown) 위지웍 에디터 & 뷰어 탑재 (Embedded WYSIWYG Editor)
- **완전 오프라인 번들링**: TOAST UI Editor 3.2.2 및 확장 플러그인(`tui-editor-extras`)을 로컬에 100% 내장하여 인터넷 연결 없이도 안전하게 구동
- **웹 링크 & 로컬 파일 즉각 열람**: 웹 서핑 중 `.md` 및 `.markdown` 링크(GitHub 원문 등) 클릭 시 깔끔한 타이포그래피의 웹 문서로 실시간 렌더링하며, 로컬 마크다운 파일 열람 및 위지웍 편집/저장(`Ctrl + S`) 지원
- **스마트 원본 서식 보존**: diff 기반 알고리즘을 적용하여 편집 후 저장 시 원본 문서의 들여쓰기 깊이, 불릿 스타일, 줄바꿈(CRLF) 서식을 정밀 보존
- **Mermaid 다이어그램 & 테마 동기화**: 문서 내 포함된 Mermaid 다이어그램 실시간 렌더링(지연 로드 최적화 적용) 및 브라우저 전역 다크/라이트 테마 자동 동기화
- **Windows 기본 연결 프로그램 등록**: 인스톨러(`installer.nsi`) 및 런타임(`default_browser.c`)에서 `.md`, `.markdown` 확장자 연결 프로그램(`OpenWithProgids`, `SupportedTypes`) 자동 등록

#### 2. 업데이트 뱃지 VS Code 스타일 상세 팝오버 카드 탑재
- **호버 정보 카드 노출**: 상단 툴바의 업데이트 준비 완료 뱃지에 마우스를 올리면 현재 버전, 최신 버전, 릴리즈 날짜를 한눈에 확인할 수 있는 미니멀 팝오버 카드 표시
- **의도 감지 인터랙션**: 단순 커서 통과 시의 번쩍임을 차단하는 250ms 의도 감지 딜레이 및 카드 조작을 위한 200ms 이동 유예 시간(Grace Period) 적용
- **원클릭 액션 버튼**: [릴리즈 노트] 클릭 시 브라우저 새 탭으로 GitHub Releases 페이지 열람, [지금 재시작] 클릭 시 즉각적인 무음 설치 및 자동 재시작 지원

#### 3. 비밀번호 관리자(`chrome://password-manager`) 싱글톤 탭 포커스 개선
- **일관된 탭 사용자 경험**: 웹사이트 로그인 폼의 열쇠 아이콘(비밀번호 저장/관리) 클릭 또는 `chrome://password-manager` 요청 시 팝업 대신 일반 브라우저 탭으로 생성
- **싱글톤 탭 포커스**: 비밀번호 관리자 탭이 이미 열려 있는 경우 중복 생성 없이 해당 탭으로 즉시 전환 및 포커스 이동

#### 4. 다운로드 팝업 버블 위치 및 레이아웃 안정화
- 다운로드 목록이 길어져 팝업 창 높이가 확장될 때 화면 좌표 경계 계산 오차로 인해 화면 좌상단(0, 0)으로 튕기던(fallback) 현상을 수정하여 툴바 다운로드 버튼 하단에 안정적으로 도킹

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