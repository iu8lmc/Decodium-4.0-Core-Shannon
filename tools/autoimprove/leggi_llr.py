#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Lettore del dump LLR di FT2 (DECODIUM_LLR_DUMP). Formato e convenzioni:
doc/fastldpc/llr_dump.md.

  python leggi_llr.py file.bin            riepilogo
  python leggi_llr.py file.bin --controlla verifica segno/bit sui decodificati
"""
import struct
import sys

import numpy as np

MAGIC = 0x4C4C5231
REC = struct.Struct("<IIdfffB10s")
DIM = REC.size + 4 * 174

# FT2 mescola i 91 bit sistematici con questo vettore fisso: gli LLR stanno
# nello spazio message91, il messaggio del record nello spazio message77.
RVEC = np.array([
    0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1], dtype=np.uint8)


def leggi(percorso):
    dati = open(percorso, "rb").read()
    if len(dati) % DIM:
        raise SystemExit("file troncato o di un'altra versione: %d byte, record da %d"
                         % (len(dati), DIM))
    for off in range(0, len(dati), DIM):
        magic, ver, t, freq, dt, sync, ok, msg = REC.unpack_from(dati, off)
        if magic != MAGIC or ver != 1:
            raise SystemExit("record %d: magic/versione inattesi" % (off // DIM))
        yield {
            "t": t, "freq": freq, "dt": dt, "sync": sync, "ok": bool(ok),
            "bits": np.unpackbits(np.frombuffer(msg, dtype=np.uint8))[:77],
            "llr": np.frombuffer(dati, dtype="<f4", count=174, offset=off + REC.size),
        }


def main():
    rec = list(leggi(sys.argv[1]))
    cicli = sorted({round(r["t"], 6) for r in rec})
    ok = [r for r in rec if r["ok"]]
    print("record %d, cicli %d, decodificati %d (%.1f per ciclo)"
          % (len(rec), len(cicli), len(ok), len(rec) / max(1, len(cicli))))
    if rec:
        sy = np.array([r["sync"] for r in rec])
        print("sync: mediana %.2f, max %.2f | frequenze %.0f-%.0f Hz"
              % (np.median(sy), sy.max(),
                 min(r["freq"] for r in rec), max(r["freq"] for r in rec)))
    if "--controlla" in sys.argv:
        for r in ok:
            segno = (r["llr"][:77] > 0).astype(np.uint8)
            n = int((segno == (r["bits"] ^ RVEC)).sum())
            print("  f=%7.1f dt=%+.3f  accordo segno/bit %d/77 %s"
                  % (r["freq"], r["dt"], n, "OK" if n == 77 else "<-- SOSPETTO"))


if __name__ == "__main__":
    main()
