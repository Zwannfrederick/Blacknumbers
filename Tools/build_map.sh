#!/usr/bin/env bash
# Regenerates /Game/Maps/TestTrack and /Game/World materials from Tools/build_test_map.py.
# Refuses to run while an Unreal editor is open (it would hold the map and lose or clash with edits).
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE:-$HOME/UnrealEngine}"

if pgrep -x UnrealEditor > /dev/null; then
  echo "Unreal Editor is open: close it before regenerating the map." >&2
  exit 2
fi

"$UE/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/BlackNumbers.uproject" -run=pythonscript \
  -script="$PROJECT_DIR/Tools/build_test_map.py" -unattended -nullrhi -nosplash -NoSound > /dev/null 2>&1
status=$?

LOG="$PROJECT_DIR/Saved/Logs/BlackNumbers.log"
grep -E "LogPython: Error|Python script executed|BN: built" "$LOG" | cut -c31-
exit $status
