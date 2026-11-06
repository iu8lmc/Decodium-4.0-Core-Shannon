#!/usr/bin/env python3
"""A/B FLdigi WEFAX metadata and malformed/incorrect controls."""
from fldigi_radiofax_metadata import has_radiofax_stop_metadata


def png(comment: bytes) -> bytes:
    body = b'Comment\x00' + comment
    return b'\x89PNG\r\n\x1a\n' + len(body).to_bytes(4, 'big') + b'tEXt' + body + bytes(4)


for text in (b'LPM:120\nComment:APT Stop: curr_freq = 450\n',
             b'LPM:120\r\nComment:APT stop: curr_freq = 448\r\n'):
    assert has_radiofax_stop_metadata(png(text))
    assert not has_radiofax_stop_metadata(png(text), 240)
    assert not has_radiofax_stop_metadata(png(text)[:-1])
    assert not has_radiofax_stop_metadata(png(text.replace(b'APT ', b'XYZ ')))
assert not has_radiofax_stop_metadata(b'not png')
print('PASS FLdigi metadata A/B and negative controls')
