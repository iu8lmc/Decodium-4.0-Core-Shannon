#!/usr/bin/env python3
"""Ricevitore webhook per la GitHub App decodium-agent.

Riceve le consegne di GitHub, ne verifica la firma, si autentica come app e
decide se intervenire. Con l'analisi configurata esamina il codice e puo'
proporre una modifica (vedi decodium_analysis.py); senza, si limita a
confermare la ricezione invece di improvvisare.

Il modello si raggiunge per due strade alternative:
  - abbonamento: CLAUDE_CODE_OAUTH_TOKEN, tramite la CLI di Claude Code;
  - a consumo:   DECOAGENT_ANTHROPIC_KEY, tramite l'API Messages.
Se ci sono entrambe vince l'abbonamento.

Configurazione tramite ambiente (vedi decodium-agent.service):
  DECOAGENT_APP_ID          identificativo numerico dell'app        (obbligatorio)
  DECOAGENT_PRIVATE_KEY     percorso del file .pem                  (obbligatorio)
  DECOAGENT_WEBHOOK_SECRET  segreto del webhook                     (obbligatorio)
  DECOAGENT_PORT            porta locale, default 8787
  DECOAGENT_BIND            indirizzo di ascolto, default 127.0.0.1
  DECOAGENT_REPOS           elenco separato da virgole di owner/repo ammessi
  DECOAGENT_LABEL           etichetta che abilita l'intervento
                            (default decodium-eligible)
  DECOAGENT_DRY_RUN         1 = non scrive nulla su GitHub, registra soltanto
  CLAUDE_CODE_OAUTH_TOKEN   token durevole da abbonamento (claude setup-token)
  DECOAGENT_CLAUDE_BIN      percorso della CLI, default "claude"
  DECOAGENT_ANTHROPIC_KEY   in alternativa, chiave API a consumo
  DECOAGENT_MODEL           modello, default claude-opus-5
  DECOAGENT_REPO_DIR        copia di lavoro del repository
"""

import base64
import hashlib
import hmac
import json
import logging
import os
import sys
import threading
import time
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

try:
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import padding
except ImportError:  # pragma: no cover
    sys.stderr.write("serve il pacchetto python3-cryptography\n")
    raise

API = "https://api.github.com"
UA = "decodium-agent"

log = logging.getLogger("decodium-agent")


# ----------------------------------------------------------------- configurazione
class Config:
    def __init__(self):
        self.app_id = os.environ.get("DECOAGENT_APP_ID", "").strip()
        self.key_path = os.environ.get("DECOAGENT_PRIVATE_KEY", "").strip()
        self.secret = os.environ.get("DECOAGENT_WEBHOOK_SECRET", "").encode()
        self.port = int(os.environ.get("DECOAGENT_PORT", "8787"))
        self.bind = os.environ.get("DECOAGENT_BIND", "127.0.0.1")
        self.label = os.environ.get("DECOAGENT_LABEL", "decodium-eligible").strip()
        self.dry_run = os.environ.get("DECOAGENT_DRY_RUN", "") in ("1", "true", "yes")
        repos = os.environ.get("DECOAGENT_REPOS", "").strip()
        self.repos = {r.strip().lower() for r in repos.split(",") if r.strip()}
        # Fase 2: senza chiave l'agente resta al solo riscontro, invece di
        # improvvisare un'analisi che non e' in grado di fare.
        self.api_key = os.environ.get("DECOAGENT_ANTHROPIC_KEY", "").strip()
        # Percorso ad abbonamento: token durevole di Claude Code, alternativo
        # alla chiave API. Se ci sono entrambi vince l'abbonamento.
        self.oauth = os.environ.get("CLAUDE_CODE_OAUTH_TOKEN", "").strip()
        self.claude_bin = os.environ.get("DECOAGENT_CLAUDE_BIN", "claude").strip()
        self.model = os.environ.get("DECOAGENT_MODEL", "claude-opus-5").strip()
        self.repo_dir = os.environ.get("DECOAGENT_REPO_DIR",
                                       "/var/lib/decodium-agent/repo").strip()

    def problems(self):
        missing = []
        if not self.app_id:
            missing.append("DECOAGENT_APP_ID")
        if not self.key_path:
            missing.append("DECOAGENT_PRIVATE_KEY")
        elif not os.path.exists(self.key_path):
            missing.append("DECOAGENT_PRIVATE_KEY (file inesistente: %s)" % self.key_path)
        if not self.secret:
            missing.append("DECOAGENT_WEBHOOK_SECRET")
        return missing


# ------------------------------------------------------------------ autenticazione
def _b64(raw: bytes) -> bytes:
    return base64.urlsafe_b64encode(raw).rstrip(b"=")


