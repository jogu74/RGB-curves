#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$PROJECT_ROOT/build"
RELEASE_ROOT="$PROJECT_ROOT/release"
CMAKE_FILE="$PROJECT_ROOT/CMakeLists.txt"
PLUGIN_BUNDLE="$BUILD_DIR/obs-colorforge.plugin"
ICON_PNG="$PROJECT_ROOT/assets/icon/ColorForgeIcon.png"
ICON_ICNS="$PROJECT_ROOT/assets/icon/ColorForgeIcon.icns"
ICON_ICO="$PROJECT_ROOT/assets/icon/ColorForgeIcon.ico"
SIGN_IDENTITY="${COLORFORGE_INSTALLER_SIGN_IDENTITY:-}"

VERSION=$(sed -n 's/.*project(obs-colorforge VERSION \([0-9][0-9.]*\) LANGUAGES CXX).*/\1/p' "$CMAKE_FILE")
if [ -z "$VERSION" ]; then
  echo "Could not determine version from CMakeLists.txt" >&2
  exit 1
fi

if [ ! -d "$PLUGIN_BUNDLE" ]; then
  echo "Missing build artifact: $PLUGIN_BUNDLE" >&2
  exit 1
fi

if [ ! -f "$ICON_PNG" ]; then
  echo "Missing icon source: $ICON_PNG" >&2
  exit 1
fi

python3 "$SCRIPT_DIR/generate-release-icons.py" "$ICON_PNG" --icns "$ICON_ICNS" --ico "$ICON_ICO"

PACKAGE_NAME="colorforge-macos-installer-v$VERSION"
PACKAGE_ROOT="$RELEASE_ROOT/$PACKAGE_NAME"
PAYLOAD_ROOT="$PACKAGE_ROOT/payload"
SCRIPTS_ROOT="$PACKAGE_ROOT/scripts"
STAGING_ROOT="$PAYLOAD_ROOT/private/tmp/ColorForgeInstaller"
COMPONENT_PKG="$PACKAGE_ROOT/ColorForge-component.pkg"
FINAL_PKG="$RELEASE_ROOT/$PACKAGE_NAME.pkg"
README_PATH="$PACKAGE_ROOT/README.txt"

rm -rf "$PACKAGE_ROOT"
mkdir -p "$STAGING_ROOT" "$SCRIPTS_ROOT"
cp -R "$PLUGIN_BUNDLE" "$STAGING_ROOT/obs-colorforge.plugin"
cp "$ICON_PNG" "$PACKAGE_ROOT/ColorForgeIcon.png"

cat >"$SCRIPTS_ROOT/postinstall" <<'EOF'
#!/bin/sh

set -eu

CONSOLE_USER=$(stat -f%Su /dev/console)
if [ -z "$CONSOLE_USER" ] || [ "$CONSOLE_USER" = "root" ]; then
  echo "Could not determine the logged-in macOS user." >&2
  exit 1
fi

USER_HOME=$(dscl . -read "/Users/$CONSOLE_USER" NFSHomeDirectory | awk '{print $2}')
if [ -z "$USER_HOME" ] || [ ! -d "$USER_HOME" ]; then
  echo "Could not determine home folder for $CONSOLE_USER." >&2
  exit 1
fi

SOURCE_ROOT="/private/tmp/ColorForgeInstaller"
SOURCE_PLUGIN="$SOURCE_ROOT/obs-colorforge.plugin"
TARGET_DIR="$USER_HOME/Library/Application Support/obs-studio/plugins"
TARGET_PLUGIN="$TARGET_DIR/obs-colorforge.plugin"

if [ ! -d "$SOURCE_PLUGIN" ]; then
  echo "Missing staged plugin bundle: $SOURCE_PLUGIN" >&2
  exit 1
fi

mkdir -p "$TARGET_DIR"
rm -rf "$TARGET_PLUGIN"

if command -v xattr >/dev/null 2>&1; then
  xattr -dr com.apple.quarantine "$SOURCE_PLUGIN" 2>/dev/null || true
fi

/usr/bin/ditto "$SOURCE_PLUGIN" "$TARGET_PLUGIN"
chown -R "$CONSOLE_USER":staff "$TARGET_PLUGIN"

if command -v xattr >/dev/null 2>&1; then
  xattr -dr com.apple.quarantine "$TARGET_PLUGIN" 2>/dev/null || true
fi

rm -rf "$SOURCE_ROOT"

echo "Installed ColorForge for user $CONSOLE_USER"
EOF

chmod +x "$SCRIPTS_ROOT/postinstall"

cat >"$README_PATH" <<EOF
ColorForge macOS Installer

Contents:
- ColorForgeIcon.png
- ColorForge-component.pkg

Final installer:
- $(basename "$FINAL_PKG")

What it does:
- Installs ColorForge into the currently logged-in user's OBS plugin folder
- Clears the macOS quarantine flag on the copied plugin bundle

Install path:
$HOME/Library/Application Support/obs-studio/plugins/obs-colorforge.plugin
EOF

pkgbuild \
  --root "$PAYLOAD_ROOT" \
  --identifier "se.colorforge.obs.plugin" \
  --version "$VERSION" \
  --scripts "$SCRIPTS_ROOT" \
  "$COMPONENT_PKG"

rm -f "$FINAL_PKG"
if [ -n "$SIGN_IDENTITY" ]; then
  productbuild \
    --sign "$SIGN_IDENTITY" \
    --package "$COMPONENT_PKG" \
    "$FINAL_PKG"
else
  productbuild \
    --package "$COMPONENT_PKG" \
    "$FINAL_PKG"
fi

echo "PACKAGE_ROOT=$PACKAGE_ROOT"
echo "PKG_PATH=$FINAL_PKG"
