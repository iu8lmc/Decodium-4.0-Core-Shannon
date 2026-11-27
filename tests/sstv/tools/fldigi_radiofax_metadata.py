#!/usr/bin/env python3
"""Validate the minimal WEFAX metadata emitted by FLdigi laboratory captures."""
import re


def has_radiofax_stop_metadata(raw: bytes, expected_lpm: int = 120) -> bool:
    if not raw.startswith(b'\x89PNG\r\n\x1a\n') or expected_lpm not in (60, 90, 120, 240):
        return False
    pos = 8
    while pos + 12 <= len(raw):
        length = int.from_bytes(raw[pos:pos + 4], 'big')
        end = pos + 12 + length
        if end > len(raw):
            return False
        if raw[pos + 4:pos + 8] == b'tEXt':
            keyword, sep, comment = raw[pos + 8:pos + 8 + length].partition(b'\x00')
            if sep and keyword == b'Comment':
                return (re.search(rb'(?m)^LPM:' + str(expected_lpm).encode('ascii') + rb'\r?$', comment) is not None
                        and re.search(rb'(?im)^Comment:APT stop\b', comment) is not None)
        pos = end
    return False
