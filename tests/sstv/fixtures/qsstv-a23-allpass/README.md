# QSSTV A/2.3 all-pass A/B regression fixtures

The `a` and `b` recordings contain signed 16-bit little-endian mono PCM at
12 kHz, compressed with zlib. They originate from two different 320x240
test images encoded by QSSTV-Experimental revision
`a84584248d3307bfa081362031ff20f365337633`, using the signed-PCM patch
and local instrumentation described in `docs/sstv/HAMDRM_QSSTV_ORACLE.md`.
Hybrid instrumentation was disabled. The signals were resampled to 12 kHz
and processed with FFmpeg `allpass=f=1500:width_type=q:width=2`.

The unmodified pinned QSSTV 10.0 receiver independently recovered the two
277-byte JP2 reference files from 48 kHz stereo conversions of these PCM
signals. The regression feeds the original 12 kHz PCM to the Decodium
offline analyzer with the temporal candidate alone and merged with adaptive
reception, checks exact recovery, and rejects swapped references. This is not
a live transmitter or RF test.
