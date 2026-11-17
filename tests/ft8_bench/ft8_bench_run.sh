#!/bin/bash
# Runs ft8_stage_compare on a corpus, one process per slot, writing <slot>.<profile>.txt/.time/.err.
# Profiles: d1 d2 prod deep harvest. Example with the phase-trellis round forced on:
#   DECODIUM_FT8_TRELLIS=1 ft8_bench_run.sh build/tests/ft8_stage_compare corpus out_new prod
# usage: ft8_bench_run.sh <binary> <corpdir> <outdir> <profile> [glob]   (env vars pass through to the decoder)
BIN=$1; CORP=$2; OUT=$3; P=$4; G=${5:-*}; mkdir -p $OUT; export QT_QPA_PLATFORM=offscreen
declare -A PROF
PROF[d1]="--stages 4 --depth 1"
PROF[d2]="--stages 4 --depth 2"
PROF[prod]="--stages 4 --depth 3"
PROF[deep]="--stages 4 --depth 4 --lft8apon 1 --maxosd 3 --norder 4"
PROF[harvest]="--stages 4 --depth 4 --lft8apon 1 --subpass 1 --cycles 3 --low-thresholds 1 --rxfsens 3 --maxosd 3 --norder 4"
for w in $CORP/$G.wav; do
  n=$(basename $w .wav)
  [ -s $OUT/$n.$P.time ] && continue
  s=$(date +%s.%N)
  $BIN ${PROF[$P]} --nfqso 1500 --nfa 200 --nfb 4000 --max-ms ${MAXMS:-60000} $w 2>$OUT/$n.$P.err | grep -E 'decoded' > $OUT/$n.$P.txt
  e=$(date +%s.%N)
  echo "$e - $s" | bc > $OUT/$n.$P.time
done
