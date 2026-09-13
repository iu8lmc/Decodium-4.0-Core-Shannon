#!/usr/bin/env python3
"""Client di prova DecoPort — scopre le radio annunciate, si collega a una e,
se richiesto, le cambia frequenza.

Serve a dimostrare il protocollo senza passare dall'interfaccia di Decodium, e a
collaudare un gateway da un'altra macchina prima di fidarsi.

    python tools/decoport_probe.py --listen 15
    python tools/decoport_probe.py --connect 192.168.1.81 --seconds 20
    python tools/decoport_probe.py --connect 192.168.1.81 --tune 14074000

Protocollo: doc/DECOPORT_PROTOCOL.md.
"""
import argparse
import hashlib
import hmac
import socket
import struct
import sys
import time

MAGIC = 0x44505254          # 'DPRT'
VERSION = 1
HEADER = 28
SESSION_PORT = 5559
ANNOUNCE_PORT = 5560

ANNOUNCE, HELLO, BYE, KEEPALIVE, CONTEXT, COMMAND, AUDIO_RX, AUDIO_TX, STATUS = range(1, 10)

FLAG_TIMESTAMP = 1 << 0
FLAG_AUTH = 1 << 1
AUTH_TAG_BYTES = 16
# Stesso sale e stesse iterazioni del lato Decodium: la chiave deve venire
# identica sulle due macchine, altrimenti non si parlano.
KDF_SALT = b"Decodium-DecoPort-v1"
KDF_ITERATIONS = 200000

AUTH_KEY = b""


def derive_key(password):
    if not password:
        return b""
    return hashlib.pbkdf2_hmac("sha256", password.strip().encode("utf-8"),
                               KDF_SALT, KDF_ITERATIONS, 32)

TYPE_NAMES = {
    ANNOUNCE: "ANNOUNCE", HELLO: "HELLO", BYE: "BYE", KEEPALIVE: "KEEPALIVE",
    CONTEXT: "CONTEXT", COMMAND: "COMMAND", AUDIO_RX: "AUDIO_RX",
    AUDIO_TX: "AUDIO_TX", STATUS: "STATUS",
}

FIELDS = [
    (1 << 0,  "frequencyHz",   ">q", 8),
    (1 << 1,  "mode",          ">B", 1),
    (1 << 2,  "ptt",           ">B", 1),
    (1 << 3,  "sMeterTenths",  ">H", 2),
    (1 << 4,  "sampleRate",    ">I", 4),
    (1 << 5,  "channels",      ">B", 1),
    (1 << 6,  "bandwidthHz",   ">I", 4),
    (1 << 7,  "rigLabel",      None, 0),     # uint8 len + utf8
    (1 << 8,  "stateFlags",    ">I", 4),
    (1 << 9,  "txAudioLeadMs", ">H", 2),
    (1 << 10, "sessionPort",   ">H", 2),
]

MODES = ["UNKNOWN", "USB", "LSB", "CW", "CWR", "AM", "FM",
         "DIGU", "DIGL", "RTTY", "RTTYR", "PKTFM"]

STATE_BITS = [(1 << 0, "CAT"), (1 << 1, "AUDIO-IN"), (1 << 2, "AUDIO-OUT"),
              (1 << 3, "CAN-TX"), (1 << 4, "TX-HELD")]


def now_ns():
    return int(time.time() * 1_000_000_000)


def build(ptype, stream_id=0, sequence=0, ts_ns=None, payload=b""):
    if ts_ns is None:
        ts_ns = now_ns()
    flags = FLAG_TIMESTAMP if ts_ns else 0
    if AUTH_KEY:
        flags |= FLAG_AUTH
    pkt = struct.pack(">IBBHIIIIHH",
                      MAGIC, VERSION, ptype, flags, stream_id, sequence,
                      ts_ns // 1_000_000_000, ts_ns % 1_000_000_000,
                      len(payload), 0) + payload
    if AUTH_KEY:
        pkt += hmac.new(AUTH_KEY, pkt, hashlib.sha256).digest()[:AUTH_TAG_BYTES]
    return pkt


