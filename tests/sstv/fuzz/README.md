# SSTV parser fuzz targets

The libFuzzer targets are developer-only and disabled by default. Configure a
separate build from the repository root with an upstream Clang installation
that supplies libFuzzer, ASan, and UBSan:

```sh
cmake -S . -B build-fuzz -G Ninja \
  -DBUILD_TESTING=ON \
  -DDECODIUM_ENABLE_SSTV=ON \
  -DDECODIUM_ENABLE_SSTV_FUZZING=ON \
  -DWSJT_GENERATE_DOCS=OFF \
  -DCMAKE_CXX_COMPILER=/path/to/clang++
cmake --build build-fuzz --target \
  fuzz_sstv_protocol_parsers fuzz_sstv_wav_pcm_reader \
  fuzz_sstv_share_parsers fuzz_sstv_incoming_media fuzz_sstv_qso_log \
  fuzz_hamdrm_parsers
```

Example bounded local runs:

```sh
build-fuzz/tests/sstv/fuzz_sstv_wav_pcm_reader \
  tests/sstv/fuzz/corpus/wav -max_total_time=60 -max_len=1048576
build-fuzz/tests/sstv/fuzz_sstv_protocol_parsers \
  tests/sstv/fuzz/corpus/protocol -max_total_time=60 -max_len=4096
build-fuzz/tests/sstv/fuzz_sstv_share_parsers \
  tests/sstv/fuzz/corpus/share -max_total_time=60 -max_len=1048576
build-fuzz/tests/sstv/fuzz_sstv_incoming_media \
  tests/sstv/fuzz/corpus/incoming-media -max_total_time=60 -max_len=1048576
build-fuzz/tests/sstv/fuzz_sstv_qso_log \
  tests/sstv/fuzz/corpus/qso -max_total_time=60 -max_len=262144
build-fuzz/tests/sstv/fuzz_hamdrm_parsers \
  tests/sstv/fuzz/corpus/hamdrm -max_total_time=60 -max_len=1048576
```

`valid_pcm16.hex` is a text-safe binary seed. The WAV harness recognizes its
`hex:` prefix and decodes it before invoking `SstvWavPcmReader`. The ordinary
`test_sstv_fuzz_smoke` CTest verifies all committed WAV, sharing, and SSTV ADIF
seeds plus core round-trip invariants without requiring Clang or libFuzzer.
`test_sstv_protocol_fuzz_smoke` drives the exact VIS/N-VIS/FSK-ID libFuzzer
harness through 2,050 deterministic edge and hostile cases, so the same parser
boundary also runs under ordinary sanitizer builds when a libFuzzer runtime is
not installed.

`test_sstv_incoming_media_fuzz_smoke` drives the actual remote-image staging
boundary through committed 1x1 PNG and JPEG seeds plus 256 deterministic
hostile byte streams. It validates hash/MIME, bounded decode, metadata-free PNG
normalisation and restart reinspection without using any external provider.
