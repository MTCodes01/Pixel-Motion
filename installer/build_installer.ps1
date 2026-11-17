# Pixel Motion Installer Build and Code-Signing Script
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = (Get-Item $scriptDir).Parent.FullName
$releaseDir = Join-Path $rootDir "build\bin\Release"
$binDir = Join-Path $rootDir "build\bin"
$certDir = Join-Path $rootDir "cert"
$pfxPath = Join-Path $certDir "PixelMotion.pfx"
$pfxPassword = "PixelMotion2026"

# 1. Locate tools
$signtool = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
if (!(Test-Path $signtool)) {
    $signtool = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe" | Select-Object -Last 1).FullName
}

$iscc = "C:\Users\sreed\AppData\Local\Programs\Inno Setup 6\ISCC.exe"
if (!(Test-Path $iscc)) {
    $iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
}

Write-Host "[1/5] Checking tools..." -ForegroundColor Cyan
Write-Host "  SignTool: $signtool"
Write-Host "  ISCC:     $iscc"

# 2. Sign application binaries and DLLs
Write-Host "`n[2/5] Digitally signing application binaries..." -ForegroundColor Cyan
$filesToSign = @(
    (Join-Path $releaseDir "PixelMotion.exe"),
    (Join-Path $releaseDir "avcodec-62.dll"),
    (Join-Path $releaseDir "avformat-62.dll"),
    (Join-Path $releaseDir "avutil-60.dll"),
    (Join-Path $releaseDir "swresample-6.dll"),
    (Join-Path $releaseDir "swscale-9.dll")
)

foreach ($f in $filesToSign) {
    if (Test-Path $f) {
        Write-Host "  Signing $(Split-Path -Leaf $f)..."
        & $signtool sign /f $pfxPath /p $pfxPassword /fd SHA256 /tr "http://timestamp.digicert.com" /td SHA256 $f
    }
}

# 3. Compile Inno Setup Script
Write-Host "`n[3/5] Compiling Inno Setup installer..." -ForegroundColor Cyan
$issPath = Join-Path $scriptDir "PixelMotion.iss"
& $iscc $issPath

# 4. Sign the Setup installer executable
$setupExe = Join-Path $binDir "PixelMotion-Setup-v1.2.2.exe"
if (!(Test-Path $setupExe)) {
    throw "Setup executable was not generated at: $setupExe"
}

Write-Host "`n[4/5] Digitally signing installer: $(Split-Path -Leaf $setupExe)..." -ForegroundColor Cyan
& $signtool sign /f $pfxPath /p $pfxPassword /fd SHA256 /tr "http://timestamp.digicert.com" /td SHA256 $setupExe

# 5. Defender Scan verification
Write-Host "`n[5/5] Running Windows Defender scan..." -ForegroundColor Cyan
$mpCmd = "C:\Program Files\Windows Defender\MpCmdRun.exe"
if (Test-Path $mpCmd) {
    & $mpCmd -Scan -ScanType 3 -File $setupExe
}

Write-Host "`nBuild and signing completed successfully!" -ForegroundColor Green
Write-Host "Installer output: $setupExe"
