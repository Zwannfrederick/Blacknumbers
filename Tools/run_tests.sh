#!/usr/bin/env bash
# Builds the editor target and runs the BlackNumbers automation tests headless.
# Usage: Tools/run_tests.sh [test filter, default BlackNumbers] [extra editor args, e.g. -BENCHMARK -FPS=30]
# Exit code is non-zero if the build or any test fails. Full log: Saved/Logs/BlackNumbers.log
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE:-$HOME/UnrealEngine}"
FILTER="${1:-BlackNumbers}"
shift || true

"$UE/Engine/Build/BatchFiles/Linux/Build.sh" BlackNumbersEditor Linux Development \
  -Project="$PROJECT_DIR/BlackNumbers.uproject" -WaitMutex 2>&1 | grep -E "error|warning:|Result:"
[ "${PIPESTATUS[0]}" -eq 0 ] || { echo "BUILD FAILED"; exit 1; }

"$UE/Engine/Binaries/Linux/UnrealEditor-Cmd" "$PROJECT_DIR/BlackNumbers.uproject" \
  -ExecCmds="Automation RunTests $FILTER; Quit" -nullrhi -unattended -nopause -nosplash -NoSound "$@" > /dev/null 2>&1
status=$?

LOG="$PROJECT_DIR/Saved/Logs/BlackNumbers.log"
grep -E "Test Completed|BN [a-z ]+test:|Expected '|Error: Player|Error: Test track" "$LOG" | cut -c31-
grep -E "TEST COMPLETE" "$LOG" | cut -c31-
exit $status
