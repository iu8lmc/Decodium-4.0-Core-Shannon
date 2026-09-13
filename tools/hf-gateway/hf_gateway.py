#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
hf_gateway.py — Gateway "radio HF via internet" per Decodium FT2/FT2-Link.

Emula il percorso audio di una radio HF tra due stazioni collegate via
internet (UDP), con simulazione opzionale del canale HF (rumore AWGN,
QSB/fading, attenuazione, offset di frequenza). Pensato per sperimentare
FT2-Link tra IU8LMC e elisir80 quando la propagazione non aiuta.

Catena audio (simmetrica su entrambi i lati):

  Decodium TX audio -> cavo virtuale A -> [gateway: capture] -> UDP -> peer
  peer -> UDP -> [gateway: canale HF -> playback] -> cavo virtuale B -> Decodium RX

Formato di trasporto: PCM int16 mono, frame da 10 ms (nessun codec:
i codec voce con AGC/compressione distruggono i modi digitali).
48000 Hz di default = stesso sample rate del path audio di Decodium
(dal 1.0.473 anche W500/W2300 live sono a 48 kHz).

Dipendenze: sounddevice, numpy  (scipy opzionale, solo per --freq-offset)

Esempi:
  python hf_gateway.py --list-devices
  python hf_gateway.py --in-device "CABLE-A Output" --out-device "CABLE-B Input" \
                       --peer 100.64.0.2:5550 --listen 5550
  # con canale HF: rumore a -30 dBFS e QSB di 6 dB ogni 20 s
  python hf_gateway.py ... --noise-dbfs -30 --qsb-depth-db 6 --qsb-period 20
  # primo collaudo senza Decodium: manda un tono a 1500 Hz al peer
  python hf_gateway.py ... --tone-test
