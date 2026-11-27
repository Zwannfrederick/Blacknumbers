#!/usr/bin/env bash
# Plays BLACK NUMBERS standalone (no editor UI), fullscreen, on the NVIDIA GPU when prime-run exists.
# Builds the editor target first so the binaries always match the source.
# Usage: Tools/play.sh [extra engine args, e.g. -windowed -ResX=1600 -ResY=900]
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE:-$HOME/UnrealEngine}"

BUILD_LOG="$PROJECT_DIR/Saved/Logs/play_build.log"
mkdir -p "$(dirname "$BUILD_LOG")"
if ! "$UE/Engine/Build/BatchFiles/Linux/Build.sh" BlackNumbersEditor Linux Development \
  -Project="$PROJECT_DIR/BlackNumbers.uproject" -WaitMutex > "$BUILD_LOG" 2>&1; then
  grep -E "error" "$BUILD_LOG" | head -20
  echo "BUILD FAILED (full log: $BUILD_LOG)"
  exit 1
fi

RUNNER=()
command -v prime-run > /dev/null && RUNNER=(prime-run)
exec "${RUNNER[@]}" "$UE/Engine/Binaries/Linux/UnrealEditor" "$PROJECT_DIR/BlackNumbers.uproject" \
  -game -fullscreen -nosplash "$@"
