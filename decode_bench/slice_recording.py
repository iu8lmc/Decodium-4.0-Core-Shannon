#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
decode_bench/slice_recording.py — Affetta una registrazione RX CONTINUA di
Decodium in slot da 15 s ALLINEATI a UTC, pronti per bench_real.py.

Decodium ("Record RX") salva UN wav continuo 12 kHz mono 16-bit in
  ~/Documents/Decodium/recordings/decodium_<YYYYMMDD_HHmmss-UTC>.wav
Il banco e i decoder (ft8_stage_compare/jt9) vogliono invece slot da 15 s
allineati ai confini UTC :00/:15/:30/:45 (FT8) — perche' il decoder assume che
il segnale inizi al confine di slot (cerca il DT in una finestra stretta).

Questo script:
  - ricava lo start UTC dal nome file (o da --start),
  - calcola il primo confine di slot a partire dallo start,
  - taglia fette da esattamente 180000 campioni (15 s) allineate ai confini,
  - le nomina YYMMDD_HHMMSS.wav (convenzione WSJT-X) cosi' i decoder ricavano
    l'ora dal nome,
  - scarta le fette parziali a inizio/fine (solo slot interi).

Uso:
  py slice_recording.py --wav "C:/Users/IU8LMC/Documents/Decodium/recordings/decodium_20260617_193012.wav" \
     --out C:/tmp/slot_2026XXXX
  # poi:
  py bench_real.py --decodium ..\\build_mingw64\\tests\\ft8_stage_compare.exe \\
     --wavs C:/tmp/slot_2026XXXX --profiles deep,harvest --with-jt9 --label campo1

Per FT4 usa --slot 7.5 (slot da 7.5 s). Solo standard library.
"""

import argparse
import array
import datetime
import glob
import os
import re
import sys
import wave

FS = 12000


def read16(path):
    w = wave.open(path, "rb")
    try:
        if w.getnchannels() != 1 or w.getsampwidth() != 2 or w.getframerate() != FS:
            raise SystemExit("ERRORE: %s non e' 12000 Hz mono 16-bit "
                             "(ch=%d width=%d fs=%d)"
                             % (path, w.getnchannels(), w.getsampwidth(), w.getframerate()))
        raw = w.readframes(w.getnframes())
    finally:
        w.close()
    a = array.array("h")
    a.frombytes(raw)
    return a


def write16(path, samples):
    w = wave.open(path, "wb")
    try:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(FS)
        w.writeframes(array.array("h", samples).tobytes())
    finally:
        w.close()


def parse_start_utc(path, override):
    if override:
        return datetime.datetime.strptime(override, "%Y%m%d_%H%M%S")
    m = re.search(r"(\d{8}_\d{6})", os.path.basename(path))
    if not m:
        sys.exit("ERRORE: start UTC non deducibile dal nome '%s'; usa --start YYYYMMDD_HHMMSS"
                 % os.path.basename(path))
    return datetime.datetime.strptime(m.group(1), "%Y%m%d_%H%M%S")


def slice_one(wav_path, out_dir, slot_sec, start_override):
    samples = read16(wav_path)
    start = parse_start_utc(wav_path, start_override)
    slot_samp = int(round(slot_sec * FS))
    slot_int = int(round(slot_sec))

    # secondi (con frazione) dallo start al primo confine di slot UTC
    sec_of_day = start.hour * 3600 + start.minute * 60 + start.second
    rem = sec_of_day % slot_int
    off_sec = (slot_int - rem) % slot_int
    off_samp = int(round(off_sec * FS))
    t = start + datetime.timedelta(seconds=off_sec)

    os.makedirs(out_dir, exist_ok=True)
    n = len(samples)
    made = []
    while off_samp + slot_samp <= n:
        chunk = samples[off_samp:off_samp + slot_samp]
        name = t.strftime("%y%m%d_%H%M%S") + ".wav"
        outp = os.path.join(out_dir, name)
        write16(outp, chunk)
        made.append(outp)
        off_samp += slot_samp
        t += datetime.timedelta(seconds=slot_sec)
    return made, n / float(FS), off_sec


def main():
    ap = argparse.ArgumentParser(
        description="Affetta una registrazione RX continua di Decodium in slot UTC da 15 s.")
    ap.add_argument("--wav", required=True,
                    help="File registrazione (o glob/dir di registrazioni).")
    ap.add_argument("--out", required=True, help="Cartella di output per gli slot.")
    ap.add_argument("--slot", type=float, default=15.0,
                    help="Durata slot in s (default 15 = FT8; usa 7.5 per FT4).")
    ap.add_argument("--start", default=None,
                    help="Override start UTC 'YYYYMMDD_HHMMSS' (se non nel nome file).")
    args = ap.parse_args()

    if os.path.isdir(args.wav):
        wavs = sorted(glob.glob(os.path.join(args.wav, "decodium_*.wav"))) \
            or sorted(glob.glob(os.path.join(args.wav, "*.wav")))
    else:
        wavs = sorted(glob.glob(args.wav)) if any(c in args.wav for c in "*?[") else [args.wav]
    if not wavs:
        sys.exit("ERRORE: nessuna registrazione trovata in %r" % args.wav)

    total = 0
    for wp in wavs:
        made, dur, off = slice_one(wp, args.out, args.slot, args.start if len(wavs) == 1 else None)
        total += len(made)
        print("%s  (%.0fs, primo confine +%ds)  -> %d slot da %.1fs"
              % (os.path.basename(wp), dur, off, len(made), args.slot))
    print("Totale: %d slot in %s" % (total, args.out))
    print("Ora:  py bench_real.py --decodium ../build_mingw64/tests/ft8_stage_compare.exe "
          "--wavs %s --profiles deep,harvest --with-jt9 --label campo1" % args.out)


if __name__ == "__main__":
    main()
