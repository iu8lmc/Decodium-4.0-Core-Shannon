#!/usr/bin/env python3
"""
Decodium ASYMX — DXpedition Certificate Manager
Tool completo per gestire certificati DXpedition e lista verificata.

Funzionalita':
  - Database locale DXpedition (JSON)
  - Generazione certificati .dxcert (HMAC-SHA256)
  - Firma lista verificata (Ed25519)
  - Upload FTP su ft2.it
  - Generazione chiavi Ed25519
"""

import json
import hmac
import hashlib
import sys
import os
import base64
import subprocess
import tempfile
import ftplib
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from datetime import datetime, timezone, timedelta

# ─── Paths ───────────────────────────────────────────────────────────

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DB_FILE = os.path.join(SCRIPT_DIR, "dxped_database.json")
PRIVATE_KEY = os.path.join(SCRIPT_DIR, "dxped_private.pem")
PUBLIC_KEY = os.path.join(SCRIPT_DIR, "dxped_public.pem")
CONFIG_FILE = os.path.join(SCRIPT_DIR, "dxped_config.json")
HMAC_KEY = b"D3c0d1uM-ASYMX-DXp3d-2026-IU8LMC"


# ─── Database ────────────────────────────────────────────────────────

def load_db():
    if os.path.exists(DB_FILE):
        with open(DB_FILE) as f:
            return json.load(f)
    return {"dxpeditions": []}

def save_db(db):
    with open(DB_FILE, 'w') as f:
        json.dump(db, f, indent=2)

def find_dxped(db, callsign):
    for dx in db["dxpeditions"]:
        if dx["callsign"] == callsign.upper():
            return dx
    return None


# ─── Config ──────────────────────────────────────────────────────────

def load_config():
    if os.path.exists(CONFIG_FILE):
        with open(CONFIG_FILE) as f:
            return json.load(f)
    return {
        "ftp_host": "ft2.it",
        "ftp_user": "",
        "ftp_pass": "",
        "ftp_path": "/verified_dxpeds.json",
        "ftp_port": 21
    }

def save_config(cfg):
    with open(CONFIG_FILE, 'w') as f:
        json.dump(cfg, f, indent=2)


# ─── Ed25519 ─────────────────────────────────────────────────────────

def sign_ed25519(payload_bytes, private_key_path):
    with tempfile.NamedTemporaryFile(delete=False, suffix='.bin') as f:
        f.write(payload_bytes)
        payload_file = f.name
    sig_file = payload_file + '.sig'
    try:
        subprocess.run([
            'openssl', 'pkeyutl', '-sign',
            '-inkey', private_key_path,
            '-rawin',
            '-in', payload_file,
            '-out', sig_file
        ], check=True, capture_output=True)
        with open(sig_file, 'rb') as f:
            return f.read()
    finally:
        if os.path.exists(payload_file):
            os.unlink(payload_file)
        if os.path.exists(sig_file):
            os.unlink(sig_file)

def generate_keys():
    subprocess.run([
        'openssl', 'genpkey', '-algorithm', 'Ed25519',
        '-out', PRIVATE_KEY
    ], check=True, capture_output=True)
    subprocess.run([
        'openssl', 'pkey', '-in', PRIVATE_KEY,
        '-pubout', '-out', PUBLIC_KEY
    ], check=True, capture_output=True)
    result = subprocess.run([
        'openssl', 'pkey', '-in', PUBLIC_KEY, '-pubin', '-text', '-noout'
    ], capture_output=True, text=True, check=True)
    return result.stdout


# ─── Certificate Generation ─────────────────────────────────────────