def parse(datagram):
    if len(datagram) < HEADER:
        return None
    magic, version, ptype, flags, stream, seq, secs, nanos, plen, _ = \
        struct.unpack(">IBBHIIIIHH", datagram[:HEADER])
    if magic != MAGIC or version != VERSION:
        return None
    if HEADER + plen > len(datagram):
        return None

    authed = False
    if flags & FLAG_AUTH:
        signed_len = HEADER + plen
        if len(datagram) < signed_len + AUTH_TAG_BYTES:
            return None
        if AUTH_KEY:
            expected = hmac.new(AUTH_KEY, datagram[:signed_len],
                                hashlib.sha256).digest()[:AUTH_TAG_BYTES]
            authed = hmac.compare_digest(expected,
                                         datagram[signed_len:signed_len + AUTH_TAG_BYTES])
    return {
        "type": ptype, "flags": flags, "streamId": stream, "sequence": seq,
        "tsNs": secs * 1_000_000_000 + nanos,
        "payload": datagram[HEADER:HEADER + plen],
        "authenticated": authed,
    }


def decode_context(payload):
    if len(payload) < 4:
        return {}
    mask = struct.unpack(">I", payload[:4])[0]
    off = 4
    out = {"mask": mask}
    for bit, name, fmt, size in FIELDS:
        if not (mask & bit):
            continue
        if name == "rigLabel":
            if off + 1 > len(payload):
                break
            length = payload[off]
            off += 1
            out[name] = payload[off:off + length].decode("utf-8", "replace")
            off += length
            continue
        if off + size > len(payload):
            break
        out[name] = struct.unpack(fmt, payload[off:off + size])[0]
        off += size
    return out


def encode_context(**kw):
    mask = 0
    body = b""
    for bit, name, fmt, _size in FIELDS:
        if name not in kw:
            continue
        mask |= bit
        if name == "rigLabel":
            data = kw[name].encode("utf-8")[:255]
            body += bytes([len(data)]) + data
        else:
            body += struct.pack(fmt, kw[name])
    return struct.pack(">I", mask) + body


def describe(ctx):
    bits = [n for b, n in STATE_BITS if ctx.get("stateFlags", 0) & b]
    # Il gateway omette i campi che non conosce: senza CAT in linea non c'e'
    # ne' modo ne' frequenza, e il client deve reggerlo invece di rompersi.
    mode_id = ctx.get("mode", 0)
    mode = MODES[mode_id] if mode_id < len(MODES) else "?"
    freq = ctx.get("frequencyHz", 0)
    return ("%-28s  %10.3f kHz  %-6s  ptt=%d  [%s]"
            % (ctx.get("rigLabel", "?"), freq / 1000.0, mode,
               ctx.get("ptt", 0), " ".join(bits) or "-"))


def do_listen(seconds):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", ANNOUNCE_PORT))
    sock.settimeout(1.0)
    print("in ascolto degli annunci su %d per %d s...\n" % (ANNOUNCE_PORT, seconds))
    seen = {}
    deadline = time.time() + seconds
    while time.time() < deadline:
        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout:
            continue
        pkt = parse(data)
        if not pkt or pkt["type"] != ANNOUNCE:
            continue
        if not pkt["authenticated"]:
            # Annuncio non verificabile: password sbagliata o assente.
            continue
        ctx = decode_context(pkt["payload"])
        key = (addr[0], pkt["streamId"])
        if key not in seen:
            print("TROVATA  %s:%d   %s" % (addr[0], ctx.get("sessionPort", SESSION_PORT),
                                           describe(ctx)))
        seen[key] = ctx
    if not seen:
        if AUTH_KEY:
            print("nessun annuncio verificato. Gateway acceso? Password giusta?")
        else:
            print("nessun annuncio. Senza --password non si vede nulla: DecoPort")
            print("non pubblica in chiaro. Il gateway e' acceso? Stessa rete?")
        return 1
    print("\n%d radio annunciate." % len(seen))
    return 0


