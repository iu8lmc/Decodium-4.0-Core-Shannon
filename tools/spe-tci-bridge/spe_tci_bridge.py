#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""spe-tci-bridge — porta al DECOMETER la potenza dell'amplificatore.

Il problema. Decodium legge la telemetria dal proprio backend CAT: se il
backend e' TCI, potenza e ROS arrivano da li'. Quindi un ponte che pubblichi
solo l'amplificatore non basta - diventerebbe "la radio" per Decodium, che
perderebbe frequenza, modo e PTT.

Questo ponte fa entrambe le cose:

    radio  --(rigctld)-->  \\
                             ponte  --(TCI)-->  Decodium
    amplificatore -------->  /

Inoltra la radio leggendola da un rigctld (il protocollo di rete di Hamlib,
che Decodium stesso sa servire) e vi aggiunge la telemetria
dell'amplificatore, pubblicando il tutto come server TCI.

Il vantaggio del TCI e' che e' multi-client: piu' programmi possono collegarsi
allo stesso ponte, mentre una seriale la apre uno solo.

Uso tipico:
    python spe_tci_bridge.py --rigctld 127.0.0.1:4533 --amp demo
    python spe_tci_bridge.py --rigctld 127.0.0.1:4533 --amp hamlib:401:COM7

poi in Decodium: backend CAT = TCI, indirizzo 127.0.0.1:50001.

Solo libreria standard: nessuna dipendenza da installare.
"""

import argparse
import base64
import hashlib
import math
import os
import socket
import struct
import sys
import threading
import time

# ---------------------------------------------------------------- stato comune
class Stato:
    """Cio' che il ponte pubblica. Un lucchetto perche' lo scrivono piu' thread."""

    def __init__(self):
        self.lock = threading.Lock()
        self.freq = 14074000
        self.mode = "digu"
        self.ptt = False
        self.fwd = 0.0          # watt all'uscita dell'amplificatore
        self.ref = 0.0          # watt riflessi
        self.swr = 1.0
        self.amp_ok = False     # l'amplificatore risponde?
        self.rig_ok = False

    def snapshot(self):
        with self.lock:
            return dict(freq=self.freq, mode=self.mode, ptt=self.ptt,
                        fwd=self.fwd, ref=self.ref, swr=self.swr,
                        amp_ok=self.amp_ok, rig_ok=self.rig_ok)


# ------------------------------------------------------------ sorgente: radio
class RigctldSource(threading.Thread):
    """Legge frequenza, modo e PTT da un rigctld e li tiene aggiornati."""

    daemon = True

    def __init__(self, stato, host, port, period=0.4, simulate_tx=False):
        super().__init__(name="rigctld")
        self.stato, self.host, self.port, self.period = stato, host, port, period
        self.simulate_tx = simulate_tx
        self.sock = None

    def _connect(self):
        s = socket.create_connection((self.host, self.port), timeout=5)
        s.settimeout(5)
        self.sock = s

    def _ask(self, cmd):
        self.sock.sendall((cmd + "\n").encode())
        data = b""
        while b"\n" not in data:
            chunk = self.sock.recv(512)
            if not chunk:
                raise ConnectionError("rigctld ha chiuso")
            data += chunk
        return data.decode(errors="replace").strip()

    def run(self):
        while True:
            try:
                if self.sock is None:
                    self._connect()
                    with self.stato.lock:
                        self.stato.rig_ok = True
                    log("radio: collegato a rigctld %s:%d" % (self.host, self.port))
                f = self._ask("f")
                m = self._ask("m").splitlines()[0]
                t = self._ask("t")
                with self.stato.lock:
                    if f.lstrip("-").isdigit():
                        self.stato.freq = int(f)
                    self.stato.mode = tci_mode(m)
                    if not self.simulate_tx:
                        self.stato.ptt = (t.strip() == "1")
            except Exception as exc:
                with self.stato.lock:
                    self.stato.rig_ok = False
                log("radio: %s" % exc)
                try:
                    if self.sock:
                        self.sock.close()
                except Exception:
                    pass
                self.sock = None
                time.sleep(3)
                continue
            time.sleep(self.period)


