# -*- coding: utf-8 -*-
"""Analisi di una segnalazione e proposta di modifica.

Fase 2. Il percorso e' sempre lo stesso e non ha scorciatoie:

  1. si aggiorna una copia del repository sul disco del server;
  2. dal testo della segnalazione si estraggono gli identificatori che
     compaiono davvero nel codice, e si classificano i file per pertinenza;
  3. si chiede al modello una modifica in forma di diff unificato, dandogli
     solo gli estratti dei file individuati;
  4. il diff viene VERIFICATO prima di esistere come ramo: percorsi ammessi,
     dimensione, e applicabilita' reale con "git apply --check";
  5. solo allora si crea il ramo, si spinge e si apre la pull request.

Cosa non fa, per costruzione: non fonde mai nulla (non esiste una chiamata al
merge in questo file), non tocca i file elencati in DENY, e se il diff non
supera la verifica non apre alcuna pull request - si limita a commentare
l'analisi, che resta utile anche senza la proposta.
"""

import json
import os
import re
import subprocess
import urllib.request

# --------------------------------------------------------------------- limiti
# Percorsi che l'agente non puo' modificare in nessun caso. I workflow sono
# gia' bloccati da GitHub (il permesso non e' concesso), ma ripeterlo qui rende
# il rifiuto esplicito e verificabile senza dover interrogare l'API.
DENY = (
    ".github/",
    "packaging/",
    "tools/github-app/",
    "CMake/getsvn.cmake",
)
DENY_SUFFIX = (".pem", ".key", ".p12", ".pfx", ".exe", ".dll", ".qm")

MAX_FILES = 5
MAX_CHANGED_LINES = 400
MAX_EXCERPT_BYTES = 12000
MAX_CANDIDATES = 8

API_URL = "https://api.anthropic.com/v1/messages"


# ------------------------------------------------------------------ repository
def git(repo, *args, check=True):
    p = subprocess.run(("git",) + args, cwd=repo, capture_output=True, text=True)
    if check and p.returncode != 0:
        raise RuntimeError("git %s: %s" % (" ".join(args), p.stderr.strip()[:300]))
    return p.stdout


def ensure_clone(repo_dir, full_name, token):
    """Clone superficiale, riportato alla punta di main a ogni giro."""
    url = "https://x-access-token:%s@github.com/%s.git" % (token, full_name)
    if not os.path.isdir(os.path.join(repo_dir, ".git")):
        os.makedirs(os.path.dirname(repo_dir), exist_ok=True)
        subprocess.run(("git", "clone", "--depth", "50", url, repo_dir),
                       capture_output=True, text=True, check=True)
    else:
        git(repo_dir, "remote", "set-url", "origin", url)
        git(repo_dir, "fetch", "--depth", "50", "origin", "main")
        git(repo_dir, "reset", "--hard", "origin/main")
        git(repo_dir, "clean", "-fd")
    return git(repo_dir, "rev-parse", "HEAD").strip()


# ------------------------------------------------------------- individuazione
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]{4,}")
PATHISH = re.compile(r"[\w./-]+\.(?:cpp|hpp|h|qml|py|ts|cmake|txt|md|f90)")

# Un identificatore serve solo se ha forma di codice: CamelCase, snake_case o
# maiuscole. Le parole della lingua comune ("potenza", "mostra") compaiono nei
# cataloghi di traduzione e nelle note di rilascio, e trascinerebbero fra i
# candidati file che non c'entrano nulla.
CODEISH = re.compile(r"[a-z][A-Z]|_|^[A-Z]{3,}$")

# File che quasi mai sono il luogo del difetto: restano ammissibili, ma pesano
# meno, cosi' non scalzano il codice vero dai primi posti.
WEAK_PREFIX = ("doc/", "translations/", "_relnotes")
WEAK_SUFFIX = (".md", ".ts", ".txt")

STOP = {
    "decodium", "github", "issue", "problem", "problema", "errore", "error",
    "quando", "invece", "questo", "questa", "which", "there", "should",
    "windows", "linux", "macos", "version", "versione", "release",
}


