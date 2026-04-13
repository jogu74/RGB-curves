$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot "build\RelWithDebInfo"
$releaseRoot = Join-Path $projectRoot "release"
$cmakeText = Get-Content -LiteralPath (Join-Path $projectRoot "CMakeLists.txt") -Raw
$versionMatch = [regex]::Match($cmakeText, 'project\(obs-rgb-curves VERSION ([0-9]+\.[0-9]+\.[0-9]+)')
if (-not $versionMatch.Success) {
  throw "Could not determine version from CMakeLists.txt"
}
$version = $versionMatch.Groups[1].Value
$packageName = "obs-rgb-curves-windows-v$version"
$packageRoot = Join-Path $releaseRoot $packageName
$pluginDir = Join-Path $packageRoot "obs-plugins\64bit"
$dataDir = Join-Path $packageRoot "data\obs-plugins\obs-rgb-curves\effects"
$dllPath = Join-Path $buildDir "obs-rgb-curves.dll"
$effectPath = Join-Path $projectRoot "data\effects\rgb-curves.effect"
$installScriptPath = Join-Path $packageRoot "install.ps1"
$uninstallScriptPath = Join-Path $packageRoot "uninstall.ps1"
$readmePath = Join-Path $packageRoot "README.txt"
$zipPath = Join-Path $releaseRoot ($packageName + ".zip")

if (!(Test-Path $dllPath)) {
  throw "Missing build artifact: $dllPath"
}

if (!(Test-Path $effectPath)) {
  throw "Missing effect file: $effectPath"
}

if (Test-Path $packageRoot) {
  Remove-Item -LiteralPath $packageRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $pluginDir | Out-Null
New-Item -ItemType Directory -Force -Path $dataDir | Out-Null

Copy-Item -LiteralPath $dllPath -Destination (Join-Path $pluginDir "obs-rgb-curves.dll") -Force
Copy-Item -LiteralPath $effectPath -Destination (Join-Path $dataDir "rgb-curves.effect") -Force

@'
$ErrorActionPreference = "Stop"

$pluginRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$obsRoot = "C:\Program Files\obs-studio"
$pluginDir = Join-Path $obsRoot "obs-plugins\64bit"
$dataDir = Join-Path $obsRoot "data\obs-plugins\obs-rgb-curves\effects"

if (!(Test-Path $obsRoot)) {
  throw "OBS installation not found at $obsRoot"
}

New-Item -ItemType Directory -Force -Path $pluginDir | Out-Null
New-Item -ItemType Directory -Force -Path $dataDir | Out-Null

Copy-Item -LiteralPath (Join-Path $pluginRoot "obs-plugins\64bit\obs-rgb-curves.dll") `
  -Destination (Join-Path $pluginDir "obs-rgb-curves.dll") -Force

Copy-Item -LiteralPath (Join-Path $pluginRoot "data\obs-plugins\obs-rgb-curves\effects\rgb-curves.effect") `
  -Destination (Join-Path $dataDir "rgb-curves.effect") -Force

Write-Host "obs-rgb-curves installed to $obsRoot"
'@ | Set-Content -LiteralPath $installScriptPath -Encoding ASCII

@'
$ErrorActionPreference = "Stop"

$obsRoot = "C:\Program Files\obs-studio"
$pluginPath = Join-Path $obsRoot "obs-plugins\64bit\obs-rgb-curves.dll"
$effectPath = Join-Path $obsRoot "data\obs-plugins\obs-rgb-curves\effects\rgb-curves.effect"
$effectDir = Split-Path -Parent $effectPath
$pluginDataRoot = Join-Path $obsRoot "data\obs-plugins\obs-rgb-curves"

if (Test-Path $pluginPath) {
  Remove-Item -LiteralPath $pluginPath -Force
}

if (Test-Path $effectPath) {
  Remove-Item -LiteralPath $effectPath -Force
}

if (Test-Path $effectDir) {
  Remove-Item -LiteralPath $effectDir -Force -ErrorAction SilentlyContinue
}

if (Test-Path $pluginDataRoot) {
  Remove-Item -LiteralPath $pluginDataRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "obs-rgb-curves removed from $obsRoot"
'@ | Set-Content -LiteralPath $uninstallScriptPath -Encoding ASCII

@'
OBS RGB Curves

Contents:
- obs-plugins\64bit\obs-rgb-curves.dll
- data\obs-plugins\obs-rgb-curves\effects\rgb-curves.effect
- install.ps1
- uninstall.ps1

Easy install:
1. Close OBS.
2. Run PowerShell as Administrator.
3. Run .\install.ps1 from this folder.

Requirements:
- 64-bit OBS Studio on Windows
- No separate Qt, CMake or OBS SDK install is required for normal use

Easy uninstall:
1. Close OBS.
2. Run PowerShell as Administrator.
3. Run .\uninstall.ps1 from this folder.

Manual install:
- Copy obs-plugins\64bit\obs-rgb-curves.dll to:
  C:\Program Files\obs-studio\obs-plugins\64bit\
- Copy data\obs-plugins\obs-rgb-curves\effects\rgb-curves.effect to:
  C:\Program Files\obs-studio\data\obs-plugins\obs-rgb-curves\effects\
'@ | Set-Content -LiteralPath $readmePath -Encoding ASCII

if (Test-Path $zipPath) {
  Remove-Item -LiteralPath $zipPath -Force
}

Compress-Archive -Path $packageRoot -DestinationPath $zipPath -CompressionLevel Optimal

Write-Output "PACKAGE_ROOT=$packageRoot"
Write-Output "ZIP_PATH=$zipPath"
