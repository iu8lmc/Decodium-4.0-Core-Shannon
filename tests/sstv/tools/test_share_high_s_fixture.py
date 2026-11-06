#!/usr/bin/env python3
"""Known-vector controls for the test-only ECDSA malleability fixture."""
import base64
from share_https_fixture import high_s_signature

sig_a = bytes.fromhex('3045022100efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf371602200834e36ad29a83bf2bc9385e491d6099c8fdf9d1ed67aa7ea5f51f93782857a9')
high_a = bytes.fromhex('3046022100efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716022100f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8')
sig_b = bytes.fromhex('3045022100f1abb023518351cd71d881567b1ea663ed3efcf6c5132b354f28d3b0b7d383670220019f4113742a2b14bd25926b49c649155f267e60d3814b4c0cc84250e46f0083')
enc = lambda value: base64.b64encode(value).decode('ascii')
assert high_s_signature(enc(sig_a)) == enc(high_a)
assert high_s_signature(enc(sig_b)) != enc(sig_b)
for invalid in (b'', sig_a + b'\0', high_a, b'\x30\x06\x02\x01\x00\x02\x01\x01'):
    try:
        high_s_signature(enc(invalid))
    except ValueError:
        pass
    else:
        raise AssertionError('invalid or already high-S signature accepted')
print('PASS high-S fixture known-vector A/B and malformed controls')
