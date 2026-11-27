#!/usr/bin/env python3
"""Compare two FTx stage binaries on identical WAVs, one slot at a time."""

import argparse
from collections import Counter
import glob
import hashlib
import json
from pathlib import Path
import re
import sys
import time

from bench import PROFILES, norm_msg, run_decodium

_LINE = re.compile(r'^\s+- .*\bfreq=\s*(-?\d+)\s+.*\bdecoded="([^"]*)"\s*$')


def parse_stage4(output, stage=4):
    """Use message and rounded frequency, matching the stage comparator key."""
    lines = output.splitlines()
    header_pattern = re.compile(r"^stage %d: nout=(\d+)\s*$" % stage)
    headers = [(i, header_pattern.match(line)) for i, line in enumerate(lines)]
    headers = [(i, match) for i, match in headers if match]
    if len(headers) != 1:
        raise ValueError("Expected exactly one stage %d result" % stage)
    index, header = headers[0]
    count = int(header.group(1))
    found = Counter()
    for line in lines[index + 1:index + 1 + count]:
        match = _LINE.match(line)
        if not match:
            raise ValueError("Malformed stage %d decode: %s" % (stage, line))
        found[(int(match.group(1)), norm_msg(match.group(2)))] += 1
    if sum(found.values()) != count:
        raise ValueError("Incomplete stage %d result" % stage)
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True, help="Reference stage comparator")
    parser.add_argument("--candidate", required=True, help="Candidate stage comparator")
    parser.add_argument("--wavs", required=True, help="Directory or glob of slot WAV files")
    parser.add_argument("--mode", choices=("ft2", "ft4", "ft8"), default="ft8")
    parser.add_argument("--profile", choices=PROFILES, default="deep")
    parser.add_argument("--depth", type=int, choices=range(1, 5),
                        help="Override decoder depth for both builds")
    parser.add_argument("--nfa", type=int, default=200)
    parser.add_argument("--nfb", type=int, default=4000)
    parser.add_argument("--nfqso", type=int, default=1500)
    parser.add_argument("--max-ms", type=int, default=20000)
    parser.add_argument("--timeout", type=int, default=300)
    parser.add_argument("--output", required=True, help="JSON results file")
    args = parser.parse_args()
    path = Path(args.wavs)
    wavs = sorted(path.glob("*.wav")) if path.is_dir() else [Path(p) for p in sorted(glob.glob(args.wavs))]
    if not wavs:
        parser.error("No WAV files found")
    executables = [str(Path(args.baseline).resolve()), str(Path(args.candidate).resolve())]
    if not all(Path(exe).is_file() for exe in executables):
        parser.error("Both decoder binaries must exist")
    stage = 7 if args.mode == "ft2" else 4
    if args.mode == "ft8":
        options = list(PROFILES[args.profile])
        if args.depth is not None:
            options[options.index("--depth") + 1] = str(args.depth)
        options += ["--no-early", "--max-ms", str(args.max_ms)]
    else:
        if args.profile == "harvest":
            parser.error("harvest is only available for FT8")
        depth_option = "--ndepth" if args.mode == "ft2" else "--depth"
        depth = args.depth if args.depth is not None else (3 if args.profile == "prod" else 4)
        options = ["--stages", str(stage), depth_option, str(depth)]
    options += ["--nfqso", str(args.nfqso), "--nfa", str(args.nfa), "--nfb", str(args.nfb)]
    rows = []
    for index, wav in enumerate(wavs):
        wav = wav.resolve()
        results = [None, None]
        # Alternate execution order to reduce systematic warm-cache bias.
        for which in ((0, 1) if index % 2 == 0 else (1, 0)):
            start = time.monotonic()
            output = run_decodium([executables[which]] + options + [str(wav)], args.timeout)
            elapsed = time.monotonic() - start
            results[which] = (parse_stage4(output, stage), elapsed)
        missing = results[0][0] - results[1][0]
        added = results[1][0] - results[0][0]
        encode = lambda counter: [dict(freq_hz=freq, message=message, count=count)
                                  for (freq, message), count in sorted(counter.items())]
        rows.append(dict(wav=str(wav), sha256=hashlib.sha256(wav.read_bytes()).hexdigest(),
                         baseline_seconds=results[0][1], candidate_seconds=results[1][1],
                         baseline_count=sum(results[0][0].values()),
                         candidate_count=sum(results[1][0].values()),
                         missing=encode(missing), added=encode(added)))
        print("%s: baseline=%d candidate=%d missing=%d added=%d" %
              (wav.name, rows[-1]["baseline_count"], rows[-1]["candidate_count"],
               sum(missing.values()), sum(added.values())), flush=True)
    report = dict(baseline=executables[0], candidate=executables[1],
                  baseline_sha256=hashlib.sha256(Path(executables[0]).read_bytes()).hexdigest(),
                  candidate_sha256=hashlib.sha256(Path(executables[1]).read_bytes()).hexdigest(),
                  mode=args.mode, options=options, slots=rows)
    Path(args.output).write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return 1 if any(row["missing"] for row in rows) else 0


if __name__ == "__main__":
    sys.exit(main())
