$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot "build\RelWithDebInfo"
$releaseRoot = Join-Path $projectRoot "release"
$issPath = Join-Path $projectRoot "installer\windows\ColorForge.iss"
$iconPng = Join-Path $projectRoot "assets\icon\ColorForgeIcon.png"
$iconIco = Join-Path $projectRoot "assets\icon\ColorForgeIcon.ico"
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

& $python (Join-Path $projectRoot "scripts\generate-release-icons.py") $iconPng --ico $iconIco

$iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
  $userCandidates = @()
  if ($env:LOCALAPPDATA) {
    $userCandidates += (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe")
  }
  if ($env:USERPROFILE) {
    $userCandidates += (Join-Path $env:USERPROFILE "AppData\Local\Programs\Inno Setup 6\ISCC.exe")
  }

  $commonCandidates = @(
    $userCandidates
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    "C:\Program Files\Inno Setup 6\ISCC.exe"
  ) | Select-Object -Unique

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
