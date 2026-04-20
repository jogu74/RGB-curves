# ColorForge

`ColorForge` is a multi-filter color toolkit for OBS Studio.

Today it includes:

- `RGB Curves`
- `Hue Curves`
- `Color Range Correction`

On macOS, `Color Range Correction` includes a screen color picker for sampling a key color directly from the desktop.

`RGB Curves` currently includes four editable curves:

- `Neutral`: adjusts overall luminance / light levels
- `Red`
- `Green`
- `Blue`

## Current Features

`RGB Curves`

- Point-based curve editing
- Movable endpoint handles with flat edge plateaus
- Separate neutral and RGB curves
- Live effect updates in OBS while editing
- Histogram overlay from the real source behind the filter
- Channel-aware histogram view for Neutral / Red / Green / Blue
- Live preview inside the editor
- Saved curves with load, save, rename, delete, export and import

`Hue Curves`

- Hue vs Sat, Hue vs Hue and Hue vs Luma curves
- Live hue-based grading through a dedicated OBS filter
- Live editor preview and hue histogram
- Separate preset storage and JSON import / export

`Color Range Correction`

- Hue, saturation and luma range keying
- Soft-edge falloff on all three ranges
- Preview modes for final image, matte and Color / Gray isolation
- Screen color picker on macOS

## How It Works

Instead of using sliders, the filter opens a dedicated curve editor where you can:

- click in the graph to add points
- drag points to reshape the curve
- double-click or right-click a point to remove it

The neutral curve affects luminance, while the red, green and blue curves affect each color channel separately.

## Project Structure

- `src/rgb_curves_filter.*`: OBS filter integration for `RGB Curves`
- `src/hue_curves_filter.*`: OBS filter integration for `Hue Curves`
- `src/color_range_correction_filter.*`: OBS filter integration for `Color Range Correction`
- `src/curve_editor_dialog.*`: Qt editor dialog and saved-curve management
- `src/hue_curve_editor_dialog.*`: Qt editor dialog for hue-driven curves
- `src/curve_widget.*`: interactive curve canvas
- `src/curve_types.hpp`: curve math, interpolation and LUT helpers for both filter families
- `data/effects/*.effect`: shaders used by the filters
- `docs/ROADMAP.md`: planned `ColorForge` filter suite direction
- `scripts/package-release.ps1`: builds a release folder and zip package for Windows

## Building

The project expects:

- OBS Studio SDK / `libobs` headers
- Qt 6 Widgets
- CMake 3.22 or newer

### macOS

On macOS, the plugin builds as an OBS `.plugin` bundle.

The build can auto-detect locally vendored dependencies when they exist:

- `third_party/obs-studio` for `libobs` headers
- `third_party/Qt/<version>/macos` for Qt 6

Otherwise, provide:

- `libobs` headers, usually from an OBS source checkout or SDK-style export
- a Qt 6 installation with CMake package files, or a `CMAKE_PREFIX_PATH` / `Qt6_DIR` that points at them

Typical configure and build:

```bash
cmake -S . -B build \
  -DOBS_INCLUDE_DIR=/path/to/obs-studio/libobs \
  -DOBS_ROOT_INCLUDE_DIR=/path/to/obs-studio/libobs \
  -DOBS_LIB_DIR=/Applications/OBS.app/Contents/Frameworks \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/macos

cmake --build build
```

The resulting bundle is written to:

- `build/obs-colorforge.plugin`

To install it into your user plugin folder:

```bash
cmake --install build
```

This installs the bundle to:

- `~/Library/Application Support/obs-studio/plugins`

The post-build step also copies the effect files into the bundle's `Contents/Resources/effects` folder so `obs_module_file()` can find them on macOS.

### Windows

If CMake does not find OBS automatically, set the environment variables before configuring:

```powershell
$env:OBS_INCLUDE_DIR="C:\path\to\obs-studio\libobs"
$env:OBS_LIB_DIR="C:\path\to\obs-studio\build\rundir\RelWithDebInfo\obs-plugins\64bit"
cmake -S . -B build
cmake --build build --config RelWithDebInfo
```

## Packaging

To create a Windows release package:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-release.ps1
```

This creates:

- a versioned release folder under `release\`
- a versioned zip package ready to share or install
- a packaged Windows release for all three ColorForge filters

To create a macOS release package:

```bash
./scripts/package-release-mac.sh
```

This creates:

- a versioned release folder under `release/`
- a versioned zip package containing `obs-colorforge.plugin`
- `Install ColorForge.command` / `Uninstall ColorForge.command` for Finder-friendly installs on macOS
- matching shell scripts for terminal-based installs
- the bundled `ColorForgeIcon.png`

To create a macOS installer package:

```bash
./scripts/package-installer-mac.sh
```

This creates:

- a versioned installer package such as `release/colorforge-macos-installer-v0.3.0.pkg`
- a staging folder under `release/` with the generated installer assets
- generated `.icns` / `.ico` icon files from `assets/icon/ColorForgeIcon.png`

By default the `.pkg` is unsigned. If you have an Apple installer certificate, set:

```bash
COLORFORGE_INSTALLER_SIGN_IDENTITY="Developer ID Installer: Your Name" ./scripts/package-installer-mac.sh
```

To create a Windows installer package:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-installer-windows.ps1
```

This creates:

- a versioned Windows installer `.exe` in `release\`
- a standard installer wizard with uninstall support
- an installer branded with the `ColorForge` icon
- upgrade cleanup for older standalone `RGB Curves` installs

For distribution, use separate zip files for Windows and macOS.

- Windows and macOS need different binaries
- the folder layout and installer scripts differ
- a single combined zip is possible, but it is messier and easier for users to install incorrectly
- the macOS installer clears the quarantine flag so Gatekeeper is less likely to mark the plugin bundle as damaged

## Runtime Requirements

For normal use on Windows, the plugin does not require a separate Qt, CMake or OBS SDK installation.

End users only need:

- OBS Studio 32-bit? No, `ColorForge` is built for `64-bit OBS Studio on Windows`
- permission to copy files into the OBS installation folder, or to run the included installer script as Administrator

In practice, if OBS itself runs correctly on the machine, the plugin should not need any extra developer tools or SDKs.

## Installation

The packaged Windows release includes:

- `ColorForgeIcon.png`
- `Install ColorForge.bat`
- `install.ps1`
- `Uninstall ColorForge.bat`
- `uninstall.ps1`
- the correct OBS plugin folder structure
- all three ColorForge filters in one plugin package

Windows note:
- The screen color picker is currently disabled on Windows.

Recommended install:

1. Close OBS.
2. Extract the release zip.
3. Double-click `Install ColorForge.bat`.
4. Approve the admin prompt if Windows asks.

For manual installation, copy:

- `obs-plugins\64bit\obs-colorforge.dll` to:
  `C:\Program Files\obs-studio\obs-plugins\64bit\`
- `data\obs-plugins\obs-colorforge\effects\rgb-curves.effect` to:
  `C:\Program Files\obs-studio\data\obs-plugins\obs-colorforge\effects\`
- `data\obs-plugins\obs-colorforge\effects\hue-curves.effect` to:
  `C:\Program Files\obs-studio\data\obs-plugins\obs-colorforge\effects\`
- `data\obs-plugins\obs-colorforge\effects\color-range-correction.effect` to:
  `C:\Program Files\obs-studio\data\obs-plugins\obs-colorforge\effects\`

If OBS is installed in a different location, copy the same files into the matching plugin and data folders for that installation.

If you are upgrading from the older standalone `RGB Curves` plugin, the Windows installer and packaged install scripts now remove the legacy files during installation.
