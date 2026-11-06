#!/usr/bin/env python3
"""Decode FT4 slot WAVs with Decodium and compare them with a JTDX ALL.TXT window."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path


MODE_MARKERS = {"~", "#", "@", "$", "%", "&", "^", "?", "*", "`", ".", ":"}
TIME_RE = re.compile(
    r"^\s*(?:(?P<date8>\d{8})|(?P<date6>\d{6}))?[_\s-]?"
    r"(?P<time>\d{2}:?\d{2}:?\d{2}|\d{4})\b"
)
CALL_RE = re.compile(
    r"\b(?=[A-Z0-9/]{3,13}\b)(?:[A-Z]{1,3}|[0-9][A-Z])?[0-9][A-Z0-9/]{1,10}\b"
)
SKIP_CALLS = {"CQ", "RR73", "QRZ", "DE", "DX"}


def parse_hms(value: str) -> int:
    raw = value.replace(":", "")
    if len(raw) == 4:
        raw += "00"
    if len(raw) != 6:
        raise ValueError(f"invalid time: {value}")
    return int(raw[:2]) * 3600 + int(raw[2:4]) * 60 + int(raw[4:6])


def fmt_hms(seconds: int) -> str:
    return f"{seconds // 3600:02d}:{(seconds % 3600) // 60:02d}:{seconds % 60:02d}"


def normalize_message(message: str) -> str:
    tokens = message.upper().split()
    while tokens and (tokens[-1] in MODE_MARKERS or re.fullmatch(r"A\d+", tokens[-1])):
        tokens.pop()
    return " ".join(tokens)


def parse_all_line(line: str, yyyymmdd: str, start_sec: int, end_sec: int) -> dict | None:
    match = TIME_RE.match(line)
    if not match:
        return None
    date_token = match.group("date8") or match.group("date6")
    if date_token and len(date_token) == 6:
        date_token = "20" + date_token
    if date_token != yyyymmdd:
        return None

    raw_time = match.group("time").replace(":", "")
    if len(raw_time) == 4:
        raw_time += "00"
    slot_sec = parse_hms(raw_time)
    if slot_sec < start_sec or slot_sec > end_sec:
        return None

    parts = line[match.end():].split()
    if len(parts) >= 6 and re.fullmatch(r"\d+(?:\.\d+)?", parts[0]) and parts[1] in {"Rx", "Tx"}:
        parts = parts[3:]
    if len(parts) < 4:
        return None

    try:
        snr = int(parts[0])
        delta_t = float(parts[1])
        freq = int(parts[2])
    except ValueError:
        return None

    message_start = 4 if parts[3] in MODE_MARKERS else 3
    if message_start >= len(parts):
        return None
    message = normalize_message(" ".join(parts[message_start:]))
    if not message:
        return None
    return {
        "ts": fmt_hms(slot_sec),
        "snr": snr,
        "dt": delta_t,
        "freq": freq,
        "msg": message,
        "raw": line.rstrip("\n"),
    }


def load_jtdx(path: Path, yyyymmdd: str, start_sec: int, end_sec: int) -> list[dict]:
    rows = []
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            row = parse_all_line(line, yyyymmdd, start_sec, end_sec)
            if row:
                rows.append(row)
    return rows


def collect_hash_seeds(rows: list[dict]) -> list[str]:
    seeds = []
    seen = set()
    for row in rows:
        text = row["msg"].upper().replace("<", " ").replace(">", " ")
        for call in CALL_RE.findall(text):
            if call in SKIP_CALLS or re.fullmatch(r"R?[+-]?\d{1,2}", call):
                continue
            if call not in seen:
                seen.add(call)
                seeds.append(call)
    return seeds


def slot_timestamp(path: Path) -> str:
    raw = path.stem.split("_", 1)[1]
    return f"{int(raw[:2]):02d}:{int(raw[2:4]):02d}:{int(raw[4:6]):02d}"


def parse_decode_line(ts: str, line: str) -> dict | None:
    match = re.search(
        r"snr=\s*([+-]?\d+)\s+dt=\s*([+-]?\d+(?:\.\d+)?)\s+freq=\s*([0-9]+).*decoded=\"(.*)\"",
        line,
    )
    if not match:
        return None
    return {
        "ts": ts,
        "snr": int(match.group(1)),
        "dt": float(match.group(2)),
        "freq": int(match.group(3)),
        "msg": normalize_message(match.group(4)),
    }


def decode_slots(args: argparse.Namespace, seeds: list[str]) -> list[dict]:
    slot_files = sorted(Path(args.slots_dir).expanduser().glob("slot_*.wav"))
    if not slot_files:
        raise SystemExit(f"no slot_*.wav files in {args.slots_dir}")

    seed_args = []
    if not args.no_seed_hash:
        for seed in seeds:
            seed_args.extend(["--seed-hash-call", seed])

    command_base = [
        str(Path(args.decoder).expanduser()),
        "--stages", str(args.stage),
        "--depth", str(args.depth),
        "--nfqso", str(args.nfqso),
        "--nfa", str(args.nfa),
        "--nfb", str(args.nfb),
        *seed_args,
    ]

    env = os.environ.copy()
    if args.env:
        for item in args.env:
            key, _, value = item.partition("=")
            if not key or not _:
                raise SystemExit(f"invalid --env item: {item}")
            env[key] = value

    started = time.time()
    rows = []
    for index, slot_path in enumerate(slot_files, 1):
        ts = slot_timestamp(slot_path)
        completed = subprocess.run(
            [*command_base, str(slot_path)],
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=args.timeout,
            check=False,
        )
        if completed.returncode != 0:
            raise SystemExit(f"decode failed for {slot_path}:\n{completed.stderr[-2000:]}")
        for line in completed.stdout.splitlines():
            row = parse_decode_line(ts, line)
            if row:
                rows.append(row)
        if args.progress and index % args.progress == 0:
            print(
                f"processed {index}/{len(slot_files)} elapsed={time.time() - started:.1f}s",
                file=sys.stderr,
                flush=True,
            )
    return rows


def row_key(row: dict) -> tuple[str, str]:
    return row["ts"], row["msg"]


def split_hash_equiv(jtdx_only: list[dict], d4_rows: list[dict]) -> tuple[list[dict], list[dict]]:
    by_slot = {}
    for row in d4_rows:
        by_slot.setdefault(row["ts"], []).append(row)

    hash_equiv = []
    true_only = []
    for row in jtdx_only:
        tokens = row["msg"].split()
        matched = None
        if "<...>" in tokens:
            for candidate in by_slot.get(row["ts"], []):
                candidate_tokens = candidate["msg"].split()
                if len(candidate_tokens) == len(tokens) and all(
                    left == "<...>" or left == right
                    for left, right in zip(tokens, candidate_tokens)
                ):
                    matched = candidate
                    break
        if matched:
            hash_equiv.append({"jtdx": row, "d4": matched})
        else:
            true_only.append(row)
    return true_only, hash_equiv


def compare(d4_rows: list[dict], jtdx_rows: list[dict], label: str, slots: int, seeds: int,
            elapsed: float) -> dict:
    by_d4 = {row_key(row): row for row in d4_rows}
    by_jtdx = {row_key(row): row for row in jtdx_rows}
    common = set(by_d4) & set(by_jtdx)
    d4_only = [by_d4[key] for key in sorted(set(by_d4) - set(by_jtdx))]
    jtdx_only = [by_jtdx[key] for key in sorted(set(by_jtdx) - set(by_d4))]
    true_only, hash_equiv = split_hash_equiv(jtdx_only, d4_rows)
    return {
        "counts": {
            "label": label,
            "d4": len(by_d4),
            "jtdx": len(by_jtdx),
            "common": len(common),
            "d4_only": len(d4_only),
            "jtdx_only": len(jtdx_only),
            "net": len(by_d4) - len(by_jtdx),
            "jtdx_only_true": len(true_only),
            "hash_equiv_false": len(hash_equiv),
            "slots": slots,
            "seeds": seeds,
            "elapsed": round(elapsed, 1),
        },
        "d4": d4_rows,
        "jtdx": jtdx_rows,
        "d4_only": d4_only,
        "jtdx_only": jtdx_only,
        "jtdx_only_true": true_only,
        "hash_equiv_false": hash_equiv,
    }


def write_summary(path: Path, result: dict, details: int) -> None:
    counts = result["counts"]
    lines = [
        (
            f"{counts['label']} d4={counts['d4']} jtdx={counts['jtdx']} "
            f"common={counts['common']} d4_only={counts['d4_only']} "
            f"jtdx_only={counts['jtdx_only']} net={counts['net']} "
            f"jtdx_only_true={counts['jtdx_only_true']} "
            f"hash_equiv_false={counts['hash_equiv_false']} slots={counts['slots']} "
            f"seeds={counts['seeds']} elapsed={counts['elapsed']}"
        ),
        "JTDX-only true sample:",
    ]
    for row in result["jtdx_only_true"][:details]:
        lines.append(
            f"  {row['ts']} {row['snr']:>3} {row['dt']:>4.1f} "
            f"{row['freq']:>4} {row['msg']}"
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slots-dir", required=True)
    parser.add_argument("--jtdx-all", required=True)
    parser.add_argument("--date", required=True, help="YYYYMMDD")
    parser.add_argument("--start", required=True, help="HH:MM:SS")
    parser.add_argument("--end", required=True, help="HH:MM:SS")
    parser.add_argument("--decoder", required=True, help="ft4_stage_compare executable")
    parser.add_argument("--label", default="ft4_compare")
    parser.add_argument("--json-out", required=True)
    parser.add_argument("--summary-out", required=True)
    parser.add_argument("--stage", type=int, default=4)
    parser.add_argument("--depth", type=int, default=3)
    parser.add_argument("--nfqso", type=int, default=1500)
    parser.add_argument("--nfa", type=int, default=200)
    parser.add_argument("--nfb", type=int, default=5000)
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--progress", type=int, default=10)
    parser.add_argument("--details", type=int, default=40)
    parser.add_argument("--no-seed-hash", action="store_true")
    parser.add_argument("--env", action="append", default=[], help="Extra environment KEY=VALUE")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    started = time.time()
    start_sec = parse_hms(args.start)
    end_sec = parse_hms(args.end)
    jtdx_rows = load_jtdx(Path(args.jtdx_all).expanduser(), args.date, start_sec, end_sec)
    seeds = collect_hash_seeds(jtdx_rows)
    d4_rows = decode_slots(args, seeds)
    slots = len(list(Path(args.slots_dir).expanduser().glob("slot_*.wav")))
    result = compare(d4_rows, jtdx_rows, args.label, slots, 0 if args.no_seed_hash else len(seeds),
                     time.time() - started)
    Path(args.json_out).write_text(json.dumps(result, indent=2), encoding="utf-8")
    write_summary(Path(args.summary_out), result, args.details)
    print(Path(args.summary_out).read_text(encoding="utf-8"), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
