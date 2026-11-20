"""Synthetic hybrid body fixture for isolated waveform tests."""

def hybrid_fixture(variant, local_ftp=False, upload=False):
    fields = ('a.example.invalid', 'operator-a', 'dummy-A', 'radio/a') if variant == 'a' else ('b.example.invalid', 'operator-b', 'dummy-B-1234', 'radio/b')
    if upload:
        fields = ('127.0.0.1', 'operator', 'dummy', 'radio')
    elif local_ftp:
        fields = ('127.0.0.1', *fields[1:])
    plain = ('?' + fields[0] + '"' + fields[1] + '<' + fields[2] + '>' + fields[3] + ':')[::-1]
    digits = [ord(c) for c in plain.encode().hex()]
    while len(digits) % 4:
        digits.append(0)
    encoded = bytearray()
    for i in range(0, len(digits), 2):
        for value in (4 * digits[i] + 7 * digits[i + 1], 5 * digits[i] + digits[i + 1]):
            hi, lo = value >> 8, value & 255
            if value == 0:
                hi, lo = 0xfe, 0xff
            elif lo == 0:
                hi, lo = 0xfd, hi
            encoded.extend((hi, lo))
    return bytes(encoded) + b'\r\n'
