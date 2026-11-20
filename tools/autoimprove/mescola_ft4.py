#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Slot FT4 con VERITA' NOTA dentro rumore VERO (copia di mescola_ft2.py per FT4).

Una registrazione off-air non dice quali messaggi ci fossero davvero, quindi
non permette di calcolare il FER; una simulazione pura non ha ne' QRM ne'
propagazione. La via di mezzo: si prende una registrazione di banda vera come
sfondo e ci si sommano segnali FT2 sintetici di cui si sa tutto -- messaggio,
frequenza, DT e SNR.

Il segnale pulito lo genera lo stack TX del programma (tests/ft2_make_test_wav),
non un modulatore scritto a parte: cosi' la forma d'onda e' quella vera.

  python mescola_ft2.py --sfondo registrazione.wav --out cartella \
      --livelli -24,-22,-20,-18,-16,-14,-12,-10 --per-livello 200

Scrive cartella/slot_XXXXX.wav (12 kHz, mono, 16 bit, 3,75 s) e verita.json
con un record per slot. Vedi doc/fastldpc/llr_dump.md per il resto del metodo.
"""
import argparse
import json
import os
import random
import struct
import subprocess
import sys
import tempfile
import wave

import numpy as np

SR = 12000
SLOT = 72576                       # campioni nella finestra FT4 del decoder (6,05 s)
BANDA_SNR = 2500.0                 # la banda di riferimento di WSJT-X
# Copied from the lab (Detector/fastldpc/lab/tools); the build dir comes from DECODIUM_AI_BUILD.
GEN = os.path.join(os.environ.get("DECODIUM_AI_BUILD", "build_689"), "tests", "ft4_make_test_wav.exe")

# Il DT che Decodium riporta e' offset_ms/1000 - 0.5: --offset-ms 600 -> dt +0.1
OFFSET_ZERO_MS = 500.0

PREFISSI = ["IU8", "IK2", "DL9", "G4", "F5", "EA7", "SP9", "OK1", "SM5", "OH2",
            "LZ1", "YO3", "S57", "9A2", "UR5", "R3", "K1", "W7", "N4", "VE3",
            "JA1", "VK2", "PY2", "LU7", "ZS6", "4X1", "TA3", "HA8", "OZ1", "PA0"]
SUFFISSI = ["ABC", "XYZ", "QRS", "TVW", "KLM", "NOP", "DEF", "GHI", "JKL", "RST"]
LOCATORI = ["JN71", "JO62", "IO83", "KO14", "EM12", "FN42", "PM95", "QF22",
            "GG66", "KM56", "IM99", "LO63"]


def genera_messaggio(rnd):
    """Un messaggio FT2 standard, del tipo che si sente in banda."""
    a = rnd.choice(PREFISSI) + rnd.choice(SUFFISSI)
    b = rnd.choice(PREFISSI) + rnd.choice(SUFFISSI)
    tipo = rnd.random()
    if tipo < 0.35:
        return "CQ %s %s" % (a, rnd.choice(LOCATORI))
    if tipo < 0.60:
        return "%s %s %s" % (a, b, rnd.choice(LOCATORI))
    if tipo < 0.80:
        return "%s %s %+03d" % (a, b, rnd.randint(-24, 10))
    if tipo < 0.92:
        return "%s %s R%+03d" % (a, b, rnd.randint(-24, 10))
    return "%s %s RR73" % (a, b)


def leggi_wav(percorso):
    with wave.open(percorso, "rb") as w:
        if w.getframerate() != SR or w.getnchannels() != 1 or w.getsampwidth() != 2:
            raise SystemExit("%s: servono 12000 Hz, mono, 16 bit" % percorso)
        return np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(np.float64)


def scrivi_wav(percorso, campioni):
    dati = np.clip(np.round(campioni), -32768, 32767).astype("<i2")
    with wave.open(percorso, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(dati.tobytes())


def genera_pulito(msg, freq, dt, drift, tmp):
    """Lo slot FT2 pulito dallo stack TX vero, senza rumore aggiunto."""
    args = [GEN, "--message", msg, "--freq", "%.1f" % freq,
            "--offset-ms", "%.1f" % (OFFSET_ZERO_MS + dt * 1000.0),
            "--gain", "0.85", tmp]
    if drift:
        args += ["--drift-hz", "%.2f" % drift]
    # Il generatore e' un programma Qt: senza le DLL di MSYS2 nel PATH esce
    # con 0xC0000135 e nessun messaggio.
    env = dict(os.environ)
    env["PATH"] = r"C:\msys64\mingw64\bin;" + env.get("PATH", "")
    r = subprocess.run(args, capture_output=True, text=True, env=env)
    if r.returncode != 0:
        raise SystemExit("ft2_make_test_wav: %s" % (r.stderr.strip() or hex(r.returncode)))
    return leggi_wav(tmp)


def fading_lorentz(x, fspread, rnd):
    """Evanescenza di Rayleigh con spettro Doppler lorentziano.

    E' la trasposizione fedele di lib/fspread_lorentz.f90, la routine che il
    programma usa gia' per simulare la propagazione (EchoCallSim): si costruisce
    uno spettro lorentziano troncato con fasi casuali, lo si porta nel tempo e
    ci si moltiplica il segnale analitico. Senza questo il segnale iniettato ha
    ampiezza costante, cioe' una condizione che in HF non esiste.

    fspread e' la larghezza Doppler in Hz: 0,1-0,3 tipica su banda tranquilla,
    1-3 su percorsi disturbati, molto di piu' in aurora.
    """
    if fspread <= 0.0:
        return x
    n = len(x)
    b = 6.0
    df = SR / float(n)
    freq = np.arange(n // 2 + 1) * df
    xx = b * freq / fspread
    amp = np.where(xx < 3.0, np.sqrt(np.maximum(1.111 / (1.0 + xx * xx) - 0.1, 0.0)), 0.0)
    fasi = np.array([rnd.uniform(0.0, 2.0 * np.pi) for _ in range(len(freq))])
    pos = amp * np.exp(1j * fasi)
    pos[0] = 1.0                      # componente continua, come nel Fortran
    if n % 2 == 0:
        pos[-1] = 0.0
    fasi_neg = np.array([rnd.uniform(0.0, 2.0 * np.pi) for _ in range(len(freq))])
    neg = amp * np.exp(1j * fasi_neg)
    spettro = np.zeros(n, dtype=complex)
    spettro[:len(pos)] = pos
    # le frequenze negative hanno fase indipendente, come nel Fortran
    spettro[n - len(neg) + 1:] = neg[1:][::-1]
    cspread = np.fft.ifft(spettro) * n
    potenza = np.mean(np.abs(cspread) ** 2)
    cspread *= np.sqrt(1.0 / potenza)         # potenza media unitaria
    from scipy.signal import hilbert
    analitico = hilbert(x)
    return np.real(analitico * cspread)


def densita_rumore(sfondo, centro=None, larghezza=250.0, riferimento="mediana"):
    """Potenza di rumore in 2500 Hz, stimata in modo robusto.

    Lo sfondo contiene segnali veri (e' il punto), quindi la media di potenza
    sovrastimerebbe il rumore: si prende la MEDIANA della densita' spettrale,
    che i segnali stretti non spostano.

    Con `centro` la mediana si prende INTORNO alla frequenza del segnale
    (+-larghezza Hz) invece che su tutta la banda: e' il rumore che il segnale
    trova davvero. Misurato il 16/9/2026: col riferimento di banda larga meta'
    dei frame a "0 dB" non venivano nemmeno agganciati, perche' il rumore
    locale poteva essere 10 dB sopra la mediana di banda.
    """
    n = 8192
    if len(sfondo) < n:
        n = 1 << int(np.floor(np.log2(len(sfondo))))
    fin = np.hanning(n)
    pezzi = []
    for start in range(0, len(sfondo) - n, n):
        spettro = np.fft.rfft(sfondo[start:start + n] * fin)
        # densita' di potenza per Hz, normalizzata per la finestra
        pezzi.append(np.abs(spettro) ** 2 / (np.sum(fin ** 2) * SR))
    if not pezzi:
        raise SystemExit("sfondo troppo corto")
    psd = np.median(np.array(pezzi), axis=0)
    freq = np.fft.rfftfreq(n, 1.0 / SR)
    if centro is None:
        dentro = (freq >= 300.0) & (freq <= 2700.0)
    else:
        dentro = (freq >= centro - larghezza) & (freq <= centro + larghezza)
        if dentro.sum() < 8:
            dentro = (freq >= 300.0) & (freq <= 2700.0)
    if riferimento == "totale":
        # tutta la potenza nei 2500 Hz attorno al segnale, QRM compreso: piu'
        # severo, e piu' vicino a quello che il decodificatore deve battere
        c = centro if centro is not None else 1500.0
        banda = (freq >= max(0.0, c - BANDA_SNR / 2.0)) & (freq <= c + BANDA_SNR / 2.0)
        passo = float(freq[1] - freq[0])
        return float(np.sum(psd[banda]) * 2.0 * passo)
    return float(np.median(psd[dentro]) * 2.0 * BANDA_SNR)   # rfft: un solo lato


def scegli_frequenza(rumore, rnd, a):
    """A caso nella banda, oppure -- con --evita-qrm -- dove lo sfondo e' piu'
    quieto. Mettere il segnale sopra una portante forte e' realistico ma
    trasforma la misura di soglia in una misura di QRM."""
    if not a.evita_qrm:
        return rnd.uniform(a.freq_min, a.freq_max)
    # PSD su TUTTO lo slot (non solo sull'inizio): una portante che compare a
    # meta' slot rovina il frame tanto quanto una che c'e' da subito.
    n = 8192
    fin = np.hanning(n)
    pezzi = [np.abs(np.fft.rfft(rumore[s:s + n] * fin)) ** 2
             for s in range(0, len(rumore) - n, n // 2)]
    spettro = np.max(np.array(pezzi), axis=0)          # il massimo nel tempo: il QRM peggiore
    freq = np.fft.rfftfreq(n, 1.0 / SR)
    dentro = np.where((freq >= a.freq_min) & (freq <= a.freq_max))[0]
    # FT2 occupa quattro toni distanziati 12000/288 = 41,67 Hz: dalla portante
    # fino a ~167 Hz sopra. Guardare solo +-40 Hz lasciava passare frame con
    # QRM in mezzo alla propria banda, e li perdeva tutti.
    giu = max(1, int(20.0 / (SR / n)))
    su = max(1, int(190.0 / (SR / n)))
    picco = np.array([spettro[max(0, i - giu):i + su + 1].max() for i in dentro])
    fondo = np.median(spettro[dentro])
    buoni = dentro[picco <= fondo * 4.0]               # meno di 6 dB sopra il fondo
    if len(buoni) == 0:
        buoni = dentro[picco <= np.quantile(picco, 0.25)]
    scelto = buoni[rnd.randrange(len(buoni))]
    return float(np.clip(freq[scelto] + rnd.uniform(-3.0, 3.0), a.freq_min, a.freq_max))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sfondo", required=True, help="registrazione di banda vera, 12 kHz mono 16 bit")
    ap.add_argument("--out", required=True)
    ap.add_argument("--livelli", default="-24,-22,-20,-18,-16,-14,-12,-10")
    ap.add_argument("--per-livello", type=int, default=200)
    ap.add_argument("--freq-min", type=float, default=400.0)
    ap.add_argument("--freq-max", type=float, default=2600.0)
    ap.add_argument("--dt-max", type=float, default=0.3, help="|DT| massimo, s")
    ap.add_argument("--drift", type=float, default=0.0,
                    help="deriva DENTRO lo slot, Hz dall'inizio alla fine (oscillatore che "
                         "scalda). E' sintetica e lineare: la vera puo' essere irregolare.")
    ap.add_argument("--fspread", type=float, default=0.0,
                    help="allargamento Doppler in Hz (evanescenza di Rayleigh, modello "
                         "lorentziano di lib/fspread_lorentz.f90). 0 = segnale di ampiezza "
                         "costante; 0,1-0,3 banda tranquilla; 1-3 percorso disturbato.")
    ap.add_argument("--deriva-slot", type=float, default=0.0,
                    help="di quanti Hz la stazione si sposta a ogni sua ritrasmissione. E' "
                         "questa, non quella dentro lo slot, che mette alla prova la finestra "
                         "in frequenza della memoria.")
    ap.add_argument("--riferimento", choices=["mediana", "totale"], default="mediana",
                    help="rumore di riferimento: mediana della densita' locale (default) "
                         "oppure potenza totale nei 2500 Hz, QRM compreso")
    ap.add_argument("--evita-qrm", action="store_true",
                    help="mette il segnale dove lo sfondo e' piu' quieto invece che a caso")
    ap.add_argument("--sequenze", type=int, default=1,
                    help="slot consecutivi con la STESSA stazione alla stessa frequenza "
                         "(1 = slot indipendenti). Serve ai meccanismi che usano la memoria: "
                         "in FT2 una stazione ripete ogni 3,75 s.")
    ap.add_argument("--passo", type=int, default=1,
                    help="ogni quanti slot la stessa stazione ritrasmette (1 = tutti gli "
                         "slot, 2 = a slot alterni come in aria, dove le stazioni FT2 si "
                         "alternano). Con passo 2 gli slot pari e dispari portano due "
                         "sequenze diverse, come due stazioni che si rispondono.")
    ap.add_argument("--qsb", type=float, default=3.0,
                    help="oscillazione del livello dentro una sequenza, dB picco-picco")
    ap.add_argument("--seed", type=int, default=20260916)
    ap.add_argument("--salta-inizio", type=float, default=60.0, help="secondi di sfondo da saltare")
    a = ap.parse_args()

    livelli = [float(x) for x in a.livelli.split(",")]
    rnd = random.Random(a.seed)
    os.makedirs(a.out, exist_ok=True)

    sfondo = leggi_wav(a.sfondo)
    inizio = int(a.salta_inizio * SR) if hasattr(a, "salta_inizio") else 0
    sfondo = sfondo[inizio:]
    nslot_sfondo = len(sfondo) // SLOT
    if nslot_sfondo < 10:
        raise SystemExit("sfondo troppo corto: %d slot" % nslot_sfondo)
    print("sfondo: %d slot da 3,75 s" % nslot_sfondo)

    verita = []
    tmp = os.path.join(tempfile.gettempdir(), "mescola_ft2_pulito.wav")
    totale = len(livelli) * a.per_livello
    fatti = 0
    # Una "corsia" per ogni resto della divisione per `passo`: con passo 2 gli
    # slot pari portano una stazione e i dispari un'altra, come due che si
    # rispondono. Ogni corsia tiene la propria sequenza.
    corsie = {}
    islot_corrente = 0
    for snr in livelli:
        for _ in range(a.per_livello):
            islot = rnd.randrange(nslot_sfondo)
            rumore = sfondo[islot * SLOT:(islot + 1) * SLOT].copy()
            corsia = islot_corrente % max(1, a.passo)
            islot_corrente += 1
            sequenza = corsie.get(corsia)
            if a.sequenze > 1 and sequenza and sequenza[4] > 0:
                msg, freq, dt, drift, restano = sequenza
                # la stessa stazione ripete: frequenza ferma entro un hertz,
                # DT quasi uguale, livello che ondeggia come il QSB vero
                freq += rnd.uniform(-0.8, 0.8) + a.deriva_slot
                dt += rnd.uniform(-0.01, 0.01)
                corsie[corsia] = (msg, freq, dt, drift, restano - 1)
            else:
                msg = genera_messaggio(rnd)
                freq = scegli_frequenza(rumore, rnd, a)
                dt = rnd.uniform(-a.dt_max, a.dt_max)
                drift = rnd.uniform(-a.drift, a.drift) if a.drift else 0.0
                corsie[corsia] = (msg, freq, dt, drift, a.sequenze - 1)
            pulito = genera_pulito(msg, freq, dt, drift, tmp)[:SLOT]
            if a.fspread > 0.0:
                pulito = fading_lorentz(pulito, a.fspread, rnd)
            if len(pulito) < SLOT:
                pulito = np.pad(pulito, (0, SLOT - len(pulito)))

            # potenza del solo segnale, sulla durata del burst (non sul silenzio)
            attivo = np.abs(pulito) > (np.max(np.abs(pulito)) * 0.05)
            p_segnale = float(np.mean(pulito[attivo] ** 2)) if attivo.any() else 0.0
            if p_segnale <= 0.0:
                raise SystemExit("il generatore ha prodotto uno slot vuoto")
            p_rumore = densita_rumore(rumore, centro=freq, riferimento=a.riferimento)
            snr_slot = snr + (rnd.uniform(-a.qsb, a.qsb) / 2.0 if a.sequenze > 1 else 0.0)
            voluta = p_rumore * (10.0 ** (snr_slot / 10.0))
            scala = np.sqrt(voluta / p_segnale)

            somma = rumore + pulito * scala
            # se si satura si abbassa tutto: il rapporto segnale/rumore non cambia
            picco = np.max(np.abs(somma))
            if picco > 32000.0:
                somma *= 32000.0 / picco

            nome = "slot_%05d.wav" % len(verita)
            scrivi_wav(os.path.join(a.out, nome), somma)
            verita.append({"file": nome, "messaggio": msg, "snr_db": snr,
                           "snr_slot_db": round(snr_slot, 2),
                           "freq_hz": round(freq, 2), "dt_s": round(dt, 4),
                           "drift_hz": round(drift, 2), "slot_sfondo": islot,
                           "riferimento": a.riferimento, "fspread_hz": a.fspread})
            fatti += 1
            if fatti % 50 == 0:
                print("  %d/%d" % (fatti, totale), flush=True)

    with open(os.path.join(a.out, "verita.json"), "w", encoding="utf-8") as f:
        json.dump(verita, f, ensure_ascii=False, indent=1)
    print("scritti %d slot in %s" % (len(verita), a.out))


if __name__ == "__main__":
    main()
