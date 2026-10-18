# GitHub Release and Asset Upload Pipeline for Lite Browser
param(
    [Parameter(Mandatory=$true)]
    [string]$Token,
    [string]$TagName = "v0.2.0",
    [string]$ReleaseName = "Lite Browser v0.2.0",
    [string]$InstallerPath = "LiteBrowserInstaller.exe"
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

$ReleaseBody = @"
## Lite Browser v0.2.0

### 주요 변경 사항
- **자동 업데이트 시스템 구현**: GitHub Releases 연동 WinHTTP 스트리밍 백그라운드 다운로드 및 실시간 진행률 표시
- **Authenticode 디지털 서명 검증**: ``WinVerifyTrust`` 기반 위변조 검증을 통한 보안 강화
- **원클릭 사일런트 업데이트 적용**: NSIS 사일런트(``/S``) 설치 핸드오버 및 브라우저 자동 재실행
- **단일 버전 관리(SSOT) 구축**: C 엔진, Windows PE 리소스, NSIS 인스톨러, Web UI 버전 일원화 (``version.h``)
- **설정 대시보드 및 메뉴 개선**: ``lite://settings`` 내 실시간 업데이트 카드 연동 및 3점 메뉴 알림 배지 적용
"@

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
