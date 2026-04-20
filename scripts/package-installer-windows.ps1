$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot "build\RelWithDebInfo"
$releaseRoot = Join-Path $projectRoot "release"
$issPath = Join-Path $projectRoot "installer\windows\ColorForge.iss"
$iconPng = Join-Path $projectRoot "assets\icon\ColorForgeIcon.png"
$iconIco = Join-Path $projectRoot "assets\icon\ColorForgeIcon.ico"
$iconIcns = Join-Path $projectRoot "assets\icon\ColorForgeIcon.icns"
$cmakeText = Get-Content -LiteralPath (Join-Path $projectRoot "CMakeLists.txt") -Raw
$versionMatch = [regex]::Match($cmakeText, 'project\(obs-colorforge VERSION ([0-9]+\.[0-9]+\.[0-9]+)')
if (-not $versionMatch.Success) {
  throw "Could not determine version from CMakeLists.txt"
}

$version = $versionMatch.Groups[1].Value
$dllPath = Join-Path $buildDir "obs-colorforge.dll"
if (!(Test-Path $dllPath)) {
  throw "Missing build artifact: $dllPath"
}

if (!(Test-Path $iconPng)) {
  throw "Missing icon source: $iconPng"
}

$python = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $python) {
  $python = (Get-Command python3 -ErrorAction SilentlyContinue).Source
}
if (-not $python) {
  throw "Python is required to generate the installer icons"
}

& $python (Join-Path $projectRoot "scripts\generate-release-icons.py") $iconPng --icns $iconIcns --ico $iconIco

$iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
  $commonCandidates = @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
  )

  foreach ($candidate in $commonCandidates) {
    if (Test-Path $candidate) {
      $iscc = $candidate
      break
    }
  }
}

if (-not $iscc) {
  throw "Inno Setup compiler not found. Install Inno Setup 6 or add ISCC.exe to PATH."
}

New-Item -ItemType Directory -Force -Path $releaseRoot | Out-Null

& $iscc `
  "/DMyAppVersion=$version" `
  "/DMyProjectDir=$projectRoot" `
  "/DMyBuildDir=$buildDir" `
  "/DMyOutputDir=$releaseRoot" `
  "/DMyIconFile=$iconIco" `
  $issPath

Write-Output "OUTPUT_DIR=$releaseRoot"