def candidate_files(repo_dir, text):
    """File del repository piu' pertinenti al testo della segnalazione.

    Prima i percorsi citati esplicitamente, poi i file che contengono gli
    identificatori estratti. Nessuna magia: e' una ricerca, e come tale va
    letta - indica dove guardare, non dove sta il difetto.
    """
    hits = {}

    for m in PATHISH.findall(text):
        name = m.strip("./")
        out = git(repo_dir, "ls-files", "*" + os.path.basename(name), check=False)
        for f in out.split():
            hits[f] = hits.get(f, 0) + 10

    idents = []
    for w in IDENT.findall(text):
        if w.lower() in STOP or len(w) > 60:
            continue
        if not CODEISH.search(w):
            continue
        if w not in idents:
            idents.append(w)
    for w in idents[:12]:
        out = subprocess.run(("git", "grep", "-l", "--fixed-strings", w),
                             cwd=repo_dir, capture_output=True, text=True)
        files = [f for f in out.stdout.split() if f]
        # un identificatore presente ovunque non discrimina nulla
        if not files or len(files) > 40:
            continue
        weight = 3 if len(files) <= 5 else 1
        for f in files:
            p = f.replace("\\", "/")
            if p.startswith(WEAK_PREFIX) or p.endswith(WEAK_SUFFIX):
                weight_f = weight * 0.25
            else:
                weight_f = weight
            hits[f] = hits.get(f, 0) + weight_f

    ranked = [f for f, _ in sorted(hits.items(), key=lambda kv: -kv[1])
              if not denied(f)]
    return ranked[:MAX_CANDIDATES], idents[:12]


def denied(path):
    p = path.replace("\\", "/")
    return p.startswith(DENY) or p.endswith(DENY_SUFFIX)


def excerpt(repo_dir, path, idents, budget):
    """Estratto attorno alle occorrenze, non il file intero."""
    full = os.path.join(repo_dir, path)
    try:
        with open(full, "r", encoding="utf-8", errors="replace") as fh:
            lines = fh.readlines()
    except OSError:
        return ""
    marks = set()
    for i, line in enumerate(lines):
        if any(w in line for w in idents):
            marks.update(range(max(0, i - 12), min(len(lines), i + 13)))
    if not marks:
        marks = set(range(0, min(len(lines), 120)))
    out, prev = [], -2
    for i in sorted(marks):
        if i != prev + 1:
            out.append("...\n")
        out.append("%5d| %s" % (i + 1, lines[i]))
        prev = i
    text = "".join(out)
    return text[:budget]


# ------------------------------------------------------------------- il modello
SYSTEM = """Sei l'agente di manutenzione del progetto Decodium 4, un'applicazione
C++/Qt6/QML per radioamatori (modi digitali FT8/FT4/FT2).

Ti viene data una segnalazione e alcuni estratti di file del repository.

Rispondi ESCLUSIVAMENTE con un oggetto JSON, senza testo attorno:

{
  "analysis": "<analisi in italiano, concisa e verificabile>",
  "confidence": "high" | "medium" | "low",
  "risk": "<cosa potrebbe rompersi con questa modifica>",
  "files": ["percorsi che hai esaminato"],
  "patch": "<diff unificato applicabile con 'git apply', oppure stringa vuota>"
}

Regole non negoziabili:
- Se gli estratti non bastano a stabilire la causa, lascia "patch" vuoto e
  spiega nell'analisi cosa servirebbe. Una diagnosi onesta vale piu' di una
  modifica inventata.
- Il diff deve riferirsi solo ai file mostrati, con percorsi esatti
  (a/<percorso> e b/<percorso>) e contesto sufficiente per applicarsi.
- Niente riformattazioni, niente rinomine, niente modifiche non richieste.
- Non toccare mai .github/, packaging/, tools/github-app/.
- Non inventare API, funzioni o proprieta' che non compaiono negli estratti.
"""


def _clean_json(text):
    text = (text or "").strip()
    if text.startswith("```"):
        text = re.sub(r"^```[a-z]*\n|\n```$", "", text)
    return json.loads(text)


def build_prompt(issue_title, issue_body, files_text):
    return ("SEGNALAZIONE\ntitolo: %s\n\n%s\n\nESTRATTI DEL REPOSITORY\n%s"
            % (issue_title, (issue_body or "")[:8000], files_text))


