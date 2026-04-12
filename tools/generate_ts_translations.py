#!/usr/bin/env python3
"""Generate missing Qt TS translations from wsjtx_en.ts using web translation."""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path


TARGETS = {
    "de": "de_DE",
    "fr": "fr_FR",
}

NUMERUS_FORMS = {
    "de": 2,
    "fr": 2,
}

TRANSLATE_ENDPOINT = "https://translate.googleapis.com/translate_a/single"
MAX_ENCODED_BATCH = 3000
REQUEST_DELAY_SECONDS = 0.15

GLOSSARY_TERMS = [
    "Decodium",
    "WSJT-X",
    "WSJTX",
    "JTAlert",
    "GridTracker",
    "Gridtracker",
    "PSK Reporter",
    "CQRLOG",
    "Hamlib",
    "QMap",
    "QMAP",
    "QSO",
    "QRZ",
    "DXCC",
    "ADIF",
    "LoTW",
    "eQSL",
    "CAT",
    "PTT",
    "VOX",
    "NTP",
    "UDP",
    "TCP",
    "TCP/IP",
    "URL",
    "UTC",
    "RX",
    "TX",
    "T/R",
    "TR",
    "CQ",
    "QRM",
    "QRA64",
    "MSK144",
    "FST4W",
    "FST4",
    "FT2",
    "FT4",
    "FT8",
    "JT4",
    "JT9",
    "JT65",
    "Q65",
    "WSPR",
    "DXped",
    "Fox",
    "Hound",
    "Cabrillo",
]

PLACEHOLDER_PATTERNS = [
    re.compile(r"%\{[^}]+\}"),
    re.compile(r"%L\d+"),
    re.compile(r"%\d+"),
    re.compile(r"%n"),
    re.compile(r"<[^>]+>"),
    re.compile(r"&"),
]

UPPERCASE_TOKEN_RE = re.compile(r"\b(?:[A-Z]{2,}[A-Z0-9+/_-]*|[A-Z0-9]+(?:-[A-Z0-9]+)+)\b")
ONLY_SYMBOLS_RE = re.compile(r"^[\s0-9.,:+\-_/()\\[\]{}|*#]+$")


