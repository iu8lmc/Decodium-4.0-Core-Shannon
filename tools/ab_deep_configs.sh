#!/usr/bin/env bash
# A/B offline dei knob del deep FT8 sui 7 slot puliti, in modalita' SEQUENZA
# (hint a7 accumulati), capped al budget live ~5849ms. Misura nout totale,
# deboli (<=-18), e tempo per config.
set -u
cd "$(dirname "$0")/../build_mingw64" || exit 1
export PATH="/c/msys64/mingw64/bin:$PATH"
TOOL=./tests/ft8_stage_compare.exe
SLOTS=(../_parity_slots/slot_2154{15,30,45}.wav ../_parity_slots/slot_2155{00,15,30,45}.wav)
COMMON="--stages 3 --nfa 100 --nfb 4000 --nzhsym 50 --lft8apon 1 --max-ms 5849 --max-iter 30"
OUT=/tmp/ab_deep
mkdir -p "$OUT"

run_cfg () {
  local name="$1"; shift
  local args="$*"
  local f="$OUT/$name.txt"
  local t0 t1
  t0=$(date +%s)
  $TOOL $COMMON $args "${SLOTS[@]}" > "$f" 2>&1
  t1=$(date +%s)
  local nout weak slots
  nout=$(grep -oP "stage 3: nout=\K[0-9]+" "$f" | awk '{s+=$1} END{print s+0}')
  slots=$(grep -cP "stage 3: nout=" "$f")
  # deboli <= -18 tra le righe "decoded="
  weak=$(grep -oP 'snr=\s*\K-?[0-9]+' "$f" | awk '$1<=-18{c++} END{print c+0}')
  printf "%-22s nout=%-4s deboli<=-18=%-4s slot=%-2s wall=%ss\n" "$name" "$nout" "$weak" "$slots" "$((t1-t0))"
  grep -oP "stage 3: nout=\K[0-9]+" "$f" | paste -sd' ' | sed "s/^/   per-slot: /"
}

echo "=== A/B deep FT8 (7 slot sequenza, cap 5849ms) ==="
run_cfg "A_live_suppl1_cyc1"   --supplemental 1 --depth 4 --cycles 1 --rxfsens 1 --maxosd 3 --norder 4
run_cfg "B_suppl0_depth4_cyc1" --supplemental 0 --depth 4 --cycles 1 --rxfsens 1 --maxosd 3 --norder 4
run_cfg "C_suppl0_cyc2_rxf2"   --supplemental 0 --depth 4 --cycles 2 --rxfsens 2 --maxosd 3 --norder 4
run_cfg "D_suppl1_norder5"     --supplemental 1 --depth 4 --cycles 1 --rxfsens 1 --maxosd 3 --norder 5
echo "=== fine ==="