def ask_via_api(api_key, model, prompt, max_tokens=8000):
    """Percorso a consumo: API Messages con chiave sk-ant-..."""
    body = {
        "model": model,
        "max_tokens": max_tokens,
        "system": SYSTEM,
        "messages": [{"role": "user", "content": prompt}],
    }
    req = urllib.request.Request(API_URL, data=json.dumps(body).encode(),
                                 method="POST")
    req.add_header("x-api-key", api_key)
    req.add_header("anthropic-version", "2023-06-01")
    req.add_header("content-type", "application/json")
    with urllib.request.urlopen(req, timeout=240) as resp:
        payload = json.loads(resp.read())
    return _clean_json("".join(b.get("text", "") for b in payload.get("content", [])))


def ask_via_cli(claude_bin, oauth_token, model, prompt, timeout=300):
    """Percorso ad abbonamento: la CLI di Claude Code con un token durevole.

    Tutti gli strumenti sono disabilitati di proposito: qui serve un'analisi
    testuale, non un agente che tocchi il disco. Le modifiche al repository le
    fa questo modulo, dopo aver verificato il diff.
    """
    env = dict(os.environ)
    if oauth_token:
        env["CLAUDE_CODE_OAUTH_TOKEN"] = oauth_token
    cmd = [claude_bin, "-p", "--bare", "--output-format", "json",
           "--append-system-prompt", SYSTEM,
           "--disallowed-tools", "Bash", "Read", "Write", "Edit", "NotebookEdit",
           "WebFetch", "WebSearch", "Glob", "Grep", "Task", "Agent"]
    if model:
        cmd += ["--model", model]
    p = subprocess.run(cmd, input=prompt, capture_output=True, text=True,
                       timeout=timeout, env=env)
    if p.returncode != 0 and not p.stdout.strip():
        raise RuntimeError("claude: " + (p.stderr or "")[:300])
    wrapper = json.loads(p.stdout)
    if wrapper.get("is_error"):
        raise RuntimeError("claude: " + str(wrapper.get("result"))[:300])
    return _clean_json(wrapper.get("result", ""))


# -------------------------------------------------------------- verifica diff
def validate_patch(repo_dir, patch):
    """Il diff e' accettabile? Restituisce (ok, motivo, file toccati)."""
    if not patch or not patch.strip():
        return False, "nessuna modifica proposta", []

    touched = re.findall(r"^\+\+\+ b/(.+)$", patch, re.M)
    touched = [t.strip() for t in touched if t.strip() != "/dev/null"]
    if not touched:
        return False, "il diff non nomina alcun file", []
    if len(touched) > MAX_FILES:
        return False, "tocca %d file, il limite e' %d" % (len(touched), MAX_FILES), touched
    for f in touched:
        if denied(f):
            return False, "percorso non consentito: %s" % f, touched
        if not os.path.exists(os.path.join(repo_dir, f)):
            return False, "file inesistente: %s" % f, touched

    changed = sum(1 for l in patch.splitlines()
                  if (l.startswith("+") or l.startswith("-"))
                  and not l.startswith(("+++", "---")))
    if changed > MAX_CHANGED_LINES:
        return False, "%d righe cambiate, il limite e' %d" % (changed, MAX_CHANGED_LINES), touched

    # la prova che conta: si applica davvero?
    p = subprocess.run(("git", "apply", "--check", "-"), cwd=repo_dir,
                       input=patch, capture_output=True, text=True)
    if p.returncode != 0:
        return False, "non si applica: " + p.stderr.strip()[:200], touched
    return True, "%d file, %d righe" % (len(touched), changed), touched


def open_pull_request(repo_dir, api, token, full_name, branch, title, body, patch):
    """Applica, spinge e apre la pull request. Non fonde: qui non c'e' merge."""
    git(repo_dir, "checkout", "-B", branch)
    p = subprocess.run(("git", "apply", "-"), cwd=repo_dir, input=patch,
                       capture_output=True, text=True)
    if p.returncode != 0:
        raise RuntimeError("git apply: " + p.stderr.strip()[:300])
    git(repo_dir, "-c", "user.name=decodium-agent",
        "-c", "user.email=decodium-agent@users.noreply.github.com",
        "commit", "-a", "-m", title)
    git(repo_dir, "push", "-f", "origin", branch)
    status, pr = api("POST", "/repos/%s/pulls" % full_name, token, {
        "title": title, "head": branch, "base": "main", "body": body,
        "maintainer_can_modify": True, "draft": True,
    })
    git(repo_dir, "checkout", "main", check=False)
    return pr