def tci_mode(hamlib_mode):
    m = (hamlib_mode or "").strip().upper()
    return {"PKTUSB": "digu", "PKTLSB": "digl", "USB": "usb", "LSB": "lsb",
            "CW": "cw", "CWR": "cw", "AM": "am", "FM": "nfm",
            "RTTY": "rtty", "RTTYR": "rttyr"}.get(m, "usb")


# ---------------------------------------------------- sorgente: amplificatore
class DemoAmp(threading.Thread):
    """Amplificatore simulato: serve a verificare la catena senza hardware."""

    daemon = True

    def __init__(self, stato, watt=400.0):
        super().__init__(name="amp-demo")
        self.stato, self.watt = stato, watt

    def run(self):
        t0 = time.time()
        with self.stato.lock:
            self.stato.amp_ok = True
        while True:
            t = time.time() - t0
            with self.stato.lock:
                if self.stato.ptt:
                    # sillabazione plausibile, cosi' la balistica si vede
                    env = max(0.0, math.sin(t * 2.1)) ** 0.6
                    self.stato.fwd = self.watt * (0.55 + 0.45 * env)
                    self.stato.swr = 1.15 + 0.05 * math.sin(t * 0.7)
                    rho = (self.stato.swr - 1) / (self.stato.swr + 1)
                    self.stato.ref = self.stato.fwd * rho * rho
                else:
                    self.stato.fwd = 0.0
                    self.stato.ref = 0.0
            time.sleep(0.1)


class HamlibAmp(threading.Thread):
    """Amplificatore vero, letto con l'interfaccia amplificatori di Hamlib.

    Interroga le grandezze ANCHE quando le capacita' dichiarate dicono di no:
    il backend SPE Expert dichiara has_get_level = 0 pur avendo la funzione
    implementata, e solo l'apparato puo' dire chi ha ragione. Vedi
    tools/amp-probe/.
    """

    daemon = True

    AMP_LEVEL_SWR = 1 << 0
    AMP_LEVEL_PWR_FWD = 1 << 4
    AMP_LEVEL_PWR_REFLECTED = 1 << 5

    def __init__(self, stato, model, port, libname=None, period=0.2):
        super().__init__(name="amp-hamlib")
        self.stato, self.model, self.port, self.period = stato, model, port, period
        self.libname = libname

    def run(self):
        import ctypes
        import ctypes.util

        name = self.libname or ctypes.util.find_library("hamlib") or "libhamlib-4.dll"
        try:
            lib = ctypes.CDLL(name)
        except OSError as exc:
            log("amplificatore: libreria Hamlib non trovata (%s)" % exc)
            return

        class Value(ctypes.Union):
            _fields_ = [("i", ctypes.c_int), ("f", ctypes.c_float),
                        ("s", ctypes.c_char_p), ("cs", ctypes.c_char_p)]

        lib.amp_init.restype = ctypes.c_void_p
        lib.amp_init.argtypes = [ctypes.c_int]
        lib.amp_open.argtypes = [ctypes.c_void_p]
        lib.amp_close.argtypes = [ctypes.c_void_p]
        lib.amp_get_level.argtypes = [ctypes.c_void_p, ctypes.c_uint64,
                                      ctypes.POINTER(Value)]
        lib.amp_load_all_backends()

        amp = lib.amp_init(self.model)
        if not amp:
            log("amplificatore: modello %d sconosciuto a Hamlib" % self.model)
            return

        # Il percorso della porta sta in amp->state.ampport.pathname. La sua
        # posizione dipende dalla versione di Hamlib, quindi si passa dalla
        # configurazione, che e' stabile.
        try:
            lib.amp_set_conf.argtypes = [ctypes.c_void_p, ctypes.c_uint64,
                                         ctypes.c_char_p]
            lib.amp_token_lookup.restype = ctypes.c_uint64
            lib.amp_token_lookup.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
            tok = lib.amp_token_lookup(amp, b"rig_pathname")
            if tok:
                lib.amp_set_conf(amp, tok, self.port.encode())
        except Exception as exc:
            log("amplificatore: impostazione porta non riuscita (%s)" % exc)

        rc = lib.amp_open(amp)
        if rc != 0:
            log("amplificatore: apertura fallita (codice %d). "
                "La porta e' libera? Il software del costruttore va chiuso." % rc)
            return
        log("amplificatore: aperto su %s" % self.port)

        muto = 0
        while True:
            letti = 0
            vals = {}
            for nome, bit in (("fwd", self.AMP_LEVEL_PWR_FWD),
                              ("ref", self.AMP_LEVEL_PWR_REFLECTED),
                              ("swr", self.AMP_LEVEL_SWR)):
                v = Value()
                if lib.amp_get_level(amp, ctypes.c_uint64(bit), ctypes.byref(v)) == 0:
                    vals[nome] = float(v.f)
                    letti += 1
            with self.stato.lock:
                self.stato.amp_ok = letti > 0
                if "fwd" in vals:
                    self.stato.fwd = vals["fwd"]
                if "ref" in vals:
                    self.stato.ref = vals["ref"]
                if "swr" in vals:
                    self.stato.swr = max(1.0, vals["swr"])
            if letti == 0:
                muto += 1
                if muto == 5:
                    log("amplificatore: non risponde a nessuna grandezza. "
                        "La lettura via Hamlib non e' praticabile su questo apparato.")
            else:
                muto = 0
            time.sleep(self.period)



