# RX Media Player - libmpv runtime DLL download helper
$binDir = Join-Path $PSScriptRoot "..\third_party\mpv\bin"
if (!(Test-Path $binDir)) {
    New-Item -ItemType Directory -Path $binDir -Force | Out-Null
}

$libmpvDll = Join-Path $binDir "libmpv-2.dll"
$mpvDll = Join-Path $binDir "mpv-2.dll"

if ((Test-Path $libmpvDll) -and (Test-Path $mpvDll)) {
    Write-Host "[OK] libmpv runtime DLLs already exist in $binDir" -ForegroundColor Green
    exit 0
}

Write-Host "Downloading libmpv Windows x64 runtime package..." -ForegroundColor Cyan
$archivePath = Join-Path $PSScriptRoot "mpv-dev.7z"
$url = "https://sourceforge.net/projects/mpv-player-windows/files/libmpv/mpv-dev-x86_64-20241020-git-7df93b8.7z/download"

try {
    Invoke-WebRequest -Uri $url -OutFile $archivePath -UserAgent "Mozilla/5.0"
    Write-Host "Extracting runtime DLLs..." -ForegroundColor Cyan
    & tar.exe -xf $archivePath -C $binDir
    Remove-Item $archivePath -Force -ErrorAction SilentlyContinue
    Write-Host "[SUCCESS] libmpv runtime deployed successfully to $binDir" -ForegroundColor Green
} catch {
    Write-Warning "Failed to download automatically: $_"
    Write-Host "Please place libmpv-2.dll and mpv-2.dll into $binDir manually." -ForegroundColor Yellow
}
