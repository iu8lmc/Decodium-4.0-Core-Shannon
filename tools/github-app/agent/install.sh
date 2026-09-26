#!/usr/bin/env bash
# Installazione di decodium-agent su un host Debian/Ubuntu con systemd.
# Va eseguito sul VPS, come root:   sudo bash install.sh
#
# Non contiene alcun segreto: la chiave privata e il segreto del webhook li
# metti tu, e restano sulla macchina.
set -euo pipefail

DIR=/opt/decodium-agent
ENVF=/etc/decodium-agent.env
USER=decoagent
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

[ "$(id -u)" -eq 0 ] || { echo "serve root: sudo bash install.sh" >&2; exit 1; }

echo "== dipendenze =="
if ! python3 -c 'import cryptography' 2>/dev/null; then
  apt-get update -qq
  apt-get install -y --no-install-recommends python3 python3-cryptography
fi
python3 -c 'import cryptography, sys; print("python3-cryptography", cryptography.__version__)'

echo "== utente di servizio =="
id -u "$USER" >/dev/null 2>&1 || useradd --system --home-dir "$DIR" --shell /usr/sbin/nologin "$USER"

echo "== file =="
install -d -o root -g root -m 0755 "$DIR"
install -o root -g root -m 0644 "$SRC/decodium_agent.py" "$DIR/decodium_agent.py"
install -o root -g root -m 0644 "$SRC/decodium_analysis.py" "$DIR/decodium_analysis.py"
install -o root -g root -m 0644 "$SRC/decodium-agent.service" /etc/systemd/system/decodium-agent.service

if [ ! -f "$ENVF" ]; then
  cat > "$ENVF" <<'EOF'
# Configurazione di decodium-agent. Questo file contiene un segreto:
# deve restare 0640 root:decoagent e non va mai versionato.
DECOAGENT_APP_ID=4558437
DECOAGENT_PRIVATE_KEY=/etc/decodium-agent/private-key.pem
DECOAGENT_WEBHOOK_SECRET=DA_COMPILARE
DECOAGENT_BIND=127.0.0.1
DECOAGENT_PORT=8787
DECOAGENT_REPOS=iu8lmc/Decodium-4.0-Core-Shannon
DECOAGENT_LABEL=decodium-eligible
# Fase 2 - analisi del codice e proposta di modifica.
# Senza chiave l'agente si limita a confermare la ricezione.
# Strada consigliata: token durevole da abbonamento, ottenuto con
#   claude setup-token
CLAUDE_CODE_OAUTH_TOKEN=
DECOAGENT_CLAUDE_BIN=/usr/local/bin/claude
# In alternativa, chiave API a consumo (sk-ant-...). Se ci sono entrambe
# vince l'abbonamento.
DECOAGENT_ANTHROPIC_KEY=
DECOAGENT_MODEL=claude-opus-5
DECOAGENT_REPO_DIR=/var/lib/decodium-agent/repo
# Finche' vale 1 l'agente non scrive nulla su GitHub: registra soltanto.
DECOAGENT_DRY_RUN=1
EOF
  echo "creato $ENVF (da completare)"
else
  echo "$ENVF esiste gia': lasciato invariato"
fi
chown root:"$USER" "$ENVF"
chmod 0640 "$ENVF"

install -d -o root -g "$USER" -m 0750 /etc/decodium-agent
if [ -f /etc/decodium-agent/private-key.pem ]; then
  chown root:"$USER" /etc/decodium-agent/private-key.pem
  chmod 0640 /etc/decodium-agent/private-key.pem
fi

echo "== servizio =="
systemctl daemon-reload
systemctl enable decodium-agent >/dev/null

cat <<EOF

Installato. Restano tre cose, e sono tue:

 1. Copia la chiave privata dell'app in
        /etc/decodium-agent/private-key.pem
    poi:  chown root:$USER /etc/decodium-agent/private-key.pem && chmod 0640 \$_

 2. Sulla pagina dell'app su GitHub genera un webhook secret, scrivilo in
        $ENVF   (riga DECOAGENT_WEBHOOK_SECRET)
    e attiva il webhook con URL
        https://groups.ft2.it/decodium-agent/webhook

 3. Aggiungi a nginx il blocco di nginx-decodium-agent.conf nel server che
    serve groups.ft2.it, PRIMA della regola che reindirizza, poi:
        nginx -t && systemctl reload nginx

Infine:
        systemctl start decodium-agent
        systemctl status decodium-agent --no-pager
        curl -fsS https://groups.ft2.it/decodium-agent/health

Finche' DECOAGENT_DRY_RUN=1 l'agente non scrive nulla su GitHub: mettilo a 0
solo dopo aver visto nel registro le consegne arrivare e la firma tornare.
        journalctl -u decodium-agent -f
EOF
