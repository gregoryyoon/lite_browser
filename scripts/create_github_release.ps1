# GitHub Release and Asset Upload Pipeline for Lite Browser
param(
    [Parameter(Mandatory=$true)]
    [string]$Token,
    [string]$TagName = "v0.2.2",
    [string]$ReleaseName = "Lite Browser v0.2.2",
    [string]$InstallerPath = "LiteBrowserInstaller.exe",
    [string]$ReleaseBody = ""
)

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Lite Browser GitHub Release Pipeline                   " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

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
- **상단 툴바 전용 업데이트 뱃지 버튼 도입**: 백그라운드 다운로드 완료 시 더보기 버튼 좌측에 Lucide SVG ``circle-arrow-up`` 뱃지 및 펄스(Pulse) 애니메이션 자동 표시
- **원클릭 즉시 재시작 및 업데이트 적용**: 뱃지 클릭 시 즉시 NSIS 사일런트(``/S``) 설치 인계 및 브라우저 자동 재실행
- **3점(⋮) 메뉴 UX 정제**: 별도 알림 점(Dot) 및 업데이트 중복 메뉴를 제거하여 깔끔하고 미니멀한 메뉴 인터페이스 구축
- **보안 및 무결성**: 최신 Authenticode SHA-256 전자 서명 및 DigiCert RFC 3161 공식 타임스탬프 적용
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
