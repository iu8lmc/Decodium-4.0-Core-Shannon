# EasyPal 2014 live virtual-audio evidence

The operator used EasyPal 07/OCT/2014 under MetalSharp Wine and transmitted
the two synthetic 320×240 24-bit BMP inputs to a full Decodium receiver over
BlackHole 2ch. Decodium saved `received-a.jp2` and `received-b.jp2`. A's RGB
pixels match its input exactly; B's mean absolute RGB error is 0.000143 on a
0–255 scale and its maximum error is 2. These files are distinct from each
other. The earlier recording of A did not acquire in the QSSTV batch receiver.

`easypal-b.flac` is a lossless 47-second crop of a 48 kHz mono PortAudio
capture of the B transmission, including about two seconds of leading silence.
Its SHA-256 is
`f00001ada624839ee6b3f66600b13c1a570e26afd2b88c6942e69ab11a970ec6`.
QSSTV-Experimental independently decoded the crop to exactly `received-b.jp2`
and reported B/2.5 kHz/high/16-QAM/long. The Decodium offline capture analyzer
also recovered that exact JP2. This is one recorded signal, not an A/B audio
fixture or proof of native Windows/RF operation.
The cropped capture was also replayed over BlackHole into a fresh full
Decodium application and yielded the exact 5,533-byte JP2
(`.../decodium-qsstv-drm-app-9a8snp3e`).

In the reverse direction Decodium transmitted `reverse-a.jpg` and
`reverse-b.jpg` to EasyPal via BlackHole. The operator saw both images in the
RX window. EasyPal saved `received-easypal-reverse-a.jpg` and
`received-easypal-reverse-b.jpg`; these saved JPEGs differ in bytes from the
sources, with RGB mean absolute differences of 2.011 and 1.006 respectively.
See `docs/sstv/HAMDRM_COMPATIBILITY_MATRIX.md` for crossed controls, exact
hashes, provenance and remaining interoperability limits.

`bsr-extended-a.bin` and `bsr-extended-b.bin` are complete requests saved by
EasyPal after deliberately interrupted receives; `bsr-a.bin` is only an
incomplete EasyPal transient and is not a wire-format fixture. Both complete
requests were encoded as audio by a Decodium test helper and recognised by a
Decodium sender; EasyPal received the repairs and the operator confirmed zero
remaining segments for both. EasyPal's own BSR audio transmission was not
verified in this environment.

`received-rs2-a.rs2` and `received-rs2-b.rs2` are the RS2 containers saved
by EasyPal after receiving the original reverse A/B JPEGs with RS2 enabled.
They match Decodium's encoder and archived TRXAMADRM 3.6's independent RS2
encoder byte-for-byte; its decoder reconstructs the inputs exactly. EasyPal
decoded A but reported FAIL for B, so B is not an interoperability pass.

`easypal-tx-a.rs2` and `easypal-tx-b.rs2` come from EasyPal's AutosaveTX
for the earlier live transmissions. Decodium decodes their complete protected
payloads exactly to `received-a.jp2` and `received-b.jp2` and re-encodes them
byte-for-byte. EasyPal appends 11 bytes after its original JP2 files before
encoding; the received JP2 fixtures preserve those bytes. `test_hamdrm_rs`
checks both directions and crossed-image negatives using these independent
containers. These fixtures do not resolve the separate EasyPal RX failure.

`decodium-rs2-a.flac` and `decodium-rs2-b.flac` preserve the signed 16-bit
stereo WAVs used in the pinned QSSTV-Experimental 10.0 batch comparison.
QSSTV returned A byte-exact and B with 125 extra zero bytes; the executable
observation can be repeated with `check_qsstv_rs2_padding_observation.py`.
The discrepancy is retained as a negative result, not a B interoperability
pass, and does not establish the cause of EasyPal's separate failure.
`decodium-rs2-sign127.flac` and `decodium-rs2-sign128.flac` contain two
further controls based on B with five or four zero bytes appended to the
JPEG respectively, making the RS2 padding header 127 or 128. The pinned QSSTV
receiver decodes the former exactly and the latter with 121 extra zeros. The
regression retries a no-image acquisition up to three times, but never retries
or hides a mismatched decoded image.

`decodium-rs2-compat-a.flac` and `decodium-rs2-compat-b.flac` record actual
Decodium transmissions with the default-off JPEG compatibility option. Both
test inputs were first padded to RS2 field 128, and the option added one more
byte; pinned QSSTV 10.0 reconstructed those transmitted bytes exactly with
unchanged image pixels. Run `check_qsstv_rs_compat_ab.py` for the offline
comparison. The selected original files and transmitted files differ by that
intentional byte; this is not a byte-preserving conversion.

The matching `decodium-rs1-compat-*`, `decodium-rs3-compat-*` and
`decodium-rs4-compat-*` FLAC recordings cover the other protection levels.
The same replay script checks all eight recordings and their decoded pixels.
All source test images were prepared with RS padding field 128; each actual
Decodium transmission then added one zero as explicitly requested.

From the repository root, after building `analyze_hamdrm_capture`, run
`python3 tests/sstv/tools/check_easypal_2014_live_fixtures.py` to check A/B
pixel comparisons, the crossed references and the B audio decode. FFmpeg is
required; this offline regression does not replace the live exchange above.