def do_connect(host, port, seconds, tune_hz, mode_name):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("", 0))
    sock.settimeout(0.5)
    peer = (host, port)

    print("HELLO -> %s:%d" % peer)
    sock.sendto(build(HELLO), peer)

    last_keepalive = time.time()
    audio_packets = 0
    audio_samples = 0
    contexts = 0
    commanded = False
    first_ctx = None
    deadline = time.time() + seconds

    while time.time() < deadline:
        if time.time() - last_keepalive > 2.0:
            sock.sendto(build(KEEPALIVE), peer)
            last_keepalive = time.time()

        try:
            data, _ = sock.recvfrom(4096)
        except socket.timeout:
            continue
        pkt = parse(data)
        if not pkt or not pkt["authenticated"]:
            continue

        if pkt["type"] in (CONTEXT, STATUS):
            ctx = decode_context(pkt["payload"])
            contexts += 1
            if first_ctx is None:
                first_ctx = ctx
                print("COLLEGATO  %s" % describe(ctx))
            elif contexts % 8 == 0:
                print("           %s" % describe(ctx))

            # Il comando parte solo dopo il primo contesto: prima non sappiamo
            # nemmeno se la radio abbia il CAT in linea.
            if tune_hz and not commanded:
                if not (ctx.get("stateFlags", 0) & 1):
                    print("!! il gateway dichiara il CAT NON in linea: non comando nulla")
                    commanded = True
                else:
                    print("COMMAND -> frequenza %.3f kHz" % (tune_hz / 1000.0))
                    sock.sendto(build(COMMAND, payload=encode_context(frequencyHz=tune_hz)), peer)
                    commanded = True
            if mode_name and commanded and mode_name in MODES:
                sock.sendto(build(COMMAND, payload=encode_context(mode=MODES.index(mode_name))), peer)
                mode_name = None

        elif pkt["type"] == AUDIO_RX:
            audio_packets += 1
            audio_samples += len(pkt["payload"]) // 2

    sock.sendto(build(BYE), peer)

    rate = (first_ctx or {}).get("sampleRate", 0)
    print("\n--- riepilogo ---")
    print("contesti ricevuti : %d" % contexts)
    if rate:
        heard = audio_samples / float(rate)
        print("pacchetti audio   : %d (%d campioni, %.2f s dichiarati a %d Hz)"
              % (audio_packets, audio_samples, heard, rate))
        # Il rapporto fra audio ricevuto e tempo trascorso e' la misura che
        # conta: 1.00 vuol dire tempo reale, sotto vuol dire che si perde roba.
        print("resa              : %.3f  (1.000 = tempo reale)" % (heard / float(seconds)))
    else:
        print("pacchetti audio   : %d (%d campioni; il gateway non ha dichiarato la frequenza)"
              % (audio_packets, audio_samples))
    if contexts == 0:
        print("\nnessun contesto: il gateway non ha risposto. Porta giusta? Firewall?")
        return 1
    return 0


# ── radio finta ─────────────────────────────────────────────────────────────
# Una radio che non esiste, che si annuncia e manda un tono: serve a provare il
# lato client — scoperta, collegamento, audio nel decoder — senza una seconda
# radio e senza un secondo computer.

