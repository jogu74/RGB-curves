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
PACKAGE_NAME="obs-rgb-curves-macos-v$VERSION"
PACKAGE_ROOT="$RELEASE_ROOT/$PACKAGE_NAME"
ZIP_PATH="$RELEASE_ROOT/$PACKAGE_NAME.zip"
PLUGIN_DEST="$PACKAGE_ROOT/obs-rgb-curves.plugin"
INSTALL_SCRIPT="$PACKAGE_ROOT/install.sh"
UNINSTALL_SCRIPT="$PACKAGE_ROOT/uninstall.sh"
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
cp -R "$SOURCE_PLUGIN" "$TARGET_PLUGIN"

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

chmod +x "$INSTALL_SCRIPT" "$UNINSTALL_SCRIPT"

cat >"$README_PATH" <<EOF
OBS RGB Curves for macOS

Contents:
- obs-rgb-curves.plugin
- install.sh
- uninstall.sh

Easy install:
1. Close OBS.
2. Run ./install.sh from this folder.
3. Start OBS.

This installs the plugin to:
$HOME/Library/Application Support/obs-studio/plugins

Easy uninstall:
1. Close OBS.
2. Run ./uninstall.sh from this folder.
EOF

rm -f "$ZIP_PATH"
(
  cd "$RELEASE_ROOT"
  /usr/bin/zip -qry "$ZIP_PATH" "$PACKAGE_NAME"
)

echo "PACKAGE_ROOT=$PACKAGE_ROOT"
echo "ZIP_PATH=$ZIP_PATH"
