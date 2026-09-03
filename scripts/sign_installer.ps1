<#
.SYNOPSIS
    Signs Lite Browser installer and executable with an Authenticode digital signature.
.DESCRIPTION
    Automates Code Signing for LiteBrowserInstaller.exe and lite_browser.exe using signtool.exe.
    Supports both local self-signed development certificates and commercial CA certificates.
.PARAMETER PfxPath
    Optional path to a custom .pfx certificate. If omitted, uses/generates a local self-signed certificate.
.PARAMETER PfxPassword
    Password for the .pfx certificate.
.PARAMETER TargetFile
    Target executable to sign (default: LiteBrowserInstaller.exe).
#>
param(
    [string]$PfxPath = "",
    [string]$PfxPassword = "LiteBrowserSecurePass123!",
    [string]$TargetFile = "LiteBrowserInstaller.exe"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
Set-Location $ProjectRoot

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Lite Browser Code Signing Pipeline (Authenticode)       " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Locate signtool.exe
$SigntoolCandidates = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue | Sort-Object FullName -Descending
if (-not $SigntoolCandidates -or $SigntoolCandidates.Count -eq 0) {
    Write-Error "signtool.exe not found in Windows Kits. Please verify Windows 10/11 SDK is installed."
    exit 1
}
$Signtool = $SigntoolCandidates[0].FullName
Write-Host "[1/3] Found signtool.exe: $Signtool" -ForegroundColor Green

# 2. Certificate Preparation
$CertDir = Join-Path $ScriptDir "certs"
if (-not (Test-Path $CertDir)) {
    New-Item -ItemType Directory -Path $CertDir -Force | Out-Null
}

$EffectivePfx = $PfxPath
if (-not $EffectivePfx) {
    $DefaultPfx = Join-Path $CertDir "lite_browser_dev.pfx"
    if (-not (Test-Path $DefaultPfx)) {
        Write-Host "[2/3] Generating new Self-Signed Code Signing Certificate..." -ForegroundColor Yellow
        $Cert = New-SelfSignedCertificate `
            -Type CodeSigning `
            -Subject "CN=Gregory Yoon, O=Lite Browser Project, OU=Development" `
            -CertStoreLocation "Cert:\CurrentUser\My" `
            -HashAlgorithm "SHA256" `
            -KeyLength 2048 `
            -NotAfter (Get-Date).AddYears(5) `
            -FriendlyName "Lite Browser Authenticode Certificate"

        $SecurePass = ConvertTo-SecureString -String $PfxPassword -Force -AsPlainText
        Export-PfxCertificate -Cert $Cert -FilePath $DefaultPfx -Password $SecurePass | Out-Null
        Write-Host "      Generated certificate: $DefaultPfx" -ForegroundColor Green
    } else {
        Write-Host "[2/3] Using certificate: $DefaultPfx" -ForegroundColor Green
    }
    $EffectivePfx = $DefaultPfx
}

# 3. Perform Signing
$ResolvedTarget = Join-Path $ProjectRoot $TargetFile
if (-not (Test-Path $ResolvedTarget)) {
    Write-Error "Target file not found: $ResolvedTarget. Please build/package first."
    exit 1
}

Write-Host "[3/3] Signing $TargetFile with SHA-256 and DigiCert RFC 3161 Timestamp..." -ForegroundColor Yellow
$TimestampServer = "http://timestamp.digicert.com"

& $Signtool sign /f $EffectivePfx /p $PfxPassword /fd SHA256 /tr $TimestampServer /td SHA256 /d "Lite Browser Installer" /du "https://github.com/gregoryyoon/lite_browser" /v $ResolvedTarget
if ($LASTEXITCODE -ne 0) {
    Write-Error "signtool.exe failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "Verifying signature:" -ForegroundColor Cyan
& $Signtool verify /pa /v $ResolvedTarget
if ($LASTEXITCODE -ne 0) {
    Write-Warning "Signature verification returned warning/notice (expected for self-signed certificates until trusted)."
}

Write-Host ""
Write-Host "==========================================================" -ForegroundColor Green
Write-Host "  Code Signing completed successfully!                    " -ForegroundColor Green
Write-Host "  File: $ResolvedTarget                                   " -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
