#!/usr/bin/env python3
"""Compare two WSJT/JTDX/Decodium ALL.TXT files over a UTC window."""

from __future__ import annotations

import argparse
import dataclasses
import datetime as dt
import re
import statistics
from pathlib import Path


TIME_RE = re.compile(
    r"^\s*(?:(?P<date8>\d{8})|(?P<date6>\d{6}))?[_\s-]?(?P<time>\d{2}:?\d{2}:?\d{2}|\d{4})\b"
)
CALL_RE = re.compile(r"\b(?=[A-Z0-9/]{3,12}\b)(?:[A-Z]{1,3}|[0-9][A-Z])?[0-9][A-Z0-9/]{1,9}\b")
MODE_MARKERS = {"~", "#", "@", "$", "%", "&", "^", "?", "*", "`"}
SKIP_CALL_TOKENS = {
    "73",
    "CQ",
    "DE",
    "DX",
    "QRZ",
    "RR73",
}


@dataclasses.dataclass(frozen=True)
class Row:
    timestamp: dt.datetime
    snr: int
    delta_t: float
    freq: int
    message: str
    source_line: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--a", required=True, help="First ALL.TXT path")
    parser.add_argument("--b", required=True, help="Second ALL.TXT path")
    parser.add_argument("--label-a", default="A")
    parser.add_argument("--label-b", default="B")
    parser.add_argument("--date", required=True, help="UTC date, YYYY-MM-DD")
    parser.add_argument("--start", required=True, help="UTC start, HH:MM:SS")
    parser.add_argument("--end", required=True, help="UTC end, HH:MM:SS")
    return parser.parse_args()


def parse_date_token(value: str | None, fallback_date: dt.date) -> dt.date:
    if not value:
        return fallback_date
    if len(value) == 8:
        return dt.datetime.strptime(value, "%Y%m%d").date()
    parsed = dt.datetime.strptime(value, "%y%m%d").date()
    return parsed


def parse_time_token(value: str) -> dt.time:
    raw = value.replace(":", "")
    if len(raw) == 4:
        raw += "00"
    return dt.datetime.strptime(raw, "%H%M%S").time()


def parse_row(line: str, fallback_date: dt.date) -> Row | None:
    time_match = TIME_RE.match(line)
    if not time_match:
        return None

    row_date = parse_date_token(
        time_match.group("date8") or time_match.group("date6"), fallback_date
    )
    row_time = parse_time_token(time_match.group("time"))
    parts = line[time_match.end() :].split()
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

    message_start = 3
    if parts[3] in MODE_MARKERS:
        message_start = 4
    if message_start >= len(parts):
        return None

    message = " ".join(parts[message_start:]).strip()
    return Row(
        timestamp=dt.datetime.combine(row_date, row_time),
        snr=snr,
        delta_t=delta_t,
        freq=freq,
        message=message,
        source_line=line.rstrip("\n"),
    )


def load_rows(path: Path, fallback_date: dt.date, start: dt.datetime, end: dt.datetime) -> list[Row]:
    rows: list[Row] = []
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            row = parse_row(line, fallback_date)
            if row and start <= row.timestamp <= end:
                rows.append(row)
    return rows


def calls_in_message(message: str) -> set[str]:
    calls = set()
    for match in CALL_RE.findall(message.upper()):
        cleaned = match.strip("/")
        if cleaned not in SKIP_CALL_TOKENS and not re.fullmatch(r"R?[+-]?\d{2}", cleaned):
            calls.add(cleaned)
    return calls


def cq_call(message: str) -> str | None:
    tokens = message.upper().split()
    if not tokens or tokens[0] != "CQ":
        return None
    for token in tokens[1:]:
        if token in {"DX", "DE"}:
            continue
        found = calls_in_message(token)
        if found:
            return sorted(found)[0]
    return None


def summarize(label: str, rows: list[Row]) -> dict[str, object]:
    calls = set()
    cq_calls = set()
    cq_rows = 0
    for row in rows:
        calls.update(calls_in_message(row.message))
        call = cq_call(row.message)
        if call:
            cq_rows += 1
            cq_calls.add(call)
    snrs = [row.snr for row in rows]
    return {
        "label": label,
        "rows": len(rows),
        "unique_calls": len(calls),
        "unique_messages": len({row.message for row in rows}),
        "cq_rows": cq_rows,
        "cq_unique_calls": len(cq_calls),
        "avg_snr": round(statistics.fmean(snrs), 2) if snrs else None,
        "min_snr": min(snrs) if snrs else None,
        "weak_le_18": sum(1 for value in snrs if value <= -18),
        "weak_le_20": sum(1 for value in snrs if value <= -20),
        "weak_le_22": sum(1 for value in snrs if value <= -22),
    }


def key(row: Row) -> tuple[dt.datetime, str]:
    return row.timestamp, row.message


def print_summary(summary: dict[str, object]) -> None:
    print(
        f"{summary['label']}: rows={summary['rows']} unique_calls={summary['unique_calls']} "
        f"unique_msgs={summary['unique_messages']} cq_rows={summary['cq_rows']} "
        f"cq_unique_calls={summary['cq_unique_calls']} avg_snr={summary['avg_snr']} "
        f"min_snr={summary['min_snr']} weak<=-18={summary['weak_le_18']} "
        f"weak<=-20={summary['weak_le_20']} weak<=-22={summary['weak_le_22']}"
    )


def main() -> int:
    args = parse_args()
    fallback_date = dt.date.fromisoformat(args.date)
    start = dt.datetime.combine(fallback_date, parse_time_token(args.start))
    end = dt.datetime.combine(fallback_date, parse_time_token(args.end))

    rows_a = load_rows(Path(args.a).expanduser(), fallback_date, start, end)
    rows_b = load_rows(Path(args.b).expanduser(), fallback_date, start, end)

    print_summary(summarize(args.label_a, rows_a))
    print_summary(summarize(args.label_b, rows_b))

    by_key_a = {key(row): row for row in rows_a}
    by_key_b = {key(row): row for row in rows_b}
    common_keys = sorted(set(by_key_a) & set(by_key_b))
    snr_deltas = [by_key_a[item].snr - by_key_b[item].snr for item in common_keys]
    if snr_deltas:
        print(
            f"common slot+message: {len(common_keys)} "
            f"avg_snr_delta={statistics.fmean(snr_deltas):.2f} "
            f"median_snr_delta={statistics.median(snr_deltas):.2f}"
        )
    else:
        print("common slot+message: 0")

    cq_a = {call for row in rows_a if (call := cq_call(row.message))}
    cq_b = {call for row in rows_b if (call := cq_call(row.message))}
    print(f"{args.label_a}-only CQ calls: {', '.join(sorted(cq_a - cq_b)) or '-'}")
    print(f"{args.label_b}-only CQ calls: {', '.join(sorted(cq_b - cq_a)) or '-'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
