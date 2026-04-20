$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot "build\RelWithDebInfo"
$releaseRoot = Join-Path $projectRoot "release"
$cmakeText = Get-Content -LiteralPath (Join-Path $projectRoot "CMakeLists.txt") -Raw
$versionMatch = [regex]::Match($cmakeText, 'project\(obs-colorforge VERSION ([0-9]+\.[0-9]+\.[0-9]+)')
if (-not $versionMatch.Success) {
  throw "Could not determine version from CMakeLists.txt"
}
$version = $versionMatch.Groups[1].Value
$packageName = "colorforge-windows-v$version"
$packageRoot = Join-Path $releaseRoot $packageName
$pluginDir = Join-Path $packageRoot "obs-plugins\64bit"
$dataDir = Join-Path $packageRoot "data\obs-plugins\obs-colorforge\effects"
$iconPath = Join-Path $projectRoot "assets\icon\ColorForgeIcon.png"
$dllPath = Join-Path $buildDir "obs-colorforge.dll"
$rgbEffectPath = Join-Path $projectRoot "data\effects\rgb-curves.effect"
$hueEffectPath = Join-Path $projectRoot "data\effects\hue-curves.effect"
$colorRangeEffectPath = Join-Path $projectRoot "data\effects\color-range-correction.effect"
$installScriptPath = Join-Path $packageRoot "install.ps1"
$installLauncherPath = Join-Path $packageRoot "Install ColorForge.bat"
$uninstallScriptPath = Join-Path $packageRoot "uninstall.ps1"
$uninstallLauncherPath = Join-Path $packageRoot "Uninstall ColorForge.bat"
$readmePath = Join-Path $packageRoot "README.txt"
$zipPath = Join-Path $releaseRoot ($packageName + ".zip")

if (!(Test-Path $dllPath)) {
  throw "Missing build artifact: $dllPath"
}

if (!(Test-Path $rgbEffectPath)) {
  throw "Missing effect file: $rgbEffectPath"
}

if (!(Test-Path $hueEffectPath)) {
  throw "Missing effect file: $hueEffectPath"
}

if (!(Test-Path $colorRangeEffectPath)) {
  throw "Missing effect file: $colorRangeEffectPath"
}