def do_serve(port, seconds, label, freq_hz, tone_hz):
    import math

    ann = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    ann.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    ses = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    ses.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    ses.bind(("", port))
    ses.settimeout(0.01)

    rate = 12000
    frame = rate * 10 // 1000          # 10 ms, come il gateway vero
    state = {"freq": freq_hz, "mode": MODES.index("DIGU"), "ptt": 0}
    tx = {"frames": 0, "samples": 0, "early": 0, "late": 0, "first": None,
          "peak": 0, "keyed_at": None}
    clients = {}
    stream_id = 0x5EED
    seq = {"ann": 0, "ctx": 0, "aud": 0}
    phase = 0.0
    step = 2.0 * math.pi * tone_hz / rate

    def context_payload():
        # CAT + AUDIO-IN + AUDIO-OUT + CAN-TX: la radio finta accetta anche di
        # "trasmettere", cioe' di misurare quello che riceve senza mandarlo in
        # aria. E' il modo di provare la trasmissione senza trasmettere.
        return encode_context(frequencyHz=state["freq"], mode=state["mode"],
                              ptt=1 if state["ptt"] else 0,
                              sMeterTenths=300, sampleRate=rate, channels=1,
                              bandwidthHz=3000, rigLabel=label,
                              stateFlags=(1 << 0) | (1 << 1) | (1 << 2) | (1 << 3),
                              txAudioLeadMs=200, sessionPort=port)

    print("radio finta '%s' su porta %d, tono %d Hz - %d s" % (label, port, tone_hz, seconds))
    t_end = time.time() + seconds
    t_ann = 0.0
    t_aud = time.time()
    while time.time() < t_end:
        now = time.time()
        if now - t_ann >= 2.0:
            t_ann = now
            seq["ann"] += 1
            pkt = build(ANNOUNCE, stream_id, seq["ann"], payload=context_payload())
            for target in ("127.0.0.1", "255.255.255.255"):
                try:
                    ann.sendto(pkt, (target, ANNOUNCE_PORT))
                except OSError:
                    pass

        try:
            data, addr = ses.recvfrom(65535)
            msg = parse(data)
            if msg and (not AUTH_KEY or msg["authenticated"]):
                t = msg["type"]
                if t == HELLO:
                    if not clients:
                        # L'orologio dell'audio riparte da adesso: altrimenti si
                        # rovescerebbe addosso al client l'attesa fatta a vuoto.
                        t_aud = now
                    clients[addr] = now
                    print("  <- HELLO da %s:%d" % addr)
                    seq["ctx"] += 1
                    ses.sendto(build(CONTEXT, stream_id, seq["ctx"],
                                     payload=context_payload()), addr)
                elif t == KEEPALIVE:
                    clients[addr] = now
                elif t == BYE:
                    clients.pop(addr, None)
                    print("  <- BYE da %s:%d" % addr)
                elif t == AUDIO_TX:
                    # Non si suona niente: si misura. Quanto audio, quanto in
                    # anticipo rispetto all'ora dichiarata, e quanto forte.
                    body = msg["payload"]
                    n = len(body) // 2
                    tx["frames"] += 1
                    tx["samples"] += n
                    if tx["first"] is None:
                        tx["first"] = now
                    ahead_ms = (msg["tsNs"] - now_ns()) / 1e6
                    if ahead_ms < 0:
                        tx["late"] += 1
                    elif ahead_ms > 2000:
                        tx["early"] += 1
                    for i in range(0, len(body), 2):
                        v = struct.unpack_from("<h", body, i)[0]
                        if abs(v) > tx["peak"]:
                            tx["peak"] = abs(v)
                elif t == COMMAND:
                    cmd = decode_context(msg["payload"])
                    if "ptt" in cmd:
                        state["ptt"] = int(cmd["ptt"])
                        if state["ptt"]:
                            tx.update({"frames": 0, "samples": 0, "early": 0,
                                       "late": 0, "first": None, "peak": 0,
                                       "keyed_at": now})
                            print("  <- PTT SU (niente va in aria: e' una radio finta)")
                        else:
                            held = (now - tx["keyed_at"]) if tx["keyed_at"] else 0.0
                            print("  <- PTT GIU' dopo %.2f s | %d frame, %.2f s di audio, "
                                  "picco %d, %d in ritardo, %d troppo in anticipo"
                                  % (held, tx["frames"], tx["samples"] / rate,
                                     tx["peak"], tx["late"], tx["early"]))
                    if "frequencyHz" in cmd:
                        state["freq"] = cmd["frequencyHz"]
                        print("  <- TUNE %.3f kHz" % (state["freq"] / 1000.0))
                    if "mode" in cmd:
                        state["mode"] = cmd["mode"]
                        print("  <- MODE %s" % MODES[state["mode"]])
                    seq["ctx"] += 1
                    for c in clients:
                        ses.sendto(build(CONTEXT, stream_id, seq["ctx"],
                                         payload=context_payload()), c)
        except socket.timeout:
            pass
        except OSError:
            pass

        # L'audio esce a tempo reale: un frame ogni 10 ms, non a raffica.
        while clients and time.time() - t_aud >= 0.010:
            t_aud += 0.010
            buf = bytearray()
            for _ in range(frame):
                v = int(9000 * math.sin(phase))
                phase += step
                buf += struct.pack("<h", v)
            seq["aud"] += 1
            pkt = build(AUDIO_RX, stream_id, seq["aud"], payload=bytes(buf))
            for c in list(clients):
                if now - clients[c] > 15:
                    del clients[c]
                    continue
                try:
                    ses.sendto(pkt, c)
                except OSError:
                    pass
        time.sleep(0.002)

    print("fine: %d frame audio inviati" % seq["aud"])
    return 0


