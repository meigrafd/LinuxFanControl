#!/usr/bin/env bash
set -euo pipefail

###############################################################################
# Build script for LinuxFanControl
# - Builds C++ Daemon via CMake/Ninja
# - Builds Avalonia GUI via dotnet publish
# - Copies Locales & Themes dynamically
###############################################################################

# Default settings
GUI_CONFIG=${GUI_CONFIG:-Release}
GUI_RID=${GUI_RID:-linux-x64}
ARTIFACTS=${ARTIFACTS:-artifacts}
BUILD_DIR=${BUILD_DIR:-build}

# Control flags
BUILD_DAEMON=true
BUILD_GUI=true

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
  case "$1" in
    --gui-only)
      BUILD_DAEMON=false
      BUILD_GUI=true
      ;;
    --daemon-only)
      BUILD_DAEMON=true
      BUILD_GUI=false
      ;;
    --gui-config)
      GUI_CONFIG="$2"
      shift
      ;;
    --gui-rid)
      GUI_RID="$2"
      shift
      ;;
    *)
      echo "[!] Unbekannte Option: $1" >&2
      exit 1
      ;;
  esac
  shift
done

echo "[i] Artifacts directory: $ARTIFACTS"
mkdir -p "$ARTIFACTS"

###############################################################################
# Build Daemon
###############################################################################
if [[ "$BUILD_DAEMON" == true ]]; then
  echo "[i] Building Daemon…"
  mkdir -p "$BUILD_DIR/daemon"
  cd "$BUILD_DIR/daemon"

  cmake "../../src/daemon" -G Ninja
  ninja

  echo "[i] Installing Daemon binary…"
  cp "daemon_lfc" "../../$ARTIFACTS/"
  cd ../../
fi

###############################################################################
# Build GUI
###############################################################################
if [[ "$BUILD_GUI" == true ]]; then
  echo "[i] GUI config      : $GUI_CONFIG"
  echo "[i] GUI RID         : $GUI_RID"
  echo "[i] Publishing GUI…"

  dotnet publish src/gui-avalonia/LinuxFanControl.Gui/LinuxFanControl.Gui.csproj \
    -c "$GUI_CONFIG" \
    -r "$GUI_RID" \
    --no-self-contained \
    -o "$ARTIFACTS/gui"

  echo "[i] Copying Locales & Themes…"
  mkdir -p "$ARTIFACTS/gui/Assets/Locales" "$ARTIFACTS/gui/Assets/Themes"
  cp -r src/gui-avalonia/LinuxFanControl.Gui/Locales/*  "$ARTIFACTS/gui/Assets/Locales/"
  cp -r src/gui-avalonia/LinuxFanControl.Gui/Themes/*   "$ARTIFACTS/gui/Assets/Themes/"
fi

echo "[i] Build complete."
