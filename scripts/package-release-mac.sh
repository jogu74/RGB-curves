#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$PROJECT_ROOT/build"
RELEASE_ROOT="$PROJECT_ROOT/release"
CMAKE_FILE="$PROJECT_ROOT/CMakeLists.txt"

VERSION=$(sed -n 's/.*project(obs-rgb-curves VERSION \([0-9][0-9.]*\) LANGUAGES CXX).*/\1/p' "$CMAKE_FILE")
if [ -z "$VERSION" ]; then
  echo "Could not determine version from CMakeLists.txt" >&2
  exit 1
fi

PLUGIN_BUNDLE="$BUILD_DIR/obs-rgb-curves.plugin"
PACKAGE_NAME="rgb-curves-macos-v$VERSION"
PACKAGE_ROOT="$RELEASE_ROOT/$PACKAGE_NAME"
ZIP_PATH="$RELEASE_ROOT/$PACKAGE_NAME.zip"
PLUGIN_DEST="$PACKAGE_ROOT/obs-rgb-curves.plugin"
INSTALL_SCRIPT="$PACKAGE_ROOT/install.sh"
INSTALL_COMMAND="$PACKAGE_ROOT/install.command"
UNINSTALL_SCRIPT="$PACKAGE_ROOT/uninstall.sh"
UNINSTALL_COMMAND="$PACKAGE_ROOT/uninstall.command"
README_PATH="$PACKAGE_ROOT/README.txt"

if [ ! -d "$PLUGIN_BUNDLE" ]; then
  echo "Missing build artifact: $PLUGIN_BUNDLE" >&2
  exit 1
fi

rm -rf "$PACKAGE_ROOT"
mkdir -p "$PACKAGE_ROOT"
cp -R "$PLUGIN_BUNDLE" "$PLUGIN_DEST"

cat >"$INSTALL_SCRIPT" <<'EOF'
#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PLUGIN_NAME="obs-rgb-curves.plugin"
SOURCE_PLUGIN="$SCRIPT_DIR/$PLUGIN_NAME"
OBS_PLUGIN_DIR="$HOME/Library/Application Support/obs-studio/plugins"
TARGET_PLUGIN="$OBS_PLUGIN_DIR/$PLUGIN_NAME"

if [ ! -d "$SOURCE_PLUGIN" ]; then
  echo "Missing plugin bundle: $SOURCE_PLUGIN" >&2
  exit 1
fi

mkdir -p "$OBS_PLUGIN_DIR"
rm -rf "$TARGET_PLUGIN"

if command -v xattr >/dev/null 2>&1; then
  xattr -dr com.apple.quarantine "$SOURCE_PLUGIN" 2>/dev/null || true
fi

/usr/bin/ditto "$SOURCE_PLUGIN" "$TARGET_PLUGIN"

if command -v xattr >/dev/null 2>&1; then
  xattr -dr com.apple.quarantine "$TARGET_PLUGIN" 2>/dev/null || true
fi

echo "Installed $PLUGIN_NAME to $OBS_PLUGIN_DIR"
EOF

cat >"$UNINSTALL_SCRIPT" <<'EOF'
#!/bin/sh

set -eu

PLUGIN_NAME="obs-rgb-curves.plugin"
TARGET_PLUGIN="$HOME/Library/Application Support/obs-studio/plugins/$PLUGIN_NAME"

if [ -e "$TARGET_PLUGIN" ]; then
  rm -rf "$TARGET_PLUGIN"
  echo "Removed $TARGET_PLUGIN"
else
  echo "Plugin not installed: $TARGET_PLUGIN"
fi
EOF

cp "$INSTALL_SCRIPT" "$INSTALL_COMMAND"
cp "$UNINSTALL_SCRIPT" "$UNINSTALL_COMMAND"

chmod +x "$INSTALL_SCRIPT" "$INSTALL_COMMAND" "$UNINSTALL_SCRIPT" "$UNINSTALL_COMMAND"

cat >"$README_PATH" <<EOF
RGB Curves for macOS

Contents:
- obs-rgb-curves.plugin
- install.command
- install.sh
- uninstall.command
- uninstall.sh

Easy install:
1. Close OBS.
2. Double-click install.command.
3. Start OBS.

This installs the plugin to:
$HOME/Library/Application Support/obs-studio/plugins

Notes:
- Do not double-click obs-rgb-curves.plugin directly.
- install.command removes the macOS quarantine flag if the zip was downloaded from the internet.

Easy uninstall:
1. Close OBS.
2. Double-click uninstall.command.
EOF

rm -f "$ZIP_PATH"
(
  cd "$RELEASE_ROOT"
  /usr/bin/zip -qry "$ZIP_PATH" "$PACKAGE_NAME"
)

echo "PACKAGE_ROOT=$PACKAGE_ROOT"
echo "ZIP_PATH=$ZIP_PATH"
