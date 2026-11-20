#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Costruisce la parte Hamlib della libreria radio di DecoPort.

Legge un albero sorgente di Hamlib (non lo compila, non esegue niente di suo) e
ne ricava, per ogni radio:

  - numero di modello, costruttore, nome, stato del backend;
  - tipo di porta e parametri seriali (velocita', bit, parita', handshake);
  - il dialetto CAT, dedotto dalle funzioni che il backend usa: e' cio' che
    permette a DecoPort di condividere la porta nativa di una radio fra piu'
    programmi senza capirne ogni comando;

e le tabelle con cui Hamlib stessa riconosce una radio dalla sua risposta:

  - "ID;" dei Kenwood/Elecraft (tre cifre) e degli Yaesu newcat (quattro);
  - l'indirizzo CI-V predefinito degli Icom.

    python tools/build_radio_library.py <albero-hamlib> data/radios-hamlib.json

Il risultato e' deterministico: lo stesso albero da' lo stesso file, byte per
byte, cosi' una differenza nel repository e' sempre una differenza di Hamlib.
"""

import json
import os
import re
import sys

# ── numeri di modello ────────────────────────────────────────────────────────

def read_model_numbers(riglist_h):
    """RIG_MODEL_<X> -> numero, risolvendo RIG_MAKE_MODEL e gli alias."""
    text = open(riglist_h, encoding="utf-8", errors="replace").read()
    per_backend = int(re.search(r"#define\s+MAX_MODELS_PER_BACKEND\s+(\d+)", text).group(1))
    backends = {m.group(1): int(m.group(2))
                for m in re.finditer(r"#define\s+(RIG_[A-Z0-9_]+)\s+(\d+)\s*$", text, re.M)}
    raw = {}
    for m in re.finditer(r"#define\s+RIG_MODEL_([A-Za-z0-9_]+)\s+(.+?)\s*(?:/[/*].*)?$", text, re.M):
        raw[m.group(1)] = m.group(2).strip()

    models = {}

    def resolve(name, depth=0):
        if name in models:
            return models[name]
        if depth > 8 or name not in raw:
            return None
        expr = raw[name]
        mk = re.fullmatch(r"RIG_MAKE_MODEL\s*\(\s*(RIG_[A-Z0-9_]+)\s*,\s*(\d+)\s*\)", expr)
        if mk and mk.group(1) in backends:
            value = backends[mk.group(1)] * per_backend + int(mk.group(2))
        elif re.fullmatch(r"\d+", expr):
            value = int(expr)
        elif re.fullmatch(r"RIG_MODEL_[A-Za-z0-9_]+", expr):
            value = resolve(expr[len("RIG_MODEL_"):], depth + 1)
        else:
            value = None
        models[name] = value
        return value

    for name in raw:
        resolve(name)
    return {k: v for k, v in models.items() if v is not None}


# ── blocchi rig_caps ─────────────────────────────────────────────────────────

CAPS_START = re.compile(r"struct\s+rig_caps\s+(\w+)\s*=\s*\{")


def caps_blocks(source):
    """Restituisce il testo di ogni 'struct rig_caps x = { ... }'."""
    for m in CAPS_START.finditer(source):
        depth = 0
        i = m.end() - 1
        while i < len(source):
            c = source[i]
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    yield source[m.end():i]
                    break
            elif c == '"':
                # salta le stringhe, che possono contenere graffe
                i += 1
                while i < len(source) and source[i] != '"':
                    i += 2 if source[i] == "\\" else 1
            i += 1


def field(block, name):
    m = re.search(r"\.\s*" + name + r"\s*=\s*([^,\n]+)", block)
    return m.group(1).strip() if m else None


def string_field(block, name):
    m = re.search(r"\.\s*" + name + r'\s*=\s*"([^"]*)"', block)
    return m.group(1) if m else None


def int_field(block, name):
    v = field(block, name)
    return int(v) if v is not None and re.fullmatch(r"\d+", v) else None


def strip_comments(source):
    source = re.sub(r"/\*.*?\*/", " ", source, flags=re.S)
    return re.sub(r"//[^\n]*", " ", source)


# Il dialetto non e' scritto da nessuna parte: lo dicono le funzioni che il
# backend usa. Quello che non si riconosce resta "binary", cioe' condiviso a
# turni di botta e risposta senza capirne la grammatica, che funziona con
# qualunque radio che parli solo quando interrogata.
def protocol_of(backend_dir, block):
    if backend_dir == "icom":
        return "civ"
    if backend_dir == "yaesu":
        return "newcat" if "newcat_" in block else "yaesu5"
    if backend_dir == "kenwood":
        # I portatili e i veicolari TH/TM chiudono i comandi con CR, non con ';'.
        if re.search(r"\bth_|\btm_|\bthd7|\bthd72|\bthd74|\btmd7", block):
            return "binary"
        return "kenwood"
    return "binary"


def parity_of(v):
    return {"RIG_PARITY_NONE": "N", "RIG_PARITY_EVEN": "E", "RIG_PARITY_ODD": "O",
            "RIG_PARITY_MARK": "M", "RIG_PARITY_SPACE": "S"}.get(v or "", None)


def handshake_of(v):
    return {"RIG_HANDSHAKE_NONE": "none", "RIG_HANDSHAKE_HARDWARE": "hardware",
            "RIG_HANDSHAKE_XONXOFF": "xonxoff"}.get(v or "", None)


def port_of(v):
    return {"RIG_PORT_SERIAL": "serial", "RIG_PORT_NETWORK": "network",
            "RIG_PORT_UDP_NETWORK": "network", "RIG_PORT_USB": "usb",
            "RIG_PORT_NONE": "none"}.get(v or "", "other")


def status_of(v):
    return (v or "").replace("RIG_STATUS_", "").capitalize() or None


def read_models(tree, numbers):
    rigs_dir = os.path.join(tree, "rigs")
    out = {}
    for backend in sorted(os.listdir(rigs_dir)):
        bdir = os.path.join(rigs_dir, backend)
        if not os.path.isdir(bdir):
            continue
        for fname in sorted(os.listdir(bdir)):
            if not fname.endswith(".c"):
                continue
            src = strip_comments(open(os.path.join(bdir, fname), encoding="utf-8",
                                      errors="replace").read())
            for block in caps_blocks(src):
                m = re.search(r"RIG_MODEL\s*\(\s*RIG_MODEL_([A-Za-z0-9_]+)\s*\)", block)
                if not m or m.group(1) not in numbers:
                    continue
                number = numbers[m.group(1)]
                entry = {
                    "model": number,
                    "symbol": m.group(1),
                    "mfg": string_field(block, "mfg_name"),
                    "name": string_field(block, "model_name"),
                    "status": status_of(field(block, "status")),
                    "backend": backend,
                    "protocol": protocol_of(backend, block),
                    "port": port_of(field(block, "port_type")),
                }
                if entry["port"] == "serial":
                    entry.update({
                        "baudMin": int_field(block, "serial_rate_min"),
                        "baudMax": int_field(block, "serial_rate_max"),
                        "dataBits": int_field(block, "serial_data_bits"),
                        "stopBits": int_field(block, "serial_stop_bits"),
                        "parity": parity_of(field(block, "serial_parity")),
                        "handshake": handshake_of(field(block, "serial_handshake")),
                    })
                entry = {k: v for k, v in entry.items() if v is not None}
                # Un modello definito due volte (succede con le varianti
                # dentro #if) tiene la prima definizione: e' deterministico.
                out.setdefault(number, entry)
    return [out[k] for k in sorted(out)]


# ── tabelle di riconoscimento ────────────────────────────────────────────────

def read_kenwood_ids(tree, numbers):
    src = strip_comments(open(os.path.join(tree, "rigs", "kenwood", "kenwood.c"),
                              encoding="utf-8", errors="replace").read())
    table = re.search(r"kenwood_id_string_list\s*\[\s*\]\s*=\s*\{(.*?)\n\s*\};", src, re.S).group(1)
    ids = {}
    for m in re.finditer(r"\{\s*RIG_MODEL_([A-Za-z0-9_]+)\s*,\s*\"([^\"]+)\"\s*\}", table):
        if m.group(1) in numbers and re.fullmatch(r"\d{3}", m.group(2)):
            ids.setdefault(m.group(2), []).append(numbers[m.group(1)])
    return [{"id": k, "models": v} for k, v in sorted(ids.items())]


def read_icom_addresses(tree, numbers):
    src = strip_comments(open(os.path.join(tree, "rigs", "icom", "icom.c"),
                              encoding="utf-8", errors="replace").read())
    table = re.search(r"icom_addr_list\s*\[\s*\]\s*=\s*\{(.*?)\n\s*\};", src, re.S).group(1)
    addrs = {}
    for m in re.finditer(r"\{\s*RIG_MODEL_([A-Za-z0-9_]+)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\}", table):
        if m.group(1) in numbers:
            addrs.setdefault(int(m.group(2), 0), []).append(numbers[m.group(1)])
    return [{"civ": "0x%02X" % k, "models": v} for k, v in sorted(addrs.items())]


def newcat_model_for(name, numbers):
    """NC_RIGID_FT991A -> RIG_MODEL_FT991: Hamlib nomina le varianti, non sempre i modelli."""
    candidates = [name]
    stripped = re.sub(r"(Contest|MP|DM|D|A)$", "", name)
    candidates.append(stripped)
    for c in list(candidates):
        if c.startswith("FTDX"):
            candidates.append("FT" + c[4:])
    for c in candidates:
        if c in numbers:
            return numbers[c]
    return None


def read_yaesu_ids(tree, numbers):
    src = strip_comments(open(os.path.join(tree, "rigs", "yaesu", "newcat.c"),
                              encoding="utf-8", errors="replace").read())
    ids = {}
    unresolved = []
    for m in re.finditer(r"NC_RIGID_([A-Za-z0-9_]+)\s*=\s*(\d+)", src):
        if m.group(1) == "NONE":
            continue
        model = newcat_model_for(m.group(1), numbers)
        if model is None:
            unresolved.append(m.group(1))
            continue
        # "ID0670;": quattro cifre, lo zero davanti fa parte della risposta.
        ids.setdefault("%04d" % int(m.group(2)), []).append(
            {"model": model, "variant": m.group(1)})
    for u in unresolved:
        print("newcat: nessun modello per NC_RIGID_%s" % u, file=sys.stderr)
    out = []
    for k, v in sorted(ids.items()):
        out.append({"id": k, "models": [e["model"] for e in v],
                    "variants": [e["variant"] for e in v]})
    return out


def hamlib_version(tree):
    try:
        text = open(os.path.join(tree, "configure.ac"), encoding="utf-8").read()
        m = re.search(r"AC_INIT\(\[Hamlib\],\s*\[([^\]]+)\]", text)
        if m:
            return m.group(1)
    except OSError:
        pass
    return os.path.basename(os.path.normpath(tree))


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    tree, out_path = sys.argv[1], sys.argv[2]
    numbers = read_model_numbers(os.path.join(tree, "include", "hamlib", "riglist.h"))
    models = read_models(tree, numbers)
    library = {
        "format": 1,
        "source": "Hamlib " + hamlib_version(tree),
        "models": models,
        "identify": {
            "kenwood": read_kenwood_ids(tree, numbers),
            "yaesu": read_yaesu_ids(tree, numbers),
            "civ": read_icom_addresses(tree, numbers),
        },
    }
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(library, f, ensure_ascii=False, indent=1, sort_keys=False)
        f.write("\n")
    protocols = {}
    for m in models:
        protocols[m["protocol"]] = protocols.get(m["protocol"], 0) + 1
    print("%s: %d modelli %s, %d ID Kenwood, %d ID Yaesu, %d indirizzi CI-V" % (
        library["source"], len(models), protocols,
        len(library["identify"]["kenwood"]), len(library["identify"]["yaesu"]),
        len(library["identify"]["civ"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