# ------------------------------------------- sorgente: SPE Expert (protocollo)
# Protocollo dalla Application Programmer's Guide di SPE; vedi
# doc/protocollo-spe-expert.md. Richiesta di stato:
#   0x55 0x55 0x55 0x01 0x90 0x90
# Risposta: 0xAA 0xAA 0xAA 0x43 <67 caratteri separati da virgola> CHK CHK CR LF
SPE_RICHIESTA = bytes([0x55, 0x55, 0x55, 0x01, 0x90, 0x90])


def spe_analizza(trama):
    """Estrae (tx, watt, ros) dalla stringa di stato. None se non valida.

    La verifica della somma di controllo non e' pignoleria: su una seriale
    disturbata una trama mutila darebbe letture assurde proprio mentre si
    trasmette a piena potenza.
    """
    i = trama.find(bytes([0xAA, 0xAA, 0xAA]))
    if i < 0 or len(trama) < i + 5:
        return None
    n = trama[i + 3]
    corpo = trama[i + 4:i + 4 + n]
    if len(corpo) < n:
        return None
    somma = sum(corpo)
    chk = trama[i + 4 + n:i + 6 + n]
    if len(chk) == 2 and (chk[0] != somma % 256 or chk[1] != (somma // 256) % 256):
        return None

    campi = corpo.decode("latin-1").split(",")
    if len(campi) < 12:
        return None
    try:
        tx = campi[2].strip().upper() == "T"
        watt = float(campi[9].strip() or 0)
        ros = float(campi[11].strip() or 1)      # ROS d'antenna
        if ros < 1.0:
            ros = float(campi[10].strip() or 1)  # ripiego: ROS prima dell'ATU
    except (ValueError, IndexError):
        return None
    return tx, watt, max(1.0, ros)


class SpeAmp(threading.Thread):
    """Amplificatore SPE Expert letto con il protocollo del costruttore."""

    daemon = True

    def __init__(self, stato, port, baud=9600, period=0.2):
        super().__init__(name="amp-spe")
        self.stato, self.port, self.baud, self.period = stato, port, baud, period

    def run(self):
        try:
            import serial
        except ImportError:
            log("amplificatore SPE: serve il modulo pyserial  ->  pip install pyserial")
            return
        try:
            ser = serial.Serial(self.port, self.baud, timeout=0.5)
        except Exception as exc:
            log("amplificatore SPE: apertura di %s fallita (%s). Il software del "
                "costruttore va chiuso: una seriale la apre un solo programma."
                % (self.port, exc))
            return
        log("amplificatore SPE: aperto su %s a %d baud" % (self.port, self.baud))

        muto = 0
        while True:
            try:
                ser.reset_input_buffer()
                ser.write(SPE_RICHIESTA)
                risposta = ser.read(96)
                esito = spe_analizza(risposta)
            except Exception as exc:
                log("amplificatore SPE: %s" % exc)
                esito = None
            if esito:
                tx, watt, ros = esito
                muto = 0
                with self.stato.lock:
                    self.stato.amp_ok = True
                    self.stato.fwd = watt if tx else 0.0
                    self.stato.swr = ros if tx else 1.0
                    rho = (ros - 1) / (ros + 1)
                    self.stato.ref = self.stato.fwd * rho * rho
            else:
                muto += 1
                if muto == 10:
                    log("amplificatore SPE: nessuna risposta valida. Porta giusta? "
                        "Velocita' giusta? Software del costruttore chiuso?")
                with self.stato.lock:
                    self.stato.amp_ok = False
            time.sleep(self.period)



class SpeListen(threading.Thread):
    """Ascolto PASSIVO del dialogo fra l'amplificatore e il suo software.

    Il software del costruttore interroga gia' l'amplificatore piu' volte al
    secondo. Se la porta e' rispecchiata su una seconda porta virtuale, qui
    basta leggere: non si invia nulla, quindi non c'e' contesa e il software
    SPE continua a funzionare come sempre.

    E' la modalita' da preferire quando la USB deve restare al costruttore.
    """

    daemon = True

    def __init__(self, stato, port, baud=9600):
        super().__init__(name="amp-spe-listen")
        self.stato, self.port, self.baud = stato, port, baud

    def run(self):
        try:
            import serial
        except ImportError:
            log("ascolto SPE: serve il modulo pyserial  ->  pip install pyserial")
            return
        try:
            ser = serial.Serial(self.port, self.baud, timeout=0.3)
        except Exception as exc:
            log("ascolto SPE: apertura di %s fallita (%s)" % (self.port, exc))
            return
        log("ascolto SPE passivo su %s a %d baud: non viene inviato nulla"
            % (self.port, self.baud))

        buf = b""
        ultimo = 0.0
        while True:
            try:
                buf += ser.read(256)
            except Exception as exc:
                log("ascolto SPE: %s" % exc)
                time.sleep(1)
                continue
            if len(buf) > 4096:
                buf = buf[-1024:]

            # Estrae ogni trama completa che passa, scartando il resto: sulla
            # linea viaggiano anche le richieste del software SPE.
            while True:
                i = buf.find(bytes([0xAA, 0xAA, 0xAA]))
                if i < 0 or len(buf) < i + 4:
                    break
                n = buf[i + 3]
                fine = i + 6 + n
                if len(buf) < fine:
                    break
                esito = spe_analizza(buf[i:fine])
                buf = buf[fine:]
                if not esito:
                    continue
                tx, watt, ros = esito
                ultimo = time.time()
                with self.stato.lock:
                    self.stato.amp_ok = True
                    self.stato.fwd = watt if tx else 0.0
                    self.stato.swr = ros if tx else 1.0
                    rho = (ros - 1) / (ros + 1)
                    self.stato.ref = self.stato.fwd * rho * rho

            if ultimo and time.time() - ultimo > 5:
                with self.stato.lock:
                    self.stato.amp_ok = False
                ultimo = 0
                log("ascolto SPE: nessuna trama da 5 s. Il software del "
                    "costruttore sta interrogando l'amplificatore?")


# --------------------------------------------------------------- server TCI
GUID = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def ws_accept(key):
    return base64.b64encode(hashlib.sha1(key.encode() + GUID).digest()).decode()


def ws_frame(text):
    payload = text.encode("utf-8")
    n = len(payload)
    if n < 126:
        head = struct.pack("!BB", 0x81, n)
    elif n < 65536:
        head = struct.pack("!BBH", 0x81, 126, n)
    else:
        head = struct.pack("!BBQ", 0x81, 127, n)
    return head + payload


def ws_read(sock):
    """Legge un frame di testo. Restituisce None alla chiusura."""
    def recv(n):
        buf = b""
        while len(buf) < n:
            c = sock.recv(n - len(buf))
            if not c:
                return None
            buf += c
        return buf

    h = recv(2)
    if not h:
        return None
    op = h[0] & 0x0F
    masked = h[1] & 0x80
    n = h[1] & 0x7F
    if n == 126:
        d = recv(2)
        if not d:
            return None
        n = struct.unpack("!H", d)[0]
    elif n == 127:
        d = recv(8)
        if not d:
            return None
        n = struct.unpack("!Q", d)[0]
    mask = recv(4) if masked else b""
    data = recv(n) if n else b""
    if data is None:
        return None
    if masked:
        data = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
    if op == 0x8:
        return None
    if op != 0x1:
        return ""
    return data.decode("utf-8", "replace")


class Client(threading.Thread):
    daemon = True

    def __init__(self, sock, addr, stato, args):
        super().__init__(name="tci-%s" % (addr,))
        self.sock, self.addr, self.stato, self.args = sock, addr, stato, args
        self.sensors = False
        self.alive = True

    def send(self, text):
        try:
            self.sock.sendall(ws_frame(text))
        except Exception:
            self.alive = False

    def handshake(self):
        data = b""
        while b"\r\n\r\n" not in data:
            c = self.sock.recv(1024)
            if not c:
                return False
            data += c
            if len(data) > 8192:
                return False
        key = None
        for line in data.decode("latin-1").split("\r\n"):
            if line.lower().startswith("sec-websocket-key:"):
                key = line.split(":", 1)[1].strip()
        if not key:
            return False
        self.sock.sendall((
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n\r\n" % ws_accept(key)).encode())
        return True

    def greeting(self):
        s = self.stato.snapshot()
        # La sequenza che Decodium si aspetta per considerarsi collegato.
        # Dichiararsi ExpertSDR3 non e' vezzo: e' cio' che induce Decodium a
        # chiedere lo stream dei sensori con tx_sensors_enable.
        for m in [
            "protocol:ExpertSDR3,1.9;",
            "version:ExpertSDR3;",
            "device:%s;" % self.args.device,
            "receive_only:false;",
            "trx_count:1;",
            "channels_count:2;",
            "vfo_limits:1500000,55000000;",
            "if_limits:-48000,48000;",
            "modulations_list:am,sam,dsb,lsb,usb,cw,nfm,digl,digu;",
            "trx:0,false;",
            "tune:0,false;",
            "dds:0,0;",
            "if:0,0,0;",
            "vfo:0,0,%d;" % s["freq"],
            "modulation:0,%s;" % s["mode"],
            "rx_enable:0,true;",
            "tx_enable:0,true;",
            "mute:false;",
            "start;",
            "ready;",
        ]:
            self.send(m)

    def run(self):
        try:
            if not self.handshake():
                return
            log("TCI: client collegato da %s" % (self.addr,))
            self.greeting()
            threading.Thread(target=self.pump, daemon=True).start()
            while self.alive:
                msg = ws_read(self.sock)
                if msg is None:
                    break
                for cmd in [c for c in msg.split(";") if c.strip()]:
                    self.handle(cmd.strip())
        except Exception as exc:
            log("TCI: %s" % exc)
        finally:
            self.alive = False
            try:
                self.sock.close()
            except Exception:
                pass
            log("TCI: client %s scollegato" % (self.addr,))

    def handle(self, cmd):
        name, _, rest = cmd.partition(":")
        name = name.strip().lower()
        if name == "tx_sensors_enable":
            self.sensors = rest.split(",")[0].strip().lower() == "true"
            log("TCI: stream sensori %s" % ("attivato" if self.sensors else "spento"))
            self.send("tx_sensors_enable:%s;" % ("true" if self.sensors else "false"))
        elif name in ("rx_sensors_enable", "rx_channel_sensors_enable"):
            self.send("%s:%s;" % (name, rest))
        elif name == "vfo":
            self.send("vfo:%s;" % rest)
        elif name in ("trx", "tune", "modulation", "drive", "tune_drive",
                      "rx_enable", "mute", "split_enable", "rit_enable"):
            self.send("%s:%s;" % (name, rest))
        # Il ponte non comanda la radio: inoltra soltanto. I comandi di
        # scrittura si accolgono per non far fallire il collegamento.

    def pump(self):
        """Invia la telemetria a cadenza fissa: i sensori TCI sono push."""
        last_vfo = None
        while self.alive:
            s = self.stato.snapshot()
            if s["freq"] != last_vfo:
                self.send("vfo:0,0,%d;" % s["freq"])
                self.send("modulation:0,%s;" % s["mode"])
                last_vfo = s["freq"]
            self.send("trx:0,%s;" % ("true" if s["ptt"] else "false"))
            if s["ptt"]:
                # Valori a singolo campo: non ambigui. La disposizione dei
                # campi dentro tx_sensors varia fra implementazioni, e un
                # equivoco fra diretta e riflessa qui non e' accettabile.
                self.send("tx_power:%.1f;" % s["fwd"])
                self.send("tx_swr:%.2f;" % s["swr"])
            time.sleep(self.args.rate)


def log(msg):
    print("[%s] %s" % (time.strftime("%H:%M:%S"), msg), flush=True)


def main():
    ap = argparse.ArgumentParser(description="Ponte SPE/amplificatore -> TCI per Decodium")
    ap.add_argument("--listen", default="127.0.0.1:50001",
                    help="dove ascoltare (default 127.0.0.1:50001)")
    ap.add_argument("--rigctld", default="",
                    help="radio da inoltrare, es. 127.0.0.1:4533 (facoltativo)")
    ap.add_argument("--amp", default="demo",
                    help="'demo', 'spe:<porta>[:<baud>]' oppure "
                         "'hamlib:<modello>:<porta>'. Es. spe:COM7 · hamlib:401:COM7")
    ap.add_argument("--device", default="SPE-Bridge", help="nome annunciato")
    ap.add_argument("--rate", type=float, default=0.2,
                    help="cadenza della telemetria in secondi (default 0.2 = 5 Hz)")
    ap.add_argument("--watt", type=float, default=400.0,
                    help="potenza di picco del simulatore")
    ap.add_argument("--simulate-tx", action="store_true",
                    help="alterna trasmissione e ricezione ogni 8 s: serve a "
                         "collaudare la catena senza mandare in aria la radio")
    args = ap.parse_args()

    stato = Stato()

    if args.simulate_tx:
        def alterna():
            while True:
                for on in (True, False):
                    with stato.lock:
                        stato.ptt = on
                    time.sleep(8)
        threading.Thread(target=alterna, daemon=True).start()
        log("trasmissione SIMULATA: alterna ogni 8 s, la radio non va in aria")

    if args.rigctld:
        h, _, p = args.rigctld.partition(":")
        RigctldSource(stato, h or "127.0.0.1", int(p or 4532),
                      simulate_tx=args.simulate_tx).start()
    else:
        log("radio: nessun rigctld indicato, frequenza e modo restano fissi")

    if args.amp == "demo":
        log("amplificatore: SIMULATO (%.0f W di picco) - serve a provare la catena"
            % args.watt)
        DemoAmp(stato, args.watt).start()
    elif args.amp.startswith("spe-listen:"):
        parti = args.amp.split(":")
        SpeListen(stato, parti[1], int(parti[2]) if len(parti) > 2 else 9600).start()
    elif args.amp.startswith("spe:"):
        parti = args.amp.split(":")
        porta = parti[1]
        baud = int(parti[2]) if len(parti) > 2 else 9600
        SpeAmp(stato, porta, baud).start()
    elif args.amp.startswith("hamlib:"):
        _, model, port = args.amp.split(":", 2)
        HamlibAmp(stato, int(model), port).start()
    else:
        log("sorgente amplificatore sconosciuta: %s" % args.amp)
        return 2

    host, _, port = args.listen.partition(":")
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Su Windows SO_REUSEADDR permette a un secondo processo di legare la
    # STESSA porta: il primo continua a rispondere e il secondo sembra partito
    # ma non serve nessuno. Si preferisce fallire a voce alta.
    if os.name == "nt":
        try:
            srv.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        except (AttributeError, OSError):
            pass
    else:
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        srv.bind((host or "127.0.0.1", int(port or 50001)))
    except OSError as exc:
        log("porta %s gia' occupata: %s" % (port, exc))
        log("un altro ponte e' gia' in esecuzione? Chiuderlo, o usare --listen "
            "con una porta diversa.")
        return 3
    srv.listen(8)
    log("TCI in ascolto su %s:%s - in Decodium: backend CAT = TCI, questo indirizzo"
        % (host, port))

    try:
        while True:
            sock, addr = srv.accept()
            Client(sock, addr, stato, args).start()
    except KeyboardInterrupt:
        log("chiusura")
    return 0


if __name__ == "__main__":
    sys.exit(main())
