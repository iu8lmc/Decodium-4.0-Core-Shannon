#!/usr/bin/env python3
"""Compare saved WSJT-X and Decodium JT65 message text, ignoring frequency/SNR."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'tests/bridge_refactor_evidence'
official = (root / 'jt65-official-wsjtx-reference.txt').read_text()
app = (root / 'jt65-reference-fixed-ab.txt').read_text()
reference = {match.group(1).strip() for match in re.finditer(r'^\d{4}\s+[-\d]+\s+[-\d.]+\s+\d+\s+#\s+(.+)$', official, re.M)}
if len(reference) != 9:
    raise SystemExit(f'expected 9 reference messages, got {len(reference)}')
for label in ('before', 'after'):
    raw = next(line for line in app.splitlines() if line.startswith(label + ' raw '))
    messages = {m.group(1).strip() for m in re.finditer(r'#\s+([^|\]\']+)', raw)}
    if messages != reference:
        raise SystemExit(f'{label}: missing={reference-messages}, extra={messages-reference}')
    print(f'{label}: {len(messages)} message texts match official reference')