def generate_cert(dx, output_path):
    obj = {
        "version": 1,
        "callsign": dx["callsign"],
        "dxcc_entity": dx["dxcc_entity"],
        "dxcc_name": dx["dxcc_name"],
        "operators": dx["operators"],
        "activation_start": dx["activation_start"],
        "activation_end": dx["activation_end"],
        "max_slots": dx["max_slots"],
        "issued_by": "Decodium Authority",
        "issued_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    }
    payload = json.dumps(obj, separators=(',', ':'), sort_keys=True).encode()
    sig = hmac.new(HMAC_KEY, payload, hashlib.sha256).hexdigest()
    obj["signature"] = sig
    with open(output_path, 'w') as f:
        json.dump(obj, f, indent=2)
    cert_hash = hashlib.sha256(payload).hexdigest()[:8]
    return cert_hash, sig


def generate_signed_list(db):
    now = datetime.now(timezone.utc)
    active = []
    for dx in db["dxpeditions"]:
        end_str = dx["activation_end"]
        try:
            end_dt = datetime.fromisoformat(end_str.replace('Z', '+00:00'))
            if end_dt >= now:
                active.append(dx["callsign"])
        except Exception:
            active.append(dx["callsign"])

    if not active:
        return None, "Nessuna DXpedition attiva"

    callsigns = sorted(set(active))
    obj = {
        "callsigns": callsigns,
        "expires": (now + timedelta(days=90)).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "updated": now.strftime("%Y-%m-%dT%H:%M:%SZ"),
        "version": 2
    }
    payload = json.dumps(obj, separators=(',', ':'), sort_keys=True).encode('utf-8')
    sig = sign_ed25519(payload, PRIVATE_KEY)
    sig_b64 = base64.b64encode(sig).decode('ascii')
    obj["signature"] = sig_b64

    output = os.path.join(SCRIPT_DIR, "verified_dxpeds.json")
    with open(output, 'w') as f:
        json.dump(obj, f, indent=2)

    return output, f"OK — {len(callsigns)} callsign: {', '.join(callsigns)}"


# ─── FTP Upload ──────────────────────────────────────────────────────

def ftp_upload(local_file, cfg):
    ftp = ftplib.FTP()
    ftp.connect(cfg["ftp_host"], cfg.get("ftp_port", 21), timeout=15)
    ftp.login(cfg["ftp_user"], cfg["ftp_pass"])
    remote_path = cfg.get("ftp_path", "/verified_dxpeds.json")
    remote_dir = os.path.dirname(remote_path)
    if remote_dir and remote_dir != '/':
        try:
            ftp.cwd(remote_dir)
        except ftplib.error_perm:
            pass
    remote_name = os.path.basename(remote_path)
    with open(local_file, 'rb') as f:
        ftp.storbinary(f'STOR {remote_name}', f)
    ftp.quit()


# ─── GUI ─────────────────────────────────────────────────────────────

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Decodium ASYMX — DXpedition Certificate Manager")
        self.geometry("920x700")
        self.resizable(True, True)
        self.configure(bg="#1e1e2e")

        self.db = load_db()
        self.config = load_config()

        style = ttk.Style()
        style.theme_use('clam')

        # Dark theme colors
        BG = "#1e1e2e"
        FG = "#cdd6f4"
        BG2 = "#313244"
        BG3 = "#45475a"
        ACCENT = "#f9e2af"
        GREEN = "#a6e3a1"
        RED = "#f38ba8"
        BLUE = "#89b4fa"

        style.configure(".", background=BG, foreground=FG, fieldbackground=BG2,
                         borderwidth=0, font=("Segoe UI", 10))
        style.configure("TFrame", background=BG)
        style.configure("TLabel", background=BG, foreground=FG, font=("Segoe UI", 10))
        style.configure("TLabelframe", background=BG, foreground=ACCENT, font=("Segoe UI", 11, "bold"))
        style.configure("TLabelframe.Label", background=BG, foreground=ACCENT)
        style.configure("TEntry", fieldbackground=BG2, foreground=FG, insertcolor=FG)
        style.configure("TButton", background=BG3, foreground=FG, font=("Segoe UI", 10, "bold"),
                         padding=(12, 6))
        style.map("TButton", background=[("active", BLUE)], foreground=[("active", "#1e1e2e")])
        style.configure("Accent.TButton", background=ACCENT, foreground="#1e1e2e")
        style.map("Accent.TButton", background=[("active", "#f5c211")])
        style.configure("Green.TButton", background=GREEN, foreground="#1e1e2e")
        style.map("Green.TButton", background=[("active", "#7dd87d")])
        style.configure("Red.TButton", background=RED, foreground="#1e1e2e")
        style.map("Red.TButton", background=[("active", "#e05577")])
        style.configure("Treeview", background=BG2, foreground=FG, fieldbackground=BG2,
                         rowheight=28, font=("Consolas", 10))
        style.configure("Treeview.Heading", background=BG3, foreground=ACCENT,
                         font=("Segoe UI", 10, "bold"))
        style.map("Treeview", background=[("selected", BLUE)], foreground=[("selected", "#1e1e2e")])

        self.BG = BG
        self.FG = FG
        self.ACCENT = ACCENT
        self.GREEN = GREEN
        self.RED = RED

        # ─── Title ───
        title_frame = ttk.Frame(self)
        title_frame.pack(fill=tk.X, padx=15, pady=(10, 5))
        ttk.Label(title_frame, text="DECODIUM ASYMX", font=("Segoe UI", 18, "bold"),
                  foreground=ACCENT).pack(side=tk.LEFT)
        ttk.Label(title_frame, text="  DXpedition Certificate Manager",
                  font=("Segoe UI", 12), foreground=FG).pack(side=tk.LEFT, padx=(5, 0))

        # ─── Notebook (tabs) ───
        nb = ttk.Notebook(self)
        nb.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        self.tab_dxped = ttk.Frame(nb)
        self.tab_ftp = ttk.Frame(nb)
        self.tab_keys = ttk.Frame(nb)
        nb.add(self.tab_dxped, text="  DXpedition  ")
        nb.add(self.tab_ftp, text="  FTP / Pubblica  ")
        nb.add(self.tab_keys, text="  Chiavi Ed25519  ")

        self.build_dxped_tab()
        self.build_ftp_tab()
        self.build_keys_tab()

        # ─── Status bar ───
        self.status_var = tk.StringVar(value="Pronto")
        status_bar = tk.Label(self, textvariable=self.status_var, bg=BG3, fg=FG,
                              anchor=tk.W, padx=10, pady=4, font=("Consolas", 9))
        status_bar.pack(fill=tk.X, side=tk.BOTTOM)

        self.refresh_tree()

    # ─── Tab: DXpedition ─────────────────────────────────────────────

    def build_dxped_tab(self):
        tab = self.tab_dxped

        # Form frame
        form = ttk.LabelFrame(tab, text="Nuova DXpedition / Modifica", padding=10)
        form.pack(fill=tk.X, padx=10, pady=(10, 5))

        fields = [
            ("Callsign:", "call_var", 12),
            ("DXCC Entity:", "dxcc_var", 6),
            ("DXCC Name:", "dxcc_name_var", 20),
            ("Operatori:", "ops_var", 25),
            ("Data Inizio:", "start_var", 12),
            ("Data Fine:", "end_var", 12),
            ("Max Slots:", "slots_var", 4),
        ]

        row = 0
        col = 0
        for label, var_name, width in fields:
            setattr(self, var_name, tk.StringVar())
            ttk.Label(form, text=label).grid(row=row, column=col, sticky=tk.W, padx=(0, 5), pady=3)
            e = ttk.Entry(form, textvariable=getattr(self, var_name), width=width)
            e.grid(row=row, column=col + 1, sticky=tk.W, padx=(0, 15), pady=3)
            col += 2
            if col >= 6:
                col = 0
                row += 1

        # Defaults
        self.start_var.set(datetime.now(timezone.utc).strftime("%Y-%m-%d"))
        self.end_var.set((datetime.now(timezone.utc) + timedelta(days=30)).strftime("%Y-%m-%d"))
        self.slots_var.set("2")

        # Buttons
        btn_frame = ttk.Frame(form)
        btn_frame.grid(row=row + 1, column=0, columnspan=8, pady=(10, 0))

        ttk.Button(btn_frame, text="Aggiungi / Aggiorna", style="Accent.TButton",
                   command=self.on_add).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Rimuovi Selezionata", style="Red.TButton",
                   command=self.on_remove).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Genera .dxcert", style="Green.TButton",
                   command=self.on_gen_cert).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Genera TUTTI i .dxcert",
                   command=self.on_gen_all_certs).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Pulisci Campi",
                   command=self.clear_form).pack(side=tk.LEFT, padx=5)

        # Tree
        tree_frame = ttk.Frame(tab)
        tree_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        cols = ("callsign", "dxcc", "dxcc_name", "operators", "start", "end", "slots")
        self.tree = ttk.Treeview(tree_frame, columns=cols, show="headings", selectmode="browse")

        headings = {
            "callsign": ("Callsign", 100),
            "dxcc": ("DXCC#", 60),
            "dxcc_name": ("DXCC Name", 160),
            "operators": ("Operatori", 180),
            "start": ("Inizio", 100),
            "end": ("Fine", 100),
            "slots": ("Slots", 50),
        }
        for c, (h, w) in headings.items():
            self.tree.heading(c, text=h)
            self.tree.column(c, width=w, minwidth=40)

        scroll = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL, command=self.tree.yview)
        self.tree.configure(yscrollcommand=scroll.set)
        self.tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scroll.pack(side=tk.RIGHT, fill=tk.Y)

        self.tree.bind("<<TreeviewSelect>>", self.on_tree_select)

    def refresh_tree(self):
        self.tree.delete(*self.tree.get_children())
        for dx in self.db["dxpeditions"]:
            self.tree.insert("", tk.END, values=(
                dx["callsign"],
                dx["dxcc_entity"],
                dx["dxcc_name"],
                ", ".join(dx["operators"]),
                dx["activation_start"][:10],
                dx["activation_end"][:10],
                dx["max_slots"]
            ))
        self.status_var.set(f"Database: {len(self.db['dxpeditions'])} DXpedition")

    def on_tree_select(self, event):
        sel = self.tree.selection()
        if not sel:
            return
        vals = self.tree.item(sel[0], "values")
        self.call_var.set(vals[0])
        self.dxcc_var.set(vals[1])
        self.dxcc_name_var.set(vals[2])
        self.ops_var.set(vals[3])
        self.start_var.set(vals[4])
        self.end_var.set(vals[5])
        self.slots_var.set(vals[6])

    def clear_form(self):
        self.call_var.set("")
        self.dxcc_var.set("")
        self.dxcc_name_var.set("")
        self.ops_var.set("")
        self.start_var.set(datetime.now(timezone.utc).strftime("%Y-%m-%d"))
        self.end_var.set((datetime.now(timezone.utc) + timedelta(days=30)).strftime("%Y-%m-%d"))
        self.slots_var.set("2")

    def on_add(self):
        call = self.call_var.get().strip().upper()
        if not call:
            messagebox.showwarning("Errore", "Inserisci un callsign")
            return
        try:
            dxcc = int(self.dxcc_var.get().strip())
        except ValueError:
            messagebox.showwarning("Errore", "DXCC Entity deve essere un numero")
            return
        dxcc_name = self.dxcc_name_var.get().strip()
        if not dxcc_name:
            messagebox.showwarning("Errore", "Inserisci il nome DXCC")
            return
        ops = [op.strip().upper() for op in self.ops_var.get().split(',') if op.strip()]
        if not ops:
            messagebox.showwarning("Errore", "Inserisci almeno un operatore")
            return

        start = self.start_var.get().strip()
        end = self.end_var.get().strip()
        if len(start) == 10:
            start += "T00:00:00Z"
        if len(end) == 10:
            end += "T23:59:59Z"

        try:
            slots = int(self.slots_var.get().strip())
        except ValueError:
            slots = 2

        existing = find_dxped(self.db, call)
        if existing:
            self.db["dxpeditions"].remove(existing)

        entry = {
            "callsign": call,
            "dxcc_entity": dxcc,
            "dxcc_name": dxcc_name,
            "operators": ops,
            "activation_start": start,
            "activation_end": end,
            "max_slots": slots,
            "added": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        }
        self.db["dxpeditions"].append(entry)
        save_db(self.db)
        self.refresh_tree()
        self.status_var.set(f"DXpedition {call} aggiunta/aggiornata")

    def on_remove(self):
        sel = self.tree.selection()
        if not sel:
            messagebox.showwarning("Errore", "Seleziona una DXpedition dalla lista")
            return
        call = self.tree.item(sel[0], "values")[0]
        if not messagebox.askyesno("Conferma", f"Rimuovere {call}?"):
            return
        dx = find_dxped(self.db, call)
        if dx:
            self.db["dxpeditions"].remove(dx)
            save_db(self.db)
            self.refresh_tree()
            self.clear_form()
            self.status_var.set(f"DXpedition {call} rimossa")

    def on_gen_cert(self):
        call = self.call_var.get().strip().upper()
        if not call:
            sel = self.tree.selection()
            if sel:
                call = self.tree.item(sel[0], "values")[0]
        if not call:
            messagebox.showwarning("Errore", "Seleziona o inserisci un callsign")
            return

        dx = find_dxped(self.db, call)
        if not dx:
            messagebox.showwarning("Errore", f"DXpedition {call} non trovata nel database.\nAggiungila prima.")
            return

        output = filedialog.asksaveasfilename(
            title=f"Salva certificato {call}",
            initialfile=f"{call}.dxcert",
            initialdir=SCRIPT_DIR,
            filetypes=[("DXped Certificate", "*.dxcert"), ("All", "*.*")]
        )
        if not output:
            return

        cert_hash, sig = generate_cert(dx, output)
        self.status_var.set(f"Certificato {call}.dxcert generato — Hash: {cert_hash}")
        messagebox.showinfo("Certificato Generato",
                            f"Certificato: {os.path.basename(output)}\n\n"
                            f"Callsign: {dx['callsign']}\n"
                            f"DXCC: {dx['dxcc_name']} ({dx['dxcc_entity']})\n"
                            f"Operatori: {', '.join(dx['operators'])}\n"
                            f"Periodo: {dx['activation_start'][:10]} - {dx['activation_end'][:10]}\n"
                            f"Max Slots: {dx['max_slots']}\n"
                            f"Hash: {cert_hash}\n\n"
                            f"Consegna questo file all'operatore DXpedition.")

    def on_gen_all_certs(self):
        if not self.db["dxpeditions"]:
            messagebox.showwarning("Errore", "Nessuna DXpedition nel database")
            return

        out_dir = filedialog.askdirectory(title="Cartella per i certificati", initialdir=SCRIPT_DIR)
        if not out_dir:
            return

        count = 0
        for dx in self.db["dxpeditions"]:
            path = os.path.join(out_dir, f"{dx['callsign']}.dxcert")
            generate_cert(dx, path)
            count += 1

        self.status_var.set(f"Generati {count} certificati in {out_dir}")
        messagebox.showinfo("Certificati Generati", f"{count} certificati .dxcert generati in:\n{out_dir}")

    # ─── Tab: FTP / Pubblica ─────────────────────────────────────────

    def build_ftp_tab(self):
        tab = self.tab_ftp

        # FTP settings
        ftp_frame = ttk.LabelFrame(tab, text="Impostazioni FTP", padding=10)
        ftp_frame.pack(fill=tk.X, padx=10, pady=(10, 5))

        self.ftp_host_var = tk.StringVar(value=self.config.get("ftp_host", "ft2.it"))
        self.ftp_port_var = tk.StringVar(value=str(self.config.get("ftp_port", 21)))
        self.ftp_user_var = tk.StringVar(value=self.config.get("ftp_user", ""))
        self.ftp_pass_var = tk.StringVar(value=self.config.get("ftp_pass", ""))
        self.ftp_path_var = tk.StringVar(value=self.config.get("ftp_path", "/verified_dxpeds.json"))

        ftp_fields = [
            ("Host:", self.ftp_host_var, 25, False),
            ("Porta:", self.ftp_port_var, 6, False),
            ("Utente:", self.ftp_user_var, 20, False),
            ("Password:", self.ftp_pass_var, 20, True),
            ("Path remoto:", self.ftp_path_var, 30, False),
        ]

        for i, (label, var, width, is_pass) in enumerate(ftp_fields):
            ttk.Label(ftp_frame, text=label).grid(row=i // 3, column=(i % 3) * 2, sticky=tk.W, padx=(0, 5), pady=3)
            e = ttk.Entry(ftp_frame, textvariable=var, width=width, show="*" if is_pass else "")
            e.grid(row=i // 3, column=(i % 3) * 2 + 1, sticky=tk.W, padx=(0, 15), pady=3)

        ttk.Button(ftp_frame, text="Salva Impostazioni FTP", command=self.save_ftp_config).grid(
            row=2, column=4, columnspan=2, pady=3, padx=5)

        # Actions
        act_frame = ttk.LabelFrame(tab, text="Azioni", padding=10)
        act_frame.pack(fill=tk.X, padx=10, pady=5)

        btn_row = ttk.Frame(act_frame)
        btn_row.pack(fill=tk.X)

        ttk.Button(btn_row, text="1. Firma Lista Verificata (Ed25519)",
                   style="Accent.TButton", command=self.on_sign_list).pack(side=tk.LEFT, padx=5, pady=5)
        ttk.Button(btn_row, text="2. Upload FTP su ft2.it",
                   style="Green.TButton", command=self.on_ftp_upload).pack(side=tk.LEFT, padx=5, pady=5)
        ttk.Button(btn_row, text="Firma + Upload (tutto)",
                   style="Green.TButton", command=self.on_sign_and_upload).pack(side=tk.LEFT, padx=5, pady=5)
        ttk.Button(btn_row, text="Test Connessione FTP",
                   command=self.on_test_ftp).pack(side=tk.LEFT, padx=5, pady=5)

        # Log
        log_frame = ttk.LabelFrame(tab, text="Log", padding=5)
        log_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        self.log_text = tk.Text(log_frame, bg="#181825", fg="#cdd6f4", font=("Consolas", 10),
                                wrap=tk.WORD, height=15, insertbackground="#cdd6f4",
                                selectbackground="#89b4fa", selectforeground="#1e1e2e")
        log_scroll = ttk.Scrollbar(log_frame, orient=tk.VERTICAL, command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=log_scroll.set)
        self.log_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        log_scroll.pack(side=tk.RIGHT, fill=tk.Y)

    def log(self, msg):
        ts = datetime.now().strftime("%H:%M:%S")
        self.log_text.insert(tk.END, f"[{ts}] {msg}\n")
        self.log_text.see(tk.END)
        self.update_idletasks()

    def save_ftp_config(self):
        self.config["ftp_host"] = self.ftp_host_var.get().strip()
        self.config["ftp_port"] = int(self.ftp_port_var.get().strip() or "21")
        self.config["ftp_user"] = self.ftp_user_var.get().strip()
        self.config["ftp_pass"] = self.ftp_pass_var.get().strip()
        self.config["ftp_path"] = self.ftp_path_var.get().strip()
        save_config(self.config)
        self.status_var.set("Impostazioni FTP salvate")
        self.log("Impostazioni FTP salvate in dxped_config.json")

    def on_sign_list(self):
        if not os.path.exists(PRIVATE_KEY):
            messagebox.showerror("Errore", "Chiave privata Ed25519 non trovata.\n"
                                 "Vai nel tab 'Chiavi Ed25519' e generane una.")
            return

        if not self.db["dxpeditions"]:
            messagebox.showwarning("Errore", "Nessuna DXpedition nel database")
            return

        self.log("Firma lista verificata in corso...")
        try:
            output, msg = generate_signed_list(self.db)
            if output is None:
                self.log(f"ERRORE: {msg}")
                messagebox.showwarning("Errore", msg)
                return
            self.log(f"Lista firmata: {output}")
            self.log(f"  {msg}")
            self.status_var.set(f"Lista firmata generata: {msg}")
        except Exception as e:
            self.log(f"ERRORE firma: {e}")
            messagebox.showerror("Errore", str(e))

    def on_ftp_upload(self):
        json_path = os.path.join(SCRIPT_DIR, "verified_dxpeds.json")
        if not os.path.exists(json_path):
            messagebox.showwarning("Errore", "verified_dxpeds.json non trovato.\n"
                                   "Prima firma la lista.")
            return

        self.save_ftp_config()
        host = self.config["ftp_host"]
        user = self.config["ftp_user"]
        if not user:
            messagebox.showwarning("Errore", "Inserisci utente FTP")
            return

        self.log(f"Connessione FTP a {host}...")
        try:
            ftp_upload(json_path, self.config)
            self.log(f"Upload completato su {host}{self.config['ftp_path']}")
            self.status_var.set(f"Upload FTP completato su {host}")
            messagebox.showinfo("Upload Completato",
                                f"verified_dxpeds.json caricato su\n"
                                f"{host}{self.config['ftp_path']}")
        except Exception as e:
            self.log(f"ERRORE FTP: {e}")
            messagebox.showerror("Errore FTP", str(e))

    def on_sign_and_upload(self):
        self.on_sign_list()
        json_path = os.path.join(SCRIPT_DIR, "verified_dxpeds.json")
        if os.path.exists(json_path):
            self.on_ftp_upload()

    def on_test_ftp(self):
        self.save_ftp_config()
        host = self.config["ftp_host"]
        self.log(f"Test connessione FTP a {host}...")
        try:
            ftp = ftplib.FTP()
            ftp.connect(host, self.config.get("ftp_port", 21), timeout=10)
            welcome = ftp.getwelcome()
            self.log(f"  Connesso: {welcome}")
            ftp.login(self.config["ftp_user"], self.config["ftp_pass"])
            self.log(f"  Login OK come {self.config['ftp_user']}")
            pwd = ftp.pwd()
            self.log(f"  Directory corrente: {pwd}")
            files = ftp.nlst()
            self.log(f"  File nella root: {', '.join(files[:10])}{'...' if len(files) > 10 else ''}")
            ftp.quit()
            self.log("  Connessione FTP OK!")
            self.status_var.set("Test FTP: OK")
        except Exception as e:
            self.log(f"  ERRORE: {e}")
            messagebox.showerror("Errore FTP", str(e))

    # ─── Tab: Chiavi Ed25519 ─────────────────────────────────────────

    def build_keys_tab(self):
        tab = self.tab_keys

        info_frame = ttk.LabelFrame(tab, text="Stato Chiavi Ed25519", padding=10)
        info_frame.pack(fill=tk.X, padx=10, pady=(10, 5))

        self.key_status_var = tk.StringVar()
        self.update_key_status()

        ttk.Label(info_frame, textvariable=self.key_status_var,
                  font=("Consolas", 10), wraplength=800).pack(anchor=tk.W)

        btn_frame = ttk.Frame(info_frame)
        btn_frame.pack(fill=tk.X, pady=(10, 0))

        ttk.Button(btn_frame, text="Genera Nuove Chiavi", style="Red.TButton",
                   command=self.on_keygen).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Mostra Chiave Pubblica",
                   command=self.on_show_pubkey).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Copia Hex C++ negli Appunti",
                   command=self.on_copy_hex).pack(side=tk.LEFT, padx=5)

        # Info box
        help_frame = ttk.LabelFrame(tab, text="Informazioni", padding=10)
        help_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        help_text = tk.Text(help_frame, bg="#181825", fg="#cdd6f4", font=("Consolas", 10),
                            wrap=tk.WORD, height=15)
        help_text.insert("1.0", """
SISTEMA DI VERIFICA DXPEDITION
==============================

Chiave Privata (dxped_private.pem)
  - Usata per FIRMARE la lista verified_dxpeds.json
  - DEVE restare segreta — solo tu la possiedi
  - NON committarla su GitHub
  - Se la perdi, devi generarne una nuova e aggiornare
    la chiave pubblica nel codice C++ di Decodium

Chiave Pubblica (dxped_public.pem)
  - Embedded nel binario di Decodium (VerifiedDxpedList.hpp)
  - Usata per VERIFICARE la firma della lista
  - Tutti gli utenti Decodium la hanno nel binario
  - Se generi nuove chiavi, DEVI aggiornare il file
    VerifiedDxpedList.hpp e ricompilare

Flusso:
  1. Aggiungi DXpedition nel tab "DXpedition"
  2. Genera il .dxcert per l'operatore (gli dai il file)
  3. Vai nel tab "FTP / Pubblica"
  4. Clicca "Firma + Upload" per firmare e pubblicare
  5. Tutti gli utenti Decodium vedranno VERIFIED

Sicurezza:
  - Ed25519 (firma asimmetrica) — nessuno puo' falsificare
    la lista senza la chiave privata
  - I certificati .dxcert usano HMAC-SHA256 (la chiave e'
    nel codice sorgente, ma servono solo per l'operatore DXped)
""")
        help_text.configure(state=tk.DISABLED)
        help_text.pack(fill=tk.BOTH, expand=True)

    def update_key_status(self):
        priv_ok = os.path.exists(PRIVATE_KEY)
        pub_ok = os.path.exists(PUBLIC_KEY)
        status = f"Chiave Privata: {'PRESENTE' if priv_ok else 'NON TROVATA'} ({PRIVATE_KEY})\n"
        status += f"Chiave Pubblica: {'PRESENTE' if pub_ok else 'NON TROVATA'} ({PUBLIC_KEY})"
        self.key_status_var.set(status)

    def on_keygen(self):
        if os.path.exists(PRIVATE_KEY):
            if not messagebox.askyesno("Attenzione",
                                       "Chiave privata gia' esistente!\n\n"
                                       "Se la sovrascivi, dovrai:\n"
                                       "1. Aggiornare la chiave pubblica in VerifiedDxpedList.hpp\n"
                                       "2. Ricompilare Decodium\n"
                                       "3. Rilasciare un nuovo aggiornamento\n\n"
                                       "Continuare?"):
                return

        try:
            pubkey_info = generate_keys()
            self.update_key_status()
            self.status_var.set("Chiavi Ed25519 generate")
            messagebox.showinfo("Chiavi Generate",
                                f"Chiavi Ed25519 generate con successo!\n\n"
                                f"{pubkey_info}\n"
                                f"IMPORTANTE: Aggiorna la chiave pubblica\n"
                                f"in VerifiedDxpedList.hpp e ricompila!")
        except Exception as e:
            messagebox.showerror("Errore", f"Generazione chiavi fallita:\n{e}")

    def on_show_pubkey(self):
        if not os.path.exists(PUBLIC_KEY):
            messagebox.showwarning("Errore", "Chiave pubblica non trovata")
            return
        try:
            result = subprocess.run([
                'openssl', 'pkey', '-in', PUBLIC_KEY, '-pubin', '-text', '-noout'
            ], capture_output=True, text=True, check=True)

            win = tk.Toplevel(self)
            win.title("Chiave Pubblica Ed25519")
            win.geometry("500x200")
            win.configure(bg=self.BG)
            txt = tk.Text(win, bg="#181825", fg="#cdd6f4", font=("Consolas", 11))
            txt.insert("1.0", result.stdout)
            txt.configure(state=tk.DISABLED)
            txt.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        except Exception as e:
            messagebox.showerror("Errore", str(e))

    def on_copy_hex(self):
        if not os.path.exists(PUBLIC_KEY):
            messagebox.showwarning("Errore", "Chiave pubblica non trovata")
            return
        try:
            result = subprocess.run([
                'openssl', 'pkey', '-in', PUBLIC_KEY, '-pubin', '-text', '-noout'
            ], capture_output=True, text=True, check=True)

            # Parse hex bytes from output
            lines = result.stdout.strip().split('\n')
            hex_bytes = []
            for line in lines:
                line = line.strip()
                if ':' in line and not line.startswith('ED25519'):
                    parts = line.split(':')
                    for p in parts:
                        p = p.strip()
                        if len(p) == 2:
                            try:
                                int(p, 16)
                                hex_bytes.append(f"0x{p}")
                            except ValueError:
                                pass

            cpp_code = (
                "  static constexpr unsigned char ed25519_pubkey[32] = {\n"
                f"    {', '.join(hex_bytes[:8])},\n"
                f"    {', '.join(hex_bytes[8:16])},\n"
                f"    {', '.join(hex_bytes[16:24])},\n"
                f"    {', '.join(hex_bytes[24:32])}\n"
                "  };"
            )

            self.clipboard_clear()
            self.clipboard_append(cpp_code)
            self.status_var.set("Chiave pubblica C++ copiata negli appunti")
            messagebox.showinfo("Copiato", f"Codice C++ copiato negli appunti:\n\n{cpp_code}")
        except Exception as e:
            messagebox.showerror("Errore", str(e))


# ─── Main ────────────────────────────────────────────────────────────

if __name__ == "__main__":
    app = App()
    app.mainloop()
