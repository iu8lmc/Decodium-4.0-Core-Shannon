#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Una radio finta su TCP, per provare il multiplexer CAT di DecoPort senza
mettere niente in aria.

    python tools/radio_sim.py --rig ft991a --listen 7001
    python tools/radio_sim.py --rig ic7300 --listen 7002 --echo
    python tools/radio_sim.py --rig ts2000 --listen 7003
    python tools/radio_sim.py --rig ft817  --listen 7004

Il multiplexer la apre come "tcp:127.0.0.1:7001", proprio come aprirebbe un
server seriale in rete. Le risposte seguono i simulatori di Hamlib
(simulators/sim*.c), con le correzioni che servono a somigliare a una radio
vera: l'IC-7300 risponde al controller (non a se stesso), l'FT-991A dice
ID0670, i comandi sconosciuti hanno la loro risposta d'errore.

Su stdout, una riga per evento: "CMD ...", "PTT 1", "PTT 0". I banchi di prova
leggono quelle.
"""

import argparse
import socket
import sys
import threading
import time


def log(*parts):
    print(time.strftime("%H:%M:%S"), *parts, flush=True)


# ── Yaesu newcat (FT-991A) ───────────────────────────────────────────────────

class Newcat:
    terminator = b";"

    def __init__(self):
        self.fa = 14074000
        self.fb = 14074500
        self.mode = "C"      # DATA-USB
        self.tx = 0
        self.ai = 0
        self.ft = "0"
        self.vs = "0"
        self.st = "0"
        self.pc = 100
        self.ks = 20
        self.sh = 21
        self.na = 0
        self.menu = {}

    def info(self):
        # IF + memoria(3) + frequenza(9) + clarifier(+0000) + RX/TX clar(2)
        #    + modo(1) + VFO(1) + CTCSS(1) + 00 + shift(1)
        return "IF001%09d+000000%s00000;" % (self.fa, self.mode)

    def handle(self, cmd):
        """Restituisce (risposta, eventi AI)."""
        c, p = cmd[:2], cmd[2:-1]
        ai = []
        if c == "FA":
            if not p:
                return "FA%09d;" % self.fa, ai
            self.fa = int(p)
            if self.ai:
                ai.append("FA%09d;" % self.fa)
            return None, ai
        if c == "FB":
            if not p:
                return "FB%09d;" % self.fb, ai
            self.fb = int(p)
            return None, ai
        if c == "MD":
            if p == "0":
                return "MD0%s;" % self.mode, ai
            if len(p) == 2 and p[0] == "0":
                self.mode = p[1]
                if self.ai:
                    ai.append("MD0%s;" % self.mode)
                return None, ai
        if c == "IF" and not p:
            return self.info(), ai
        if c == "ID" and not p:
            return "ID0670;", ai
        if c == "PS" and not p:
            return "PS1;", ai
        if c == "AI":
            if not p:
                return "AI%d;" % self.ai, ai
            self.ai = int(p)
            return None, ai
        if c == "TX":
            if not p:
                return "TX%d;" % self.tx, ai
            new = 0 if p == "0" else 1
            if new != self.tx:
                self.tx = new
                log("PTT", new)
            return None, ai
        if c == "FT":
            if not p:
                return "FT%s;" % self.ft, ai
            self.ft = "1" if p == "3" else "0"
            return None, ai
        if c == "VS":
            if not p:
                return "VS%s;" % self.vs, ai
            self.vs = p
            return None, ai
        if c == "ST":
            if not p:
                return "ST%s;" % self.st, ai
            self.st = p
            return None, ai
        if c == "PC":
            if not p:
                return "PC%03d;" % self.pc, ai
            self.pc = int(p)
            return None, ai
        if c == "KS":
            if not p:
                return "KS%03d;" % self.ks, ai
            self.ks = int(p)
            return None, ai
        if c == "SH" and p.startswith("0"):
            if p == "0":
                return "SH0%02d;" % self.sh, ai
            self.sh = int(p[1:])
            return None, ai
        if c == "NA" and p.startswith("0"):
            if p == "0":
                return "NA0%d;" % self.na, ai
            self.na = int(p[1:])
            return None, ai
        if c == "EX" and len(p) >= 3:
            # menu a tre cifre: "EX032;" legge, "EX0321;" imposta e non risponde
            if len(p) == 3:
                return "EX%s%s;" % (p, self.menu.get(p, "0")), ai
            self.menu[p[:3]] = p[3:]
            return None, ai
        if c == "SM" and p == "0":
            return "SM0000;", ai
        if c == "RM" and len(p) == 1:
            return "RM%s000;" % p, ai
        if c in ("AG", "RG", "SQ") and p == "0":
            return "%s0100;" % c, ai
        return "?;", ai


# ── Kenwood (TS-2000) ────────────────────────────────────────────────────────

class Kenwood:
    terminator = b";"

    def __init__(self):
        self.fa = 7074000
        self.fb = 7076000
        self.mode = 2        # USB
        self.tx = 0
        self.ai = 0

    def handle(self, cmd):
        c, p = cmd[:2], cmd[2:-1]
        ai = []
        if c == "FA":
            if not p:
                return "FA%011d;" % self.fa, ai
            self.fa = int(p)
            if self.ai:
                ai.append("FA%011d;" % self.fa)
            return None, ai
        if c == "FB":
            if not p:
                return "FB%011d;" % self.fb, ai
            self.fb = int(p)
            return None, ai
        if c == "MD":
            if not p:
                return "MD%d;" % self.mode, ai
            self.mode = int(p)
            return None, ai
        if c == "IF" and not p:
            # IF + frequenza(11) + passo(4) + RIT(+00000) + RIT/XIT/banco(3)
            #    + memoria(2) + TX(1) + modo(1) + VFO(1) + scan(1) + split(1)
            #    + tono(1) + numero tono(2) + shift(1): 35 caratteri
            return ("IF%011d" % self.fa + "    " + "+00000" + "000" + "00"
                    + "%d%d" % (self.tx, self.mode) + "0000" + "00" + "0;"), ai
        if c == "ID" and not p:
            return "ID019;", ai
        if c == "PS" and not p:
            return "PS1;", ai
        if c == "AI":
            if not p:
                return "AI%d;" % self.ai, ai
            self.ai = int(p)
            return None, ai
        if c == "TX":
            if self.tx != 1:
                self.tx = 1
                log("PTT", 1)
            return None, ai
        if c == "RX" and not p:
            if self.tx != 0:
                self.tx = 0
                log("PTT", 0)
            return None, ai
        if c == "FR" and not p:
            return "FR0;", ai
        if c == "FT" and not p:
            return "FT0;", ai
        return "?;", ai


# ── Icom CI-V (IC-7300) ──────────────────────────────────────────────────────

def bcd_le(value, nbytes):
    out = bytearray()
    for _ in range(nbytes):
        lo = value % 10
        value //= 10
        hi = value % 10
        value //= 10
        out.append((hi << 4) | lo)
    return bytes(out)


def from_bcd_le(data):
    value = 0
    for b in reversed(data):
        value = value * 100 + ((b >> 4) * 10) + (b & 0x0F)
    return value


class Civ:
    terminator = b"\xfd"

    def __init__(self, addr=0x94, echo=False):
        self.addr = addr
        self.echo = echo
        self.freq = [14074000, 14076000]
        self.mode = [0x01, 0x01]    # USB
        self.data = [0x01, 0x00]    # dati accesi su VFO A
        self.filt = [0x01, 0x01]
        self.ptt = 0
        self.split = 0

    def reply(self, to, payload):
        return bytes([0xFE, 0xFE, to, self.addr]) + payload + b"\xfd"

    def handle(self, frame):
        """frame: FE FE to from cmd ... FD. Restituisce (lista frame da mandare)."""
        out = []
        if self.echo:
            out.append(frame)
        if len(frame) < 6:
            return out
        to, frm, cmd = frame[2], frame[3], frame[4]
        body = frame[5:-1]
        if to not in (self.addr, 0x00):
            return out
        ok = self.reply(frm, b"\xfb")
        ng = self.reply(frm, b"\xfa")
        if cmd == 0x03 and not body:
            out.append(self.reply(frm, b"\x03" + bcd_le(self.freq[0], 5)))
        elif cmd == 0x04 and not body:
            out.append(self.reply(frm, bytes([0x04, self.mode[0], self.filt[0]])))
        elif cmd == 0x05 and len(body) == 5:
            self.freq[0] = from_bcd_le(body)
            out.append(ok)
            out.append(self.reply(0x00, b"\x00" + bcd_le(self.freq[0], 5)))   # transceive
        elif cmd == 0x06 and body:
            self.mode[0] = body[0]
            out.append(ok)
        elif cmd in (0x07, 0x18):
            out.append(ok)
        elif cmd == 0x0F:
            if not body:
                out.append(self.reply(frm, bytes([0x0F, self.split])))
            else:
                self.split = body[0]
                out.append(ok)
        elif cmd == 0x14 and len(body) >= 1:
            if len(body) == 1:
                out.append(self.reply(frm, bytes([0x14, body[0], 0x01, 0x28])))
            else:
                out.append(ok)
        elif cmd == 0x15 and len(body) == 1:
            out.append(self.reply(frm, bytes([0x15, body[0], 0x00, 0x00])))
        elif cmd == 0x16 and len(body) >= 1:
            if len(body) == 1:
                out.append(self.reply(frm, bytes([0x16, body[0], 0x00])))
            else:
                out.append(ok)
        elif cmd == 0x19 and body == b"\x00":
            out.append(self.reply(frm, bytes([0x19, 0x00, self.addr])))
        elif cmd == 0x1A and body:
            if body[0] == 0x06 and len(body) == 1:
                out.append(self.reply(frm, bytes([0x1A, 0x06, self.data[0], self.filt[0]])))
            elif body[0] == 0x03 and len(body) == 1:
                # larghezza del filtro DSP, come indice (Hamlib la chiede all'apertura)
                out.append(self.reply(frm, bytes([0x1A, 0x03, 0x31])))
            elif body[0] == 0x05 and len(body) == 3:
                out.append(self.reply(frm, b"\x1a" + body + b"\x00"))
            elif len(body) > 3 or (body[0] == 0x06 and len(body) > 1):
                if body[0] == 0x06:
                    self.data[0] = body[1]
                out.append(ok)
            else:
                out.append(ng)
        elif cmd == 0x1C and body[:1] == b"\x00":
            if len(body) == 1:
                out.append(self.reply(frm, bytes([0x1C, 0x00, self.ptt])))
            else:
                new = 1 if body[1] else 0
                if new != self.ptt:
                    self.ptt = new
                    log("PTT", new)
                out.append(ok)
        elif cmd == 0x25 and len(body) >= 1:
            vfo = 0 if body[0] == 0 else 1
            if len(body) == 1:
                out.append(self.reply(frm, bytes([0x25, body[0]]) + bcd_le(self.freq[vfo], 5)))
            else:
                self.freq[vfo] = from_bcd_le(body[1:6])
                out.append(ok)
        elif cmd == 0x26 and len(body) >= 1:
            vfo = 0 if body[0] == 0 else 1
            if len(body) == 1:
                out.append(self.reply(frm, bytes([0x26, body[0], self.mode[vfo],
                                                  self.data[vfo], self.filt[vfo]])))
            else:
                self.mode[vfo] = body[1]
                if len(body) > 2:
                    self.data[vfo] = body[2]
                out.append(ok)
        else:
            out.append(ng)
        return out


# ── Yaesu a cinque byte (FT-817) ─────────────────────────────────────────────

class Yaesu5:
    terminator = None

    def __init__(self):
        self.freq = 1407400      # unita' da 10 Hz
        self.mode = 0x01         # USB
        self.ptt = 0

    def handle(self, cmd):
        op = cmd[4]
        if op == 0x03:
            digits = "%08d" % self.freq
            return bytes(int(digits[i:i + 2], 16) for i in range(0, 8, 2)) + bytes([self.mode])
        if op == 0x01:
            self.freq = int(cmd[:4].hex())
            return b""
        if op == 0x07:
            self.mode = cmd[0]
            return b""
        if op in (0x08, 0x88):
            new = 1 if op == 0x08 else 0
            was = self.ptt
            if new != self.ptt:
                self.ptt = new
                log("PTT", new)
            return bytes([0x00 if was != new else 0xF0])
        if op == 0xE7:
            return b"\x08"
        if op == 0xF7:
            return bytes([0x80 if not self.ptt else 0x00])
        if op == 0xBB:
            return b"\x00\x00"
        return b""


# ── servizio ────────────────────────────────────────────────────────────────

def serve_text(conn, rig):
    buf = b""
    while True:
        data = conn.recv(4096)
        if not data:
            return
        buf += data
        while b";" in buf:
            raw, buf = buf.split(b";", 1)
            cmd = raw.decode("latin-1").strip() + ";"
            log("CMD", cmd)
            reply, ai = rig.handle(cmd)
            time.sleep(0.005)
            if reply:
                conn.sendall(reply.encode("latin-1"))
            for a in ai:
                conn.sendall(a.encode("latin-1"))


def serve_civ(conn, rig):
    buf = b""
    while True:
        data = conn.recv(4096)
        if not data:
            return
        buf += data
        while True:
            s = buf.find(b"\xfe\xfe")
            if s < 0:
                buf = b""
                break
            e = buf.find(b"\xfd", s)
            if e < 0:
                buf = buf[s:]
                break
            frame = buf[s:e + 1]
            buf = buf[e + 1:]
            log("CMD", frame.hex(" ").upper())
            time.sleep(0.005)
            for out in rig.handle(frame):
                conn.sendall(out)


def serve_yaesu5(conn, rig):
    buf = b""
    while True:
        data = conn.recv(4096)
        if not data:
            return
        buf += data
        while len(buf) >= 5:
            cmd, buf = buf[:5], buf[5:]
            log("CMD", cmd.hex(" ").upper())
            time.sleep(0.01)
            reply = rig.handle(cmd)
            if reply:
                conn.sendall(reply)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rig", choices=["ft991a", "ic7300", "ts2000", "ft817"], required=True)
    ap.add_argument("--listen", type=int, required=True)
    ap.add_argument("--echo", action="store_true", help="CI-V: ripete ogni comando, come un bus")
    ap.add_argument("--seconds", type=float, default=0, help="esce dopo tanti secondi")
    args = ap.parse_args()

    rig = {"ft991a": Newcat, "ts2000": Kenwood, "ft817": Yaesu5}.get(args.rig)
    rig = rig() if rig else Civ(echo=args.echo)
    serve = {"ft991a": serve_text, "ts2000": serve_text, "ic7300": serve_civ,
             "ft817": serve_yaesu5}[args.rig]

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", args.listen))
    srv.listen(1)
    if args.seconds:
        threading.Timer(args.seconds, lambda: (log("FINE"), sys.stdout.flush(),
                                               __import__("os")._exit(0))).start()
    log("PRONTA", args.rig, args.listen)
    while True:
        conn, _ = srv.accept()
        conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        log("COLLEGATO")
        try:
            serve(conn, rig)
        except OSError:
            pass
        log("SCOLLEGATO")
        conn.close()


if __name__ == "__main__":
    main()
