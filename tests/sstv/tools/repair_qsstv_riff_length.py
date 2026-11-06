#!/usr/bin/env python3
"""Fix only a stale RIFF size in a bounded canonical PCM WAV copy.

The input must already contain a complete data chunk with a correct size;
this does not reconstruct missing audio or assert that transmission completed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    try:
        if not args.source.is_file() or not 44 <= args.source.stat().st_size <= 128 * 1024 * 1024:
            raise ValueError('source is missing or outside the bounded size')
        data = bytearray(args.source.read_bytes())
        if (data[:4] != b'RIFF' or data[8:16] != b'WAVEfmt '
                or struct.unpack_from('<I', data, 16)[0] != 16
                or data[36:40] != b'data'):
            raise ValueError('expected canonical PCM WAV with a 44-byte header')
        encoding, channels, rate, byte_rate, block_align, bits = struct.unpack_from('<HHIIHH', data, 20)
        if (encoding != 1 or channels not in (1, 2) or not 8000 <= rate <= 192000
                or bits != 16 or block_align != channels * 2
                or byte_rate != rate * block_align):
            raise ValueError('unsupported PCM format')
        original = struct.unpack_from('<I', data, 4)[0]
        data_length = struct.unpack_from('<I', data, 40)[0]
        if original != 36 or data_length != len(data) - 44 or data_length == 0 or data_length % block_align:
            raise ValueError('RIFF or data lengths do not match the expected stale-header case')
        pcm_hash = hashlib.sha256(data[44:]).hexdigest()
        struct.pack_into('<I', data, 4, len(data) - 8)
        with args.destination.open('xb') as output:
            output.write(data)
        print(json.dumps({'source': str(args.source), 'destination': str(args.destination),
                          'data_bytes': data_length, 'pcm_sha256': pcm_hash,
                          'wav_sha256': hashlib.sha256(data).hexdigest()}, sort_keys=True))
    except (OSError, ValueError, struct.error) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