def app_jwt(cfg: Config) -> str:
    """JWT RS256 firmato con la chiave privata dell'app.

    Vale dieci minuti; l'orologio viene arretrato di un minuto perche' GitHub
    rifiuta un iat nel futuro e i VPS derivano facilmente di qualche secondo.
    """
    with open(cfg.key_path, "rb") as fh:
        key = serialization.load_pem_private_key(fh.read(), password=None)
    now = int(time.time())
    header = {"alg": "RS256", "typ": "JWT"}
    payload = {"iat": now - 60, "exp": now + 540, "iss": cfg.app_id}
    signing_input = b".".join((
        _b64(json.dumps(header, separators=(",", ":")).encode()),
        _b64(json.dumps(payload, separators=(",", ":")).encode()),
    ))
    signature = key.sign(signing_input, padding.PKCS1v15(), hashes.SHA256())
    return (signing_input + b"." + _b64(signature)).decode()


def api(method, path, token, body=None, accept="application/vnd.github+json"):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(API + path, data=data, method=method)
    req.add_header("Authorization", "Bearer " + token)
    req.add_header("Accept", accept)
    req.add_header("X-GitHub-Api-Version", "2022-11-28")
    req.add_header("User-Agent", UA)
    if data:
        req.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(req, timeout=25) as resp:
        raw = resp.read()
        return resp.status, (json.loads(raw) if raw else None)


_token_cache = {}
_token_lock = threading.Lock()


def installation_token(cfg: Config, installation_id: int) -> str:
    """Token di installazione, riusato finche' manca piu' di un minuto alla scadenza."""
    with _token_lock:
        hit = _token_cache.get(installation_id)
        if hit and hit[1] - time.time() > 60:
            return hit[0]
    status, body = api("POST",
                       "/app/installations/%d/access_tokens" % installation_id,
                       app_jwt(cfg))
    token = body["token"]
    expires = time.mktime(time.strptime(body["expires_at"], "%Y-%m-%dT%H:%M:%SZ"))
    with _token_lock:
        _token_cache[installation_id] = (token, expires)
    return token


# ------------------------------------------------------------------------ azioni
def comment(cfg, installation_id, repo_full, issue_number, text):
    if cfg.dry_run:
        log.info("[prova a vuoto] avrei commentato su %s#%s:\n%s",
                 repo_full, issue_number, text)
        return
    token = installation_token(cfg, installation_id)
    api("POST", "/repos/%s/issues/%s/comments" % (repo_full, issue_number),
        token, {"body": text})
    log.info("commento pubblicato su %s#%s", repo_full, issue_number)


ACK = """**decodium-agent** ha preso in carico questa segnalazione.

| | |
|---|---|
| Evento | `{event}` / `{action}` |
| Etichetta abilitante | `{label}` |
| Ricevuto | {when} UTC |

In questa fase l'agente si limita a confermare la ricezione: non analizza il
codice e non apre pull request. Quando l'analisi sara' attiva, la risposta
conterra' i file coinvolti e una valutazione del rischio, e ogni proposta di
modifica passera' comunque da una revisione umana.
"""


def handle_event(cfg, event, payload):
    """Decide se e come rispondere. Non solleva: gli errori vanno nel registro."""
    action = payload.get("action", "")
    repo = (payload.get("repository") or {}).get("full_name", "")
    installation = (payload.get("installation") or {}).get("id")

    if not repo or installation is None:
        log.info("ignorato: evento senza repository o installazione (%s/%s)", event, action)
        return
    if cfg.repos and repo.lower() not in cfg.repos:
        log.info("ignorato: %s non e' fra i repository ammessi", repo)
        return
    if event != "issues" or action not in ("opened", "labeled", "reopened"):
        log.info("ignorato: %s/%s non richiede intervento", event, action)
        return

    issue = payload.get("issue") or {}
    labels = {(l or {}).get("name", "") for l in issue.get("labels") or []}
    if cfg.label and cfg.label not in labels:
        log.info("ignorato: %s#%s non ha l'etichetta %s",
                 repo, issue.get("number"), cfg.label)
        return

    number = issue.get("number")
    if not (cfg.api_key or cfg.oauth):
        comment(cfg, installation, repo, number,
                ACK.format(event=event, action=action, label=cfg.label,
                           when=time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())))
        return

    analyse(cfg, installation, repo, issue)


REPORT = """**decodium-agent** ha esaminato questa segnalazione.

{analysis}

| | |
|---|---|
| Confidenza | {confidence} |
| Rischio | {risk} |
| File esaminati | {files} |

{outcome}

---
<sub>Analisi automatica sul codice alla revisione `{sha}`. Va letta come una
traccia da verificare, non come una diagnosi certa: nessuna modifica viene
fusa senza revisione umana.</sub>
"""