def normalize_whitespace(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def should_translate(text: str) -> bool:
    if not text or not text.strip():
        return False
    if ONLY_SYMBOLS_RE.fullmatch(text):
        return False
    return any(ch.isalpha() for ch in text)


def protect_text(text: str) -> tuple[str, list[tuple[str, str]]]:
    protected = normalize_whitespace(text)
    replacements: list[tuple[str, str]] = []
    token_index = 0

    def next_token() -> str:
        nonlocal token_index
        token = f"ZXPROTECT{token_index:05d}ZX"
        token_index += 1
        return token

    for term in sorted(GLOSSARY_TERMS, key=len, reverse=True):
        pattern = re.compile(re.escape(term))

        def term_repl(match: re.Match[str]) -> str:
            token = next_token()
            replacements.append((token, match.group(0)))
            return token

        protected = pattern.sub(term_repl, protected)

    for pattern in PLACEHOLDER_PATTERNS:
        def generic_repl(match: re.Match[str]) -> str:
            token = next_token()
            replacements.append((token, match.group(0)))
            return token

        protected = pattern.sub(generic_repl, protected)

    def uppercase_repl(match: re.Match[str]) -> str:
        token = next_token()
        replacements.append((token, match.group(0)))
        return token

    protected = UPPERCASE_TOKEN_RE.sub(uppercase_repl, protected)
    return protected, replacements


def restore_text(text: str, replacements: list[tuple[str, str]]) -> str:
    restored = text
    for token, value in reversed(replacements):
        restored = restored.replace(token, value)
    return restored


def translate_batch(strings: list[str], target: str) -> list[str]:
    markers = [f"ZXMARKER{i:04d}ZX" for i in range(len(strings) - 1)]
    joined_parts: list[str] = []
    for index, value in enumerate(strings):
        joined_parts.append(value)
        if index < len(markers):
            joined_parts.append(markers[index])
    joined = "\n".join(joined_parts)
    params = {
        "client": "gtx",
        "sl": "en",
        "tl": target,
        "dt": "t",
        "q": joined,
    }
    url = f"{TRANSLATE_ENDPOINT}?{urllib.parse.urlencode(params)}"
    with urllib.request.urlopen(url, timeout=60) as response:
        payload = response.read().decode("utf-8")
    data = json.loads(payload)
    translated = "".join(part[0] for part in data[0])
    for marker in markers:
        translated = translated.replace(f"{marker}\n", marker).replace(f"\n{marker}", marker)
    pieces = [translated]
    for marker in markers:
        new_pieces: list[str] = []
        for piece in pieces:
            new_pieces.extend(piece.split(marker))
        pieces = new_pieces
    if len(pieces) != len(strings):
        raise RuntimeError(f"marker split mismatch: expected {len(strings)}, got {len(pieces)}")
    return [piece.strip("\n") for piece in pieces]


def batched_translate(strings: list[str], target: str) -> list[str]:
    batches: list[list[str]] = []
    current: list[str] = []
    current_size = 0
    for item in strings:
        projected = current_size + len(urllib.parse.quote(item)) + 32
        if current and projected > MAX_ENCODED_BATCH:
            batches.append(current)
            current = []
            current_size = 0
        current.append(item)
        current_size += len(urllib.parse.quote(item)) + 32
    if current:
        batches.append(current)

    translated: list[str] = []
    for index, batch in enumerate(batches, start=1):
        translated.extend(translate_batch(batch, target))
        print(f"[{target}] batch {index}/{len(batches)}", file=sys.stderr, flush=True)
        time.sleep(REQUEST_DELAY_SECONDS)
    return translated


def generate_translation(source_path: Path, target_code: str, output_path: Path) -> None:
    if target_code not in TARGETS:
        raise ValueError(f"unsupported target {target_code}")

    parser = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))
    tree = ET.parse(source_path, parser=parser)
    root = tree.getroot()
    root.set("language", TARGETS[target_code])

    cache: dict[str, str] = {}
    protected_sources: dict[str, tuple[str, list[tuple[str, str]]]] = {}
    ordered_texts: list[str] = []

    for message in root.findall(".//message"):
        source = message.findtext("source") or ""
        if not should_translate(source):
            continue
        if source not in protected_sources:
            protected_sources[source] = protect_text(source)
            ordered_texts.append(source)

    protected_strings = [protected_sources[source][0] for source in ordered_texts]
    translated_strings = batched_translate(protected_strings, target_code)

    for source, translated in zip(ordered_texts, translated_strings):
        cache[source] = restore_text(translated, protected_sources[source][1])

    for message in root.findall(".//message"):
        source = message.findtext("source") or ""
        translation = message.find("translation")
        if translation is None:
            translation = ET.SubElement(message, "translation")
        translation.attrib.pop("type", None)

        translated = cache.get(source, source)
        if message.get("numerus") == "yes":
            for child in list(translation):
                translation.remove(child)
            translation.text = None
            for _ in range(NUMERUS_FORMS[target_code]):
                numerusform = ET.SubElement(translation, "numerusform")
                numerusform.text = translated
        else:
            for child in list(translation):
                translation.remove(child)
            translation.text = translated

    ET.indent(tree, space="    ")
    xml_body = ET.tostring(root, encoding="unicode")
    output_path.write_text(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE TS>\n" + xml_body + "\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=sorted(TARGETS))
    parser.add_argument(
        "--source",
        default="translations/wsjtx_en.ts",
        help="Source TS file to use as English base",
    )
    parser.add_argument(
        "--output",
        default="",
        help="Output TS file path (defaults to translations/wsjtx_<lang>.ts)",
    )
    args = parser.parse_args()

    source_path = Path(args.source)
    output_path = Path(args.output) if args.output else Path(f"translations/wsjtx_{args.target}.ts")
    generate_translation(source_path, args.target, output_path)
    print(output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
