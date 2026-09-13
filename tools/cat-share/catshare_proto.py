# -*- coding: utf-8 -*-
"""Prototipo del server CAT condiviso: protocollo rigctld di Hamlib.

Serve a stabilire sperimentalmente il formato esatto che il client Hamlib
(modello 2, NET rigctl) si aspetta da \\dump_state. Una volta che il client
apre la connessione senza errori, il formato si porta nel C++ di Decodium.

Stato simulato: un FT-991 su 14.074 MHz in DATA-U.
"""
import socket
import socketserver
import sys
import threading

STATE = {"freq": 14074000.0, "mode": "PKTUSB", "width": 3000,
         "ptt": 0, "split": 0, "tx_freq": 14074000.0, "tx_vfo": "VFOB"}

# Modello del rig annunciato ai client. 1 = Hamlib dummy: neutro e sempre
# accettato, mentre annunciare il modello reale obbligherebbe a dichiararne
# fedelmente tutte le capacita'.
RIG_MODEL = 1

DUMP_STATE = "\n".join([
    "1",                       # versione del protocollo
    str(RIG_MODEL),            # modello
    "2",                       # regione ITU
    # gamme in ricezione: min max modi low_power high_power vfo ant
    "30000.000000 56000000.000000 0x2ffffff -1 -1 0x3 0x3",
    "0 0 0 0 0 0 0",           # terminatore
    # gamme in trasmissione
    "1800000.000000 54000000.000000 0x2ffffff 5000 100000 0x3 0x3",
    "0 0 0 0 0 0 0",
    "0x2ffffff 1",             # passi di sintonia: modi passo
    "0 0",
    "0x82 500",                # filtri: modi larghezza
    "0x221 3000",
    "0 0",
    "0",                       # max_rit
    "0",                       # max_xit
    "0",                       # max_ifshift
    "0",                       # announces
    "0",                       # preamplificatori (terminatore)
    "0",                       # attenuatori (terminatore)
    "0x0",                     # has_get_func
    "0x0",                     # has_set_func
    "0x0",                     # has_get_level
    "0x0",                     # has_set_level
    "0x0",                     # has_get_parm
    "0x0",                     # has_set_parm
    "vfo_ops=0x0",
    "ptt_type=0x1",
    "targetable_vfo=0x0",
    "done",
]) + "\n"


class Handler(socketserver.StreamRequestHandler):
    def handle(self):
        peer = self.client_address
        print("[proto] connesso", peer, flush=True)
        while True:
            raw = self.rfile.readline()
            if not raw:
                break
            line = raw.decode("utf-8", "replace").strip()
            if not line:
                continue
            print("[proto] <- %r" % line, flush=True)
            out = self.dispatch(line)
            if out is None:
                break
            print("[proto] -> %r" % out[:120], flush=True)
            self.wfile.write(out.encode())
            self.wfile.flush()
        print("[proto] chiuso", peer, flush=True)

    def dispatch(self, line):
        parts = line.split()
        cmd = parts[0]

        if cmd in ("q", "Q"):
            return None
        if cmd == "\\dump_state":
            return DUMP_STATE
        if cmd == "\\chk_vfo":
            return "CHKVFO 0\n"
        if cmd == "\\set_lock_mode":
            return "RPRT 0\n"
        if cmd == "\\get_lock_mode":
            return "0\n"
        if cmd in ("f", "\\get_freq"):
            return "%.0f\n" % STATE["freq"]
        if cmd in ("F", "\\set_freq"):
            STATE["freq"] = float(parts[-1])
            return "RPRT 0\n"
        if cmd in ("m", "\\get_mode"):
            return "%s\n%d\n" % (STATE["mode"], STATE["width"])
        if cmd in ("M", "\\set_mode"):
            STATE["mode"] = parts[1]
            return "RPRT 0\n"
        if cmd in ("t", "\\get_ptt"):
            return "%d\n" % STATE["ptt"]
        if cmd in ("T", "\\set_ptt"):
            STATE["ptt"] = int(parts[-1])
            return "RPRT 0\n"
        if cmd in ("v", "\\get_vfo"):
            return "VFOA\n"
        if cmd in ("V", "\\set_vfo"):
            return "RPRT 0\n"
        if cmd in ("s", "\\get_split_vfo"):
            return "%d\n%s\n" % (STATE["split"], STATE["tx_vfo"])
        if cmd in ("S", "\\set_split_vfo"):
            STATE["split"] = int(parts[1])
            return "RPRT 0\n"
        if cmd in ("i", "\\get_split_freq"):
            return "%.0f\n" % STATE["tx_freq"]
        if cmd in ("I", "\\set_split_freq"):
            STATE["tx_freq"] = float(parts[-1])
            return "RPRT 0\n"
        if cmd == "\\get_powerstat":
            return "1\n"
        print("[proto] !! comando non gestito: %r" % line, flush=True)
        return "RPRT -11\n"


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 4532
    srv = Server(("127.0.0.1", port), Handler)
    print("[proto] in ascolto su 127.0.0.1:%d" % port, flush=True)
    srv.serve_forever()