if (Test-Path $packageRoot) {
  Remove-Item -LiteralPath $packageRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $pluginDir | Out-Null
New-Item -ItemType Directory -Force -Path $dataDir | Out-Null

Copy-Item -LiteralPath $dllPath -Destination (Join-Path $pluginDir "obs-colorforge.dll") -Force
Copy-Item -LiteralPath $rgbEffectPath -Destination (Join-Path $dataDir "rgb-curves.effect") -Force
Copy-Item -LiteralPath $hueEffectPath -Destination (Join-Path $dataDir "hue-curves.effect") -Force
Copy-Item -LiteralPath $colorRangeEffectPath -Destination (Join-Path $dataDir "color-range-correction.effect") -Force
if (Test-Path $iconPath) {
  Copy-Item -LiteralPath $iconPath -Destination (Join-Path $packageRoot "ColorForgeIcon.png") -Force
}

@'
param(
  [string]$ObsRoot
)

$ErrorActionPreference = "Stop"

function Test-Administrator {
  $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
  $principal = New-Object Security.Principal.WindowsPrincipal($identity)
  return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Remove-LegacyRgbCurves([string]$ResolvedObsRoot) {
  $legacyPluginPaths = @(
    (Join-Path $ResolvedObsRoot "obs-plugins\64bit\rgb-curves.dll"),
    (Join-Path $ResolvedObsRoot "obs-plugins\64bit\obs-rgb-curves.dll")
  )
  $legacyDataRoots = @(
    (Join-Path $ResolvedObsRoot "data\obs-plugins\rgb-curves"),
    (Join-Path $ResolvedObsRoot "data\obs-plugins\obs-rgb-curves")
  )

  foreach ($path in $legacyPluginPaths) {
    if (Test-Path $path) {
      Remove-Item -LiteralPath $path -Force
    }
  }

  foreach ($path in $legacyDataRoots) {
    if (Test-Path $path) {
      Remove-Item -LiteralPath $path -Recurse -Force -ErrorAction SilentlyContinue
    }
  }
}

function Resolve-ObsRoot([string]$OverridePath) {
  $candidates = @()

  if ($OverridePath) {
    $candidates += $OverridePath
  }

  if ($env:ProgramFiles) {
    $candidates += (Join-Path $env:ProgramFiles "obs-studio")
  }

  if ($env:ProgramFiles -and $env:ProgramFiles -ne ${env:ProgramFiles(x86)} -and ${env:ProgramFiles(x86)}) {
    $candidates += (Join-Path ${env:ProgramFiles(x86)} "obs-studio")
  }

  foreach ($candidate in $candidates | Select-Object -Unique) {
    if ((Test-Path (Join-Path $candidate "bin\64bit\obs64.exe")) -or (Test-Path (Join-Path $candidate "obs-plugins\64bit"))) {
      return $candidate
    }
  }

  throw "OBS installation not found. Install OBS first or rerun with -ObsRoot `"C:\path\to\obs-studio`"."
}

if (-not (Test-Administrator)) {
  $arguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath)
  if ($ObsRoot) {
    $arguments += @("-ObsRoot", $ObsRoot)
  }

  Start-Process -FilePath "powershell" -Verb RunAs -ArgumentList $arguments | Out-Null
  exit
}

$pluginRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$obsRoot = Resolve-ObsRoot $ObsRoot
$pluginDir = Join-Path $obsRoot "obs-plugins\64bit"
$dataDir = Join-Path $obsRoot "data\obs-plugins\obs-colorforge\effects"

Remove-LegacyRgbCurves $obsRoot

New-Item -ItemType Directory -Force -Path $pluginDir | Out-Null
New-Item -ItemType Directory -Force -Path $dataDir | Out-Null

Copy-Item -LiteralPath (Join-Path $pluginRoot "obs-plugins\64bit\obs-colorforge.dll") `
  -Destination (Join-Path $pluginDir "obs-colorforge.dll") -Force

Copy-Item -LiteralPath (Join-Path $pluginRoot "data\obs-plugins\obs-colorforge\effects\rgb-curves.effect") `
  -Destination (Join-Path $dataDir "rgb-curves.effect") -Force

Copy-Item -LiteralPath (Join-Path $pluginRoot "data\obs-plugins\obs-colorforge\effects\hue-curves.effect") `
  -Destination (Join-Path $dataDir "hue-curves.effect") -Force

Copy-Item -LiteralPath (Join-Path $pluginRoot "data\obs-plugins\obs-colorforge\effects\color-range-correction.effect") `
  -Destination (Join-Path $dataDir "color-range-correction.effect") -Force

Write-Host "ColorForge installed to $obsRoot"
'@ | Set-Content -LiteralPath $installScriptPath -Encoding ASCII

@'
param(
  [string]$ObsRoot
)

$ErrorActionPreference = "Stop"

function Test-Administrator {
  $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
  $principal = New-Object Security.Principal.WindowsPrincipal($identity)
  return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Remove-LegacyRgbCurves([string]$ResolvedObsRoot) {
  $legacyPluginPaths = @(
    (Join-Path $ResolvedObsRoot "obs-plugins\64bit\rgb-curves.dll"),
    (Join-Path $ResolvedObsRoot "obs-plugins\64bit\obs-rgb-curves.dll")
  )
  $legacyDataRoots = @(
    (Join-Path $ResolvedObsRoot "data\obs-plugins\rgb-curves"),
    (Join-Path $ResolvedObsRoot "data\obs-plugins\obs-rgb-curves")
  )

  foreach ($path in $legacyPluginPaths) {
    if (Test-Path $path) {
      Remove-Item -LiteralPath $path -Force
    }
  }

  foreach ($path in $legacyDataRoots) {
    if (Test-Path $path) {
      Remove-Item -LiteralPath $path -Recurse -Force -ErrorAction SilentlyContinue
    }
  }
}

function Resolve-ObsRoot([string]$OverridePath) {
  $candidates = @()

  if ($OverridePath) {
    $candidates += $OverridePath
  }

  if ($env:ProgramFiles) {
    $candidates += (Join-Path $env:ProgramFiles "obs-studio")
  }

  if ($env:ProgramFiles -and $env:ProgramFiles -ne ${env:ProgramFiles(x86)} -and ${env:ProgramFiles(x86)}) {
    $candidates += (Join-Path ${env:ProgramFiles(x86)} "obs-studio")
  }

  foreach ($candidate in $candidates | Select-Object -Unique) {
    if (Test-Path $candidate) {
      return $candidate
    }
  }

  throw "OBS installation not found. Install OBS first or rerun with -ObsRoot `"C:\path\to\obs-studio`"."
}

if (-not (Test-Administrator)) {
  $arguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath)
  if ($ObsRoot) {
    $arguments += @("-ObsRoot", $ObsRoot)
  }

  Start-Process -FilePath "powershell" -Verb RunAs -ArgumentList $arguments | Out-Null
  exit
}

$obsRoot = Resolve-ObsRoot $ObsRoot
$pluginPath = Join-Path $obsRoot "obs-plugins\64bit\obs-colorforge.dll"
$rgbEffectPath = Join-Path $obsRoot "data\obs-plugins\obs-colorforge\effects\rgb-curves.effect"
$hueEffectPath = Join-Path $obsRoot "data\obs-plugins\obs-colorforge\effects\hue-curves.effect"
$colorRangeEffectPath = Join-Path $obsRoot "data\obs-plugins\obs-colorforge\effects\color-range-correction.effect"
$effectDir = Split-Path -Parent $rgbEffectPath
$pluginDataRoot = Join-Path $obsRoot "data\obs-plugins\obs-colorforge"

if (Test-Path $pluginPath) {
  Remove-Item -LiteralPath $pluginPath -Force
}

if (Test-Path $rgbEffectPath) {
  Remove-Item -LiteralPath $rgbEffectPath -Force
}

if (Test-Path $hueEffectPath) {
  Remove-Item -LiteralPath $hueEffectPath -Force
}

if (Test-Path $colorRangeEffectPath) {
  Remove-Item -LiteralPath $colorRangeEffectPath -Force
}

if (Test-Path $effectDir) {
  Remove-Item -LiteralPath $effectDir -Force -ErrorAction SilentlyContinue
}

if (Test-Path $pluginDataRoot) {
  Remove-Item -LiteralPath $pluginDataRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Remove-LegacyRgbCurves $obsRoot

Write-Host "ColorForge removed from $obsRoot"
'@ | Set-Content -LiteralPath $uninstallScriptPath -Encoding ASCII

@'
@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1"
echo.
pause
'@ | Set-Content -LiteralPath $installLauncherPath -Encoding ASCII

@'
@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall.ps1"
echo.
pause
'@ | Set-Content -LiteralPath $uninstallLauncherPath -Encoding ASCII

@'
ColorForge

ColorForge is a multi-filter color toolkit for OBS Studio.

Included in this Windows package:
- RGB Curves
- Hue Curves
- Color Range Correction

Contents:
- ColorForgeIcon.png
- obs-plugins\64bit\obs-colorforge.dll
- data\obs-plugins\obs-colorforge\effects\rgb-curves.effect
- data\obs-plugins\obs-colorforge\effects\hue-curves.effect
- data\obs-plugins\obs-colorforge\effects\color-range-correction.effect
- Install ColorForge.bat
- install.ps1
- Uninstall ColorForge.bat
- uninstall.ps1

Easy install:
1. Close OBS.
2. Double-click Install ColorForge.bat.
3. Approve the admin prompt if Windows asks.

Upgrade note:
- Older standalone RGB Curves plugin files are removed automatically during install.

Windows note:
- The screen color picker is currently disabled on Windows.

Requirements:
- 64-bit OBS Studio on Windows
- No separate Qt, CMake or OBS SDK install is required for normal use

Easy uninstall:
1. Close OBS.
2. Double-click Uninstall ColorForge.bat.
3. Approve the admin prompt if Windows asks.

Manual install:
- Copy obs-plugins\64bit\obs-colorforge.dll to:
  C:\Program Files\obs-studio\obs-plugins\64bit\
- Copy data\obs-plugins\obs-colorforge\effects\rgb-curves.effect to:
  C:\Program Files\obs-studio\data\obs-plugins\obs-colorforge\effects\
- Copy data\obs-plugins\obs-colorforge\effects\hue-curves.effect to:
  C:\Program Files\obs-studio\data\obs-plugins\obs-colorforge\effects\
- Copy data\obs-plugins\obs-colorforge\effects\color-range-correction.effect to:
  C:\Program Files\obs-studio\data\obs-plugins\obs-colorforge\effects\
'@ | Set-Content -LiteralPath $readmePath -Encoding ASCII

if (Test-Path $zipPath) {
  Remove-Item -LiteralPath $zipPath -Force
}

Compress-Archive -Path $packageRoot -DestinationPath $zipPath -CompressionLevel Optimal

Write-Output "PACKAGE_ROOT=$packageRoot"
Write-Output "ZIP_PATH=$zipPath"
