#!/usr/bin/env bash
# Standalone (non-editor) performance capture at 1920x1080 on the NVIDIA GPU.
# Writes Saved/Benchmark/benchmark.md (frame timings), vram.csv (nvidia-smi samples),
# bn.utrace (open with Unreal Insights) and screenshots under Saved/Screenshots.
# Usage: Tools/benchmark.sh [extra args, e.g. -windowed to watch it instead of rendering offscreen]
set -uo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UE="${UE:-$HOME/UnrealEngine}"
OUT="$PROJECT_DIR/Saved/Benchmark"
# Unreal's trace writer mangles non-ASCII paths (e.g. "Masaüstü"), so trace to /tmp and move it after.
TRACE_TMP="${TMPDIR:-/tmp}/blacknumbers-bn.utrace"
mkdir -p "$OUT"
rm -f "$OUT/benchmark.md" "$OUT/vram.csv" "$OUT/bn.utrace" "$TRACE_TMP"

nvidia-smi --query-gpu=timestamp,memory.used,memory.total,utilization.gpu --format=csv -l 1 > "$OUT/vram.csv" 2>/dev/null &
SMI=$!

prime-run "$UE/Engine/Binaries/Linux/UnrealEditor" "$PROJECT_DIR/BlackNumbers.uproject" -game \
  -ResX=1920 -ResY=1080 -RenderOffscreen -unattended -nosplash \
  -ExecCmds="r.SetRes 1920x1080,r.VSync 0,t.MaxFPS 0,bn.Benchmark" \
  -trace=default,gpu -tracefile="$TRACE_TMP" "$@" > "$OUT/run.log" 2>&1
status=$?
kill $SMI 2>/dev/null
[ -f "$TRACE_TMP" ] && mv "$TRACE_TMP" "$OUT/bn.utrace"

if [ -f "$OUT/benchmark.md" ]; then
  cat "$OUT/benchmark.md"
  awk -F', ' 'NR>1 {gsub(/ MiB/,"",$2); if ($2+0>max) max=$2+0} END {print "\nPeak VRAM used on the GPU (whole device, includes desktop): " max " MiB"}' "$OUT/vram.csv"
else
  echo "No benchmark report; see $OUT/run.log" >&2
  status=1
fi
exit $status