def main():
    ap = argparse.ArgumentParser(description="Client di prova DecoPort")
    ap.add_argument("--listen", type=int, metavar="SEC",
                    help="ascolta gli annunci per SEC secondi ed esci")
    ap.add_argument("--connect", metavar="HOST", help="collegati a questo gateway")
    ap.add_argument("--port", type=int, default=SESSION_PORT)
    ap.add_argument("--seconds", type=int, default=15, help="durata del collegamento")
    ap.add_argument("--tune", type=int, metavar="HZ",
                    help="dopo il collegamento, chiedi questa frequenza")
    ap.add_argument("--mode", metavar="NAME", help="e questo modo (USB, DIGU, ...)")
    ap.add_argument("--password", metavar="PW",
                    help="password DecoPort: senza, il gateway non risponde")
    ap.add_argument("--key-from-ini", metavar="FILE",
                    help="prendi la chiave gia' derivata da un Decodium3.ini "
                         "(voce DecoPortKey): serve a provare in locale senza "
                         "conoscere la password")
    ap.add_argument("--serve", action="store_true",
                    help="fai la radio: annunciati e manda un tono, per provare un client")
    ap.add_argument("--label", default="DecoPort Test Rig",
                    help="nome della radio finta")
    ap.add_argument("--freq", type=int, default=14074000,
                    help="frequenza dichiarata dalla radio finta")
    ap.add_argument("--tone", type=int, default=1500,
                    help="tono generato, in Hz sull'audio")
    args = ap.parse_args()

    global AUTH_KEY
    if args.password:
        print("derivo la chiave dalla password...")
        AUTH_KEY = derive_key(args.password)
    elif args.key_from_ini:
        # La chiave sta nell'INI gia' derivata: la si prende com'e', non la si
        # stampa e non la si scrive da nessuna parte.
        import re as _re
        key_hex = ""
        with open(args.key_from_ini, "r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                # La voce puo' stare dentro un profilo: "FT991A\DecoPortKey=..."
                m = _re.search(r"DecoPortKey\s*=\s*(\S+)", line)
                if m:
                    key_hex = m.group(1).strip().strip('"')
        if not key_hex:
            print("DecoPortKey non trovata in " + args.key_from_ini)
            return 2
        AUTH_KEY = bytes.fromhex(key_hex)
        print("chiave presa dall'INI (%d byte)" % len(AUTH_KEY))

    if args.serve:
        return do_serve(args.port, args.seconds, args.label, args.freq, args.tone)
    if args.listen:
        return do_listen(args.listen)
    if args.connect:
        return do_connect(args.connect, args.port, args.seconds, args.tune, args.mode)
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