def analyse(cfg, installation_id, repo_full, issue):
    """Analizza la segnalazione e, se la modifica regge, apre una pull request."""
    import decodium_analysis as an

    number = issue.get("number")
    token = installation_token(cfg, installation_id)
    sha = an.ensure_clone(cfg.repo_dir, repo_full, token)
    log.info("repository alla revisione %s", sha[:12])

    text = (issue.get("title") or "") + "\n" + (issue.get("body") or "")
    files, idents = an.candidate_files(cfg.repo_dir, text)
    log.info("file candidati: %s", ", ".join(files) or "nessuno")
    if not files:
        comment(cfg, installation_id, repo_full, number,
                "**decodium-agent**: dal testo della segnalazione non emergono "
                "riferimenti a file o simboli presenti nel repository, quindi "
                "non ho un punto da cui partire. Aggiungere il nome di una "
                "funzione, di un file o un estratto del registro diagnostico "
                "renderebbe l'analisi possibile.")
        return

    budget = an.MAX_EXCERPT_BYTES // max(1, len(files))
    excerpts = "".join(
        "\n=== %s ===\n%s\n" % (f, an.excerpt(cfg.repo_dir, f, idents, budget))
        for f in files)

    prompt = an.build_prompt(issue.get("title") or "", issue.get("body") or "",
                             excerpts)
    if cfg.oauth:
        log.info("analisi via abbonamento (Claude Code)")
        result = an.ask_via_cli(cfg.claude_bin, cfg.oauth, cfg.model, prompt)
    else:
        log.info("analisi via API a consumo")
        result = an.ask_via_api(cfg.api_key, cfg.model, prompt)
    patch = result.get("patch") or ""
    ok, why, touched = an.validate_patch(cfg.repo_dir, patch)
    log.info("proposta: %s (%s)", "accettata" if ok else "scartata", why)

    if not ok:
        outcome = ("Non allego una modifica: %s. L'analisi qui sopra resta "
                   "valida come traccia." % why)
    elif cfg.dry_run:
        outcome = ("[prova a vuoto] avrei aperto una pull request su %s (%s)."
                   % (", ".join("`%s`" % t for t in touched), why))
    else:
        branch = "agent/issue-%s" % number
        pr = an.open_pull_request(
            cfg.repo_dir, api, token, repo_full, branch,
            "Proposta per la segnalazione #%s" % number,
            "Chiude parzialmente #%s\n\n%s\n\n*Bozza aperta automaticamente da "
            "decodium-agent: richiede revisione umana.*"
            % (number, result.get("analysis", "")),
            patch)
        outcome = "Proposta in bozza: #%s (%s)." % (pr["number"], why)

    body = REPORT.format(
        analysis=result.get("analysis", "").strip(),
        confidence=result.get("confidence", "?"),
        risk=result.get("risk", "?"),
        files=", ".join("`%s`" % f for f in files),
        outcome=outcome, sha=sha[:12])
    comment(cfg, installation_id, repo_full, number, body)


# ------------------------------------------------------------------------- server
class Handler(BaseHTTPRequestHandler):
    server_version = UA
    cfg = None

    def log_message(self, fmt, *args):  # silenzia il registro di default
        log.debug("%s - %s", self.address_string(), fmt % args)

    def _reply(self, code, text=b""):
        self.send_response(code)
        self.send_header("Content-Length", str(len(text)))
        self.end_headers()
        if text:
            self.wfile.write(text)

    def do_GET(self):
        # sonda di salute, per il monitoraggio e per accertare che nginx instradi
        if self.path.rstrip("/").endswith("/health"):
            self._reply(200, b"decodium-agent ok\n")
        else:
            self._reply(404)

    def do_POST(self):
        length = int(self.headers.get("Content-Length") or 0)
        if length <= 0 or length > 8 * 1024 * 1024:
            self._reply(400, b"lunghezza non valida\n")
            return
        raw = self.rfile.read(length)

        signature = self.headers.get("X-Hub-Signature-256", "")
        expected = "sha256=" + hmac.new(self.cfg.secret, raw, hashlib.sha256).hexdigest()
        if not hmac.compare_digest(signature, expected):
            # Una firma che non torna non e' un errore di rete: e' qualcuno che
            # non possiede il segreto. Si rifiuta e si annota.
            log.warning("firma non valida da %s", self.address_string())
            self._reply(401, b"firma non valida\n")
            return

        event = self.headers.get("X-GitHub-Event", "")
        delivery = self.headers.get("X-GitHub-Delivery", "")
        try:
            payload = json.loads(raw)
        except ValueError:
            self._reply(400, b"corpo non JSON\n")
            return

        # GitHub considera fallita una consegna che non risponde entro dieci
        # secondi: si accetta subito e si lavora a parte.
        self._reply(204)
        log.info("consegna %s evento=%s", delivery, event)
        try:
            handle_event(self.cfg, event, payload)
        except urllib.error.HTTPError as exc:
            log.error("GitHub ha risposto %s: %s", exc.code, exc.read()[:400])
        except Exception:
            log.exception("errore nel trattamento di %s", delivery)


def main():
    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s")
    cfg = Config()
    problems = cfg.problems()
    if problems:
        log.error("configurazione incompleta: %s", ", ".join(problems))
        return 2

    Handler.cfg = cfg
    srv = ThreadingHTTPServer((cfg.bind, cfg.port), Handler)
    log.info("in ascolto su %s:%d - app=%s etichetta=%s repository=%s%s",
             cfg.bind, cfg.port, cfg.app_id, cfg.label or "(qualsiasi)",
             ", ".join(sorted(cfg.repos)) or "(qualsiasi)",
             " [prova a vuoto]" if cfg.dry_run else "")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
