#!/usr/bin/env bash
# Regenerates PBR textures, vehicle template imports, /Game/World materials,
# /Game/Maps/TestTrack, and /Game/Maps/KaramaraSlice.
# Refuses to run while an Unreal editor is open (it would hold the map and lose or clash with edits).
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE:-$HOME/UnrealEngine}"

if pgrep -x UnrealEditor > /dev/null; then
  echo "Unreal Editor is open: close it before regenerating the map." >&2
  exit 2
fi

# Generate seamless 1024x1024 PBR PNGs, Most Wanted UI PNGs, and stage UE template vehicle files.
python3 "$PROJECT_DIR/Tools/bn_pbr_textures.py" --generate-only || exit $?
python3 "$PROJECT_DIR/Tools/bn_ui_textures.py" --generate-only || exit $?
python3 "$PROJECT_DIR/Tools/import_ue_vehicles.py" --stage-files || exit $?

status=0
for script in bn_ui_textures.py import_ue_vehicles.py build_test_map.py build_karamara_map.py; do
  "$UE/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/BlackNumbers.uproject" -run=pythonscript \
    -script="$PROJECT_DIR/Tools/$script" -unattended -nullrhi -nosplash -NoSound > /dev/null 2>&1
  s=$?
  LOG="$PROJECT_DIR/Saved/Logs/BlackNumbers.log"
  grep -E "LogPython: Error|Python script executed|BN: built|BN: verified|BN: ensured" "$LOG" | cut -c31-
  if [ "$s" -ne 0 ]; then
    status=$s
  fi
done
exit $status