"""

import argparse
import socket
import struct
import sys
import threading
import time
from collections import deque

import numpy as np

try:
    import sounddevice as sd
except ImportError:
    print("ERRORE: manca 'sounddevice'. Installa con: pip install sounddevice numpy")
    sys.exit(1)

# --- protocollo UDP ---------------------------------------------------------
# header: magic 4s | ver B | flags B | seq I | t_ms Q | rate I  = 22 byte
MAGIC = b"HFGW"
VERSION = 1
HDR = struct.Struct("!4sBBIQI")
FLAG_AUDIO = 0
FLAG_PING = 1
FLAG_PONG = 2


def now_ms() -> int:
    return int(time.monotonic() * 1000)


# --- simulazione canale HF ---------------------------------------------------
class HfChannel:
    """Applica impairment HF al segnale ricevuto, prima del playback.

    Tutto è spento di default (pass-through pulito). L'SNR effettivo lo
    regoli combinando --attenuate-db (abbassa il segnale) e --noise-dbfs
    (alza il pavimento di rumore); le statistiche stampano i livelli misurati.
    """

    def __init__(self, rate: int, noise_dbfs: float | None,
                 attenuate_db: float, qsb_depth_db: float,
                 qsb_period: float, freq_offset: float):
        self.rate = rate
        self.noise_lin = 10 ** (noise_dbfs / 20.0) if noise_dbfs is not None else 0.0
        self.gain_lin = 10 ** (-attenuate_db / 20.0)
        self.qsb_depth_db = qsb_depth_db
        self.qsb_period = max(qsb_period, 1.0)
        self.freq_offset = freq_offset
        self._t = 0  # campioni processati (fase QSB / mixer)
        self._rng = np.random.default_rng()
        self._hilbert = None
        if abs(freq_offset) > 0.01:
            try:
                from scipy.signal import hilbert  # noqa: F401
                self._hilbert = hilbert
                # finestra scorrevole per l'analitico (3 frame, si usa il centrale)
                self._fo_hist = np.zeros(0, dtype=np.float64)
            except ImportError:
                print("AVVISO: --freq-offset richiede scipy (pip install scipy) — offset DISATTIVATO")
                self.freq_offset = 0.0

    @property
    def active(self) -> bool:
        return (self.noise_lin > 0.0 or self.gain_lin != 1.0
                or self.qsb_depth_db > 0.0 or abs(self.freq_offset) > 0.01)

    def process(self, x: np.ndarray) -> np.ndarray:
        """x: float64 in [-1,1]. Ritorna il segnale 'passato per il canale'."""
        n = len(x)
        y = x * self.gain_lin

        # QSB: fading sinusoidale lento, profondità picco-picco qsb_depth_db
        if self.qsb_depth_db > 0.0:
            t = (self._t + np.arange(n)) / self.rate
            fade_db = -0.5 * self.qsb_depth_db * (1.0 + np.sin(2 * np.pi * t / self.qsb_period))
            y = y * (10 ** (fade_db / 20.0))

        # offset di frequenza via segnale analitico (scipy), fase continua
        if abs(self.freq_offset) > 0.01 and self._hilbert is not None:
            self._fo_hist = np.concatenate([self._fo_hist, y])[-3 * n:]
            if len(self._fo_hist) >= 3 * n:
                analytic = self._hilbert(self._fo_hist)
                mid = analytic[n:2 * n]  # frame centrale = bordi puliti
                ph = 2 * np.pi * self.freq_offset * (self._t + np.arange(n)) / self.rate
                y = np.real(mid * np.exp(1j * ph))

        # rumore AWGN (pavimento in dBFS)
        if self.noise_lin > 0.0:
            y = y + self._rng.normal(0.0, self.noise_lin, n)

        self._t += n
        return np.clip(y, -1.0, 1.0)


# --- gateway ------------------------------------------------------------------
class Gateway:
    def __init__(self, args):
        self.rate = args.rate
        self.frame = int(self.rate * args.frame_ms / 1000)
        self.jitter_frames = max(2, int(args.jitter_ms / args.frame_ms))
        peer_host, peer_port = args.peer.rsplit(":", 1)
        self.peer = (peer_host, int(peer_port))
        self.tone_test = args.tone_test
        self.channel = HfChannel(self.rate, args.noise_dbfs, args.attenuate_db,
                                 args.qsb_depth_db, args.qsb_period, args.freq_offset)

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("0.0.0.0", args.listen))
        self.sock.settimeout(0.5)

        self.seq_tx = 0
        self.rx_buf: dict[int, np.ndarray] = {}
        self.rx_next: int | None = None
        self.rx_lock = threading.Lock()
        self.prebuffering = True

        # statistiche
        self.st = dict(sent=0, recv=0, lost=0, late=0, underrun=0,
                       rtt_ms=-1.0, in_rms=0.0, out_rms=0.0)
        self._stop = threading.Event()
        self._tone_phase = 0.0

    # --- rete ---
    def _send(self, flags: int, payload: bytes):
        hdr = HDR.pack(MAGIC, VERSION, flags, self.seq_tx & 0xFFFFFFFF, now_ms(), self.rate)
        try:
            self.sock.sendto(hdr + payload, self.peer)
        except OSError:
            pass

    def rx_thread(self):
        while not self._stop.is_set():
            try:
                data, _addr = self.sock.recvfrom(4096)
            except socket.timeout:
                continue
            except OSError:
                break
            if len(data) < HDR.size:
                continue
            magic, ver, flags, seq, t_ms, rate = HDR.unpack_from(data)
            if magic != MAGIC or ver != VERSION:
                continue
            if flags == FLAG_PING:
                hdr = HDR.pack(MAGIC, VERSION, FLAG_PONG, seq, t_ms, self.rate)
                try:
                    self.sock.sendto(hdr, self.peer)
                except OSError:
                    pass
                continue
            if flags == FLAG_PONG:
                self.st["rtt_ms"] = now_ms() - t_ms
                continue
            if rate != self.rate:
                # peer con sample rate diverso: segnala e scarta
                self.st["late"] += 1
                continue
            pcm = np.frombuffer(data[HDR.size:], dtype=np.int16)
            if len(pcm) != self.frame:
                continue
            with self.rx_lock:
                if self.rx_next is None:
                    self.rx_next = seq
                if seq < self.rx_next:
                    self.st["late"] += 1
                    continue
                self.rx_buf[seq] = pcm.astype(np.float64) / 32768.0
                self.st["recv"] += 1
                # limite memoria: mai più di 4x jitter buffer
                if len(self.rx_buf) > 4 * self.jitter_frames:
                    for k in sorted(self.rx_buf)[: len(self.rx_buf) - 2 * self.jitter_frames]:
                        del self.rx_buf[k]
                        self.rx_next = max(self.rx_next, k + 1)

    def ping_thread(self):
        while not self._stop.is_set():
            self._send(FLAG_PING, b"")
            time.sleep(2.0)

    # --- audio ---
    def audio_callback(self, indata, outdata, frames, time_info, status):
        # TX: cattura da Decodium (o tono di test) -> UDP verso il peer
        if self.tone_test:
            t = (self._tone_phase + np.arange(frames)) / self.rate
            mono = 0.5 * np.sin(2 * np.pi * 1500.0 * t)
            self._tone_phase += frames
        else:
            mono = indata[:, 0].astype(np.float64)
        self.st["in_rms"] = float(np.sqrt(np.mean(mono ** 2)) + 1e-12)
        pcm = np.clip(mono * 32767.0, -32768, 32767).astype(np.int16)
        self._send(FLAG_AUDIO, pcm.tobytes())
        self.seq_tx += 1
        self.st["sent"] += 1

        # RX: jitter buffer -> canale HF -> verso Decodium
        out = np.zeros(frames, dtype=np.float64)
        with self.rx_lock:
            if self.prebuffering:
                if len(self.rx_buf) >= self.jitter_frames:
                    self.prebuffering = False
            if not self.prebuffering and self.rx_next is not None:
                if self.rx_next in self.rx_buf:
                    out = self.rx_buf.pop(self.rx_next)
                    self.rx_next += 1
                else:
                    # frame perso o in ritardo: silenzio, avanza comunque
                    if self.rx_buf:
                        self.st["lost"] += 1
                        self.rx_next += 1
                    else:
                        self.st["underrun"] += 1
                        self.prebuffering = True  # ricostruisci il buffer
        if self.channel.active:
            out = self.channel.process(out)
        self.st["out_rms"] = float(np.sqrt(np.mean(out ** 2)) + 1e-12)
        outdata[:, 0] = out.astype(np.float32)

    # --- run ---
    def run(self, in_dev, out_dev):
        threads = [threading.Thread(target=self.rx_thread, daemon=True),
                   threading.Thread(target=self.ping_thread, daemon=True)]
        for t in threads:
            t.start()
        stream = sd.Stream(samplerate=self.rate, blocksize=self.frame,
                           channels=1, dtype="float32",
                           device=(in_dev, out_dev),
                           callback=self.audio_callback)
        print(f"gateway attivo: {self.rate} Hz, frame {self.frame} campioni "
              f"({1000 * self.frame // self.rate} ms), jitter {self.jitter_frames} frame")
        print(f"peer: {self.peer[0]}:{self.peer[1]}  "
              f"canale HF: {'ATTIVO' if self.channel.active else 'pass-through'}"
              f"{'  [TONO DI TEST 1500 Hz]' if self.tone_test else ''}")
        with stream:
            try:
                while True:
                    time.sleep(2.0)
                    s = self.st
                    def dbfs(v):
                        return f"{20*np.log10(max(v,1e-9)):5.1f}"
                    print(f"tx {s['sent']:7d}  rx {s['recv']:7d}  persi {s['lost']:5d}  "
                          f"ritardo {s['late']:4d}  underrun {s['underrun']:3d}  "
                          f"rtt {s['rtt_ms']:5.0f} ms  "
                          f"in {dbfs(s['in_rms'])} dBFS  out {dbfs(s['out_rms'])} dBFS")
            except KeyboardInterrupt:
                pass
            finally:
                self._stop.set()
        print("gateway chiuso.")


def main():
    ap = argparse.ArgumentParser(
        description="Gateway radio-HF-via-internet per Decodium FT2/FT2-Link",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    ap.add_argument("--list-devices", action="store_true",
                    help="elenca i dispositivi audio e esce")
    ap.add_argument("--in-device", default=None,
                    help="dispositivo di CATTURA (dove arriva il TX audio di Decodium, es. 'CABLE-A Output')")
    ap.add_argument("--out-device", default=None,
                    help="dispositivo di RIPRODUZIONE (che alimenta l'RX di Decodium, es. 'CABLE-B Input')")
    ap.add_argument("--peer", default="127.0.0.1:5550", help="IP:porta UDP del gateway remoto")
    ap.add_argument("--listen", type=int, default=5550, help="porta UDP locale di ascolto")
    ap.add_argument("--rate", type=int, default=48000, help="sample rate (48000 = path audio Decodium)")
    ap.add_argument("--frame-ms", type=int, default=10, choices=[5, 10, 20],
                    help="durata frame (10 ms sta sotto l'MTU a 48 kHz)")
    ap.add_argument("--jitter-ms", type=int, default=120, help="profondità jitter buffer")
    ap.add_argument("--tone-test", action="store_true",
                    help="trasmette un tono 1500 Hz invece dell'audio catturato (collaudo)")
    # canale HF (tutto spento di default)
    ap.add_argument("--noise-dbfs", type=float, default=None,
                    help="pavimento di rumore AWGN in dBFS (es. -30); default: niente rumore")
    ap.add_argument("--attenuate-db", type=float, default=0.0,
                    help="attenuazione del segnale ricevuto in dB (per abbassare l'SNR)")
    ap.add_argument("--qsb-depth-db", type=float, default=0.0,
                    help="profondità QSB picco-picco in dB (0 = niente fading)")
    ap.add_argument("--qsb-period", type=float, default=20.0, help="periodo QSB in secondi")
    ap.add_argument("--freq-offset", type=float, default=0.0,
                    help="offset di frequenza in Hz (richiede scipy)")
    args = ap.parse_args()

    if args.list_devices:
        print(sd.query_devices())
        return

    frame_bytes = int(args.rate * args.frame_ms / 1000) * 2
    if frame_bytes + HDR.size > 1400:
        print(f"AVVISO: frame da {frame_bytes + HDR.size} byte supera l'MTU tipico (1500): "
              f"usa --frame-ms 10 o inferiore a {args.rate} Hz")

    def dev(x):
        # accetta sia il nome (substring) sia l'indice numerico di --list-devices
        if x is not None and str(x).lstrip("-").isdigit():
            return int(x)
        return x

    gw = Gateway(args)
    gw.run(dev(args.in_device), dev(args.out_device))


if __name__ == "__main__":
    main()
