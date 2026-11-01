# QSSTV MOT data-group oracle

A developer-only harness executes the actual `CMOTDABEnc` implementation in
QSSTV commit `8c27d6d169d8c6c197eb47c2089870e39bc06a02`. It produces a
synthetic four-byte object named `sample.jpg`; those bytes are not a valid JPEG
and this exercise tests MOT framing rather than image decoding. Its empty
format argument intentionally takes QSSTV's default JPEG subtype, avoiding a
pinned-source undefined string transform for nonempty formats. The harness is
separate from Decodium and does not build or distribute QSSTV code.

On the tested macOS environment with Qt 6 available through pkg-config:

```sh
git clone https://github.com/ON4QZ/QSSTV.git /tmp/decodium-qsstv-reference
git -C /tmp/decodium-qsstv-reference checkout --detach 8c27d6d169d8c6c197eb47c2089870e39bc06a02
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/datadecoding \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core) tests/sstv/tools/qsstv_mot_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/datadecoding/DABMOT.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/util/CRC.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core) -o /tmp/qsstv-mot-oracle
/tmp/qsstv-mot-oracle
/tmp/qsstv-mot-oracle segmented
```

The default output lines are the header and first body group. The optional
`segmented` argument emits a 103-byte object in three segments, including
QSSTV run-in order (last segment first) and a duplicate. They are embedded in
`test_hamdrm_core`, which parses and assembles them and compares Decodium
output as described below. The first comparison exposed an actual
CRC mismatch: QSSTV uses MSB-first polynomial `0x1021`, initial `0xffff`,
final xor `0xffff`, whereas the previous Decodium MOT path used reflected
CRC-16/X-25. The MOT path now uses the existing MSB-first channel CRC routine.
The reflected utility remains available but is not used for MOT. For historical
local partial snapshots, load verifies the full file SHA-256
first, checks each old reflected CRC and translates matching groups in memory;
it leaves the original file unchanged. Live or network groups with the old CRC
are rejected. A focused persistence test covers this recovery.

The segmented fixture is parsed and assembled in QSSTV transmission order;
Decodium-generated headers and segment payloads are compared separately from
continuity indices, which QSSTV increments for every repeated transmission.

The packet oracle executes QSSTV `CDataEncoder::GeneratePacket`, with a stub
only for fetching the next data unit after the three packets under test:

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/datadecoding \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network) \
  tests/sstv/tools/qsstv_packet_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/datadecoding/DataDecoder.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/datadecoding/DABMOT.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/util/CRC.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network) \
  -o /tmp/qsstv-packet-oracle
/tmp/qsstv-packet-oracle
```

Its three packets, including the padded last packet, are byte-identical to
Decodium output and reassemble into the same 35-byte data unit in
`test_hamdrm_waveform`. These checks prove executed QSSTV-encoder-to-Decodium
MOT and packet vectors plus byte-level generation comparisons. They do not
prove execution of
QSSTV's decoder, a real image, BSR exchange, OFDM waveforms, two applications,
EasyPal or over-the-air interoperability. The restricted upstream files listed
in `UPSTREAM_PROVENANCE.md` are not compiled or copied into Decodium.

## FAC convolution and bit interleaver

The channel oracle invokes the pinned QSSTV `CConvEncoder` for a deterministic
48-bit FAC input (pattern 6, 45 cells), and `CBitInterleaver` on a 254-bit
input with `t0=13`. It can be reproduced on the same macOS/Qt environment:

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/mlc \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network) \
  tests/sstv/tools/qsstv_channel_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/ChannelCode.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/ConvEncoder.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/BitInterleaver.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/interleaver/BlockInterleaver.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network) \
  -o /tmp/qsstv-channel-oracle
/tmp/qsstv-channel-oracle
```

`test_hamdrm_channel` checks exact output against both independently executed
vectors and recovers each original input using Decodium's inverse operations.
These are two individual coding-layer vectors, not an independent FAC frame,
MSC profile matrix, RF waveform or QSSTV receiver test.

## Complete FAC coding-chain fixture

`qsstv_fac_oracle.cpp` invokes QSSTV's FAC parameter writer, energy dispersal,
convolutional encoder, bit interleaver and QAM mapper independently of the
Decodium implementation. It emits 288 combinations: three frame identities,
two bandwidth flags, two interleaver depths, three MSC constellation flags,
two protection flags and four packet IDs, using `TEST123` as the label. The
fixture stores the complete 48-bit FAC payload and the signs of the 45 QAM
cells; it does not measure their floating-point amplitude or an OFDM signal.
The checked-in `qsstv-fac-8c27d6d.txt` has SHA-256
`73354262eb2e96984084725e21cdfb865bec773a03016770e7e83106b8d0397b`.

The FAC data vector is explicitly zero-initialized before the QSSTV writer
runs, so the reserved bits have a defined value. The harness invokes the same
QSSTV primitives in the order used by its FAC MLC encoder; it does not invoke
the whole QSSTV application or its receiver. Build and regenerate it on macOS:

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/FAC \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/mlc \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network) \
  tests/sstv/tools/qsstv_fac_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/FAC/FAC.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/Parameter.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping/CellMappingTable.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/util/CRC.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/ChannelCode.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/ConvEncoder.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/BitInterleaver.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/interleaver/BlockInterleaver.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/EnergyDispersal.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/QAMMapping.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network) \
  -o /tmp/qsstv-fac-oracle
/tmp/qsstv-fac-oracle > /tmp/qsstv-fac-vectors.txt
cmp /tmp/qsstv-fac-vectors.txt tests/sstv/fixtures/qsstv-fac-8c27d6d.txt
```

`test_hamdrm_channel` checks every fixture row for exact payload and QAM sign
agreement and decodes the corresponding normalized cells. This supports FAC
coding compatibility for these fields and input label; it does not establish
cell placement, frame synchronization, MSC waveform or on-air interoperability.

## Complete MSC MLC fixture

`qsstv_msc_oracle.cpp` invokes the pinned QSSTV `CMSCMLCEncoder` initialization
and its actual `ProcessDataInternal` implementation. A developer harness
supplies input/output vectors and stubs only a logging method needed by an
unused inherited buffer-interface vtable. It sets the useful cell count to
339, 369, 455, 530, 647 or 704, then executes QAM4/QAM16/QAM64 at each of two
protection levels. These are 36 combinations using a deterministic LCG input;
the six counts match Decodium's currently documented cell-plan counts, but
this oracle does not independently validate how either implementation derives
those counts or arranges the cells in a frame.

Each output cell is quantized to an integer constellation level, separately
for real and imaginary components, and stored as a letter representing -7..7.
The fixture `qsstv-msc-8c27d6d.txt` has SHA-256
`7d4bfdeabc87c5b3a305c750d7131bde3ee34fb67829f2f6aa8a8bfc2bb5883f`.
Decodium compares every quantized cell and input capacity and decodes the
normalized reference symbols to the original input in `test_hamdrm_channel`.
This establishes individual MLC encoding/decoding comparisons; it is not a
sampled waveform or a QSSTV receiver execution.

On the tested macOS/Qt environment the fixture can be reproduced with:

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/mlc \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network) \
  tests/sstv/tools/qsstv_msc_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/MLC.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/Parameter.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping/CellMappingTable.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/ChannelCode.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/ConvEncoder.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/BitInterleaver.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/interleaver/BlockInterleaver.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/EnergyDispersal.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/mlc/QAMMapping.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network) \
  -o /tmp/qsstv-msc-oracle
/tmp/qsstv-msc-oracle > /tmp/qsstv-msc-vectors.txt
cmp /tmp/qsstv-msc-vectors.txt tests/sstv/fixtures/qsstv-msc-8c27d6d.txt
```

## Cell-map oracle

The developer-only `tests/sstv/tools/qsstv_cellmap_oracle.cpp` invokes the
pinned QSSTV `CCellMappingTable::MakeTable` for all six A/B/E and 2300/2500 Hz
profiles. The checked-in fixture records the dimensions, useful MSC count,
the QSSTV cell flags for every carrier in every superframe symbol, and
the complex values of the pilot cells.
`test_hamdrm_channel` compares each cell's MSC, FAC, time, frequency,
scattered, and boosted-pilot flags against this independent output. The
pilot complex values are compared with an absolute tolerance of 1e-6. This
does not establish over-the-air reception or full QSSTV interoperability.

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network) \
  tests/sstv/tools/qsstv_cellmap_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping/CellMappingTable.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network) \
  -o /tmp/qsstv-cellmap-oracle
/tmp/qsstv-cellmap-oracle | cmp - tests/sstv/fixtures/qsstv-cellmap-8c27d6d.txt
/tmp/qsstv-cellmap-oracle pilots | cmp - tests/sstv/fixtures/qsstv-pilots-8c27d6d.txt
```

## Independent complex OFDM symbols

`qsstv_ofdm_oracle.cpp` executes QSSTV's `COFDMModulation` at the 350 Hz
carrier offset used by its transmitter. The fixture records every complex
sample, including the guard interval, for two deterministic carrier patterns
in each of the six A/B/amateur-E and 2.3/2.5 kHz profiles. Its SHA-256 is
`afd58978cde134e99bf7d29cdc6e57878e15eb58757dc7f9f50420f938769791`.
`test_hamdrm_phy` compares Decodium's scaled, frequency-shifted symbols to
these samples and demodulates the QSSTV symbols back to their input carriers,
using a 2e-7 absolute complex tolerance. The reference is an independent
executed OFDM modulation stage, not a complete QSSTV transmitter or receiver,
real audio exchange, or evidence of EasyPal interoperability.

To regenerate on the tested macOS/Qt environment, using the pinned checkout:

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network fftw3) \
  tests/sstv/tools/qsstv_ofdm_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/OFDM.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/Parameter.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping/CellMappingTable.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/matlib/MatlibStdToolbox.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network fftw3) \
  -o /tmp/qsstv-ofdm-oracle
/tmp/qsstv-ofdm-oracle | cmp - tests/sstv/fixtures/qsstv-ofdm-8c27d6d.txt
```

## Independent symbol cell mapper

`qsstv_mapper_oracle.cpp` calls QSSTV's `COFDMCellMapping` for two
independent input patterns across all six A/B/amateur-E bandwidth profiles and
all symbols in each three-frame superframe. Its fixture has SHA-256
`1e2ec0734118bbaba8042b091cb7b5242537bd1321763792f9352699fe29122a`.
`test_hamdrm_channel` compares each output cell and extracts the original FAC
and MSC from the reference cells while checking pilots. This tests executable
mapping and frame sequencing in addition to the earlier static flag oracle;
it is not a complete QSSTV waveform exchange.

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections -Wno-deprecated-declarations \
  -I/tmp/decodium-qsstv-reference/src/drmtx/common \
  -I/tmp/decodium-qsstv-reference/src/drmtx \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  -I/tmp/decodium-qsstv-reference/src/utils \
  -I/tmp/decodium-qsstv-reference/src \
  -I/tmp/decodium-qsstv-reference/src/config \
  $(pkg-config --cflags Qt6Core Qt6Widgets Qt6Network) \
  tests/sstv/tools/qsstv_mapper_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping/OFDMCellMapping.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/Parameter.cpp \
  /tmp/decodium-qsstv-reference/src/drmtx/common/ofdmcellmapping/CellMappingTable.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core Qt6Widgets Qt6Network) \
  -o /tmp/qsstv-mapper-oracle
/tmp/qsstv-mapper-oracle | cmp - tests/sstv/fixtures/qsstv-mapper-8c27d6d.txt
```

## Synthetic QSSTV PCM through the native receiver

`qsstv_mapper_ofdm_oracle.cpp` executes QSSTV's FAC coder (when requested),
cell mapper and OFDM modulator together, keeping oscillator phase continuous
for a three-frame 48 kHz, 16-bit little-endian PCM superframe. It uses the
amateur E / 2.5 kHz / QAM16 / high-protection profile. Two input patterns are
provided for each case. With synthetic invalid FAC, Decodium synchronizes,
accepts the pilots and rejects FAC. With separately QSSTV-encoded valid FAC
and short interleaving, Decodium accepts frames and reaches an MSC/packet
integrity failure, as expected for the intentionally random MSC cells. Both
outcomes agree across two PCM chunk sizes and emit no false MOT group.

The fixture SHA-256 values are:

- `qsstv-superframe-a-8c27d6d.pcm`: `e1d715b51f54dc19e2848fb6b608b240e337e2318ef5470773b17cc8f5e1f584`
- `qsstv-superframe-b-8c27d6d.pcm`: `c6cf61c4ac2025a1ed2bd20b93e5af539aab6f1ebb006d0a464d33280f5d1a21`
- `qsstv-valid-fac-a-8c27d6d.pcm`: `9bda9b25e6fa0c9d3424042a09d71c213a8d510a7c8dcd88dbe07ff42ffaf2c0`
- `qsstv-valid-fac-b-8c27d6d.pcm`: `fd6acbe03c201ea260ab472ed13d30bfb28110112c8bb9fda9b7f3826843ddf1`

These are generated stage outputs, not recorded audio or a complete QSSTV
MOT transfer. They establish neither image delivery nor EasyPal compatibility.

## QSSTV-generated MOT over PCM

The same developer oracle now runs QSSTV's packet encoder, MSC MLC encoder,
short symbol interleaver, FAC encoder, cell mapper and OFDM modulator in
sequence. The two input data groups were generated separately by QSSTV's MOT
encoder: the header and body for `sample.jpg` with the synthetic four-byte
body `12 34 56 78`. Separate PCM fixtures repeat each group and alternate the
header and body in both orders. `test_hamdrm_waveform` decodes these PCM
streams with two chunk sizes, compares the exact MOT groups, and assembles
both alternating streams to the expected four-byte object. This is a
synthetic object, not a valid JPEG or an exchange with the QSSTV application.
Only E / 2.5 kHz / QAM16 / high protection / short interleaving is covered.

- `qsstv-mot-a-8c27d6d.pcm`: `6989a825ef8ef789c77cd4be13446445e1e68141b4753f7edf0a45eeaa43c8e1`
- `qsstv-mot-b-8c27d6d.pcm`: `6610915ff6a3432c248f58ed0bd6321972f0c62a7cba9a5218ae40d453481256`
- `qsstv-object-a-8c27d6d.pcm`: `7ce7528714b022b4ba3e2d9cba241ba161658d9b366dba06255ab72792b8d320`
- `qsstv-object-b-8c27d6d.pcm`: `8bd1bc93ab9b089dbace643c156a9c43f961231bdddcf04655b4b142cd89faa9`

## Two JPEG images over QSSTV-generated PCM

The oracle's `image` mode feeds a real JPEG into QSSTV's `CMOTDABEnc`, then
runs QSSTV packet, FAC, MSC, short symbol interleaving, cell mapping and OFDM
stages. Nine frames contain the MOT header and segmented body, including the
QSSTV run-in order. The isolated probe preserves the object's transport ID
when constructing its packet helper because the helper's unused slideshow
constructor resets QSSTV's global ID; this is a harness correction, not a
change to QSSTV or Decodium.

For two independently generated 16x16 JPEGs, `test_hamdrm_waveform` recovers
the QSSTV groups from PCM using two chunk sizes, assembles the original JPEG
bytes exactly, validates its metadata and dimensions, and decodes the expected
red or blue pixels. The test covers E / 2.5 kHz / QAM16 / high protection /
short interleaving. It does not execute the full QSSTV application or prove
the reverse direction, live audio, other profiles, or EasyPal interoperability.

- `qsstv-image-a.jpg`: `d8f477eda8727eb80f162760af8f93c7d655e5df81566a665cc53ef8a076ec03`
- `qsstv-image-a-8c27d6d.pcm`: `c60bcbeb531102b0cad62804a5c13ec01b240b9d2cc2a21f80d7c4f7db79fc25`
- `qsstv-image-b.jpg`: `14648ca1cfb6c6b186755a23c247bbc3d9b0eacc690897666676750f56b1b3ad`
- `qsstv-image-b-8c27d6d.pcm`: `3d6552bc2982a583f0629d058842bfd8805a8ca1f4bdadd7ca2da1b08f18152c`

For both JPEGs, the test also feeds the nine recovered data groups to the
Decodium transmitter and compares every native 48 kHz output sample with the
corresponding QSSTV-generated reference, allowing 1.1/32768 absolute error
for signed 16-bit quantization. Packet continuity advances modulo eight across
the QSSTV probe's independent group helper calls. This sample comparison is
strong reverse-direction evidence for this exact sequence and profile, but it
does not execute QSSTV's receiver or establish real-world interoperability.

## Decodium FAC into the QSSTV receiver

`tests/sstv/tools/run_qsstv_fac_rx.sh` checks the separate QSSTV checkout's
revision and the three reference source files before building a temporary
executable around its actual `msdhardfac`, Viterbi and deinterleaver code.
The separately built Decodium exporter supplies 288 FAC vectors spanning three
frame identities, two bandwidths, two interleavers, three constellations, two
protection levels and four packet IDs. The QSSTV receiver recovers every
48-bit payload both unmodified and with one entire QAM cell inverted: 576
independent decode checks. No QSSTV source is linked into the application or
copied into this repository. This exercises the receive FAC coding stage,
not QSSTV's full waveform acquisition, MSC or image receiver.

```sh
QSSTV_REFERENCE_ROOT=/tmp/decodium-qsstv-reference \
  tests/sstv/tools/run_qsstv_fac_rx.sh
```

## Decodium MSC into the QSSTV receiver

`tests/sstv/tools/run_qsstv_msc_rx.sh` builds Decodium's independent bit/cell
exporter and a temporary program calling the pinned QSSTV `msdhardmsc`
receiver, deinterleaver and Viterbi decoder. The harness derives Part-B lengths
and puncture settings from the reference receiver's tables. It verifies 72
native MSC vectors: two input patterns for each combination of six cell
counts, QAM4/16/64 and high/normal protection. Pattern B adds a small 0.015
perturbation to each component of one cell. Every output bit matches its
native input. The separate checkout revision and used reference files are
checked before compilation; no upstream source is copied or linked into the
application. This does not test QSSTV's waveform acquisition, symbol
interleaving over time, packet/MOT receiver, or a live image exchange.

```sh
QSSTV_REFERENCE_ROOT=/tmp/decodium-qsstv-reference \
  tests/sstv/tools/run_qsstv_msc_rx.sh
```

## QSSTV receive map against Decodium temporal interleaving

`qsstv_msc_map_rx_oracle.cpp` invokes QSSTV's actual `mkmscmap` and related
receiver tables for six profiles and both interleaving depths. The generated
fixture SHA-256 is
`40be9c866811b2253f134adc0a67675158ce67476b217f02220d28968efc5cd9`.
`test_hamdrm_channel` fills a simulated six-frame receive ring with native
Decodium interleaver output in the native cell plan and applies QSSTV's exact
map. It recovers two distinct deterministic source sequences for all twelve
profile/depth combinations, after the relevant startup delay and through
zero-frame flushing. The expected reference-map row is one step ahead of the
circular write slot. This checks the actual QSSTV receive map, but not its
real-time sample acquisition or a running QSSTV application.

```sh
clang++ -std=c++17 -ffunction-sections -fdata-sections \
  -I/tmp/decodium-qsstv-reference/src/drmrx \
  $(pkg-config --cflags Qt6Core fftw3) \
  tests/sstv/tools/qsstv_msc_map_rx_oracle.cpp \
  /tmp/decodium-qsstv-reference/src/drmrx/mkmscmap.cpp \
  /tmp/decodium-qsstv-reference/src/drmrx/mkfacmap.cpp \
  /tmp/decodium-qsstv-reference/src/drmrx/deinterleaver.cpp \
  /tmp/decodium-qsstv-reference/src/drmrx/drm.cpp \
  -Wl,-dead_strip $(pkg-config --libs Qt6Core fftw3) \
  -o /tmp/qsstv-msc-map-rx-oracle
/tmp/qsstv-msc-map-rx-oracle | cmp - tests/sstv/fixtures/qsstv-msc-rx-map-8c27d6d.txt
```

## Local two-instance audio test, 2026-10-31

Two Decodium test-mode instances were connected through BlackHole 2ch, with
CAT disabled and VOX selected. The TX completed for each of two different
225-byte JPEG fixtures; RX captured audio, but neither image was reconstructed.
Logs are under `/tmp/decodium-hamdrm-live-a2` and
`/tmp/decodium-hamdrm-live-b`. The B trace showed four pilot failures before
five accepted frames and a packet continuity error. This is a failed live A/B
test, not evidence of QSSTV or EasyPal interoperability.

The test exposed an acquisition bug where the receiver searched the same
initial silence indefinitely. The receiver now advances past fully examined
candidates and retains the incomplete suffix. A waveform regression tests two
different silence prefixes and passes, but this fix alone did not make the
two-instance image transfer succeed. Initial acquisition and packet continuity
still need investigation.

Follow-up: a rejected pilot candidate previously discarded an entire frame,
including the beginning of any valid frame overlapping that candidate. The
receiver now resumes its search after the rejected candidate. Waveform tests
exercise two different partial-frame prefixes, check that pilot candidates
were rejected, and recover both original MOT groups. With this change, the
two-instance BlackHole test succeeded for both JPEG A and JPEG B on profile
`hamdrm-A-2300-high-4-QAM-long`: TX and RX exited successfully, RX saved each
225-byte object, and `cmp` confirmed byte-for-byte identity. Logs are under
`/tmp/decodium-hamdrm-reacq-a` and `/tmp/decodium-hamdrm-reacq-b`.
Each run reported 113 rejected pilot candidates before lock, so acquisition
efficiency and robustness still require work. These local Decodium-to-Decodium
tests do not establish live interoperability with QSSTV or EasyPal.

A follow-up experiment aggregated cyclic-prefix correlations across four
symbols. In a separate BlackHole A transfer the image remained byte-identical,
but rejected pilot candidates rose from 113 in the previous runs to 126;
the timing of separate real-time runs is not controlled well enough to claim
a precise performance regression. The experimental metric was removed because
it did not demonstrate an improvement. The retained waveform regression also
checks recovery of two distinct MOT groups after deterministic broadband-noise
prefixes of 0.4 and 0.6 seconds at 12 kHz. It is a synthetic acquisition test,
not a measurement of RF interference tolerance.

## QSSTV-stage JPEG PCM into the full Decodium application

On macOS, a separate PortAudio process replayed each of the two pinned
QSSTV-generated nine-frame JPEG PCM fixtures into BlackHole 2ch. A full
Decodium test-mode application received through its normal sound input and
HAMDRM controller on `hamdrm-E-2500-high-16-QAM-short`. The process selected a
fresh isolated profile, verified that SSTV actually captured BlackHole, and
played 0.5 seconds of silence followed by four consecutive fixture repetitions.
For both A and B, Decodium saved the JPEG and its 225 bytes matched the
corresponding source file exactly; both application processes exited zero.
A subsequent test with one fixture transmission also reconstructed each JPEG
byte for byte after the same 0.5-second silent lead-in; that is the current
default in the script. Two successful local runs do not establish acquisition
reliability under noise, clock mismatch or other timing conditions.

`--lab-hamdrm-profile` selects a named profile for test-mode HAMDRM RX and TX;
misuse without `--test-mode`, without `--lab-hamdrm`, or without a value is
rejected before startup workers are created. The registered
`test_hamdrm_lab_cli` covers these cases. Reproduce the real-time audio checks
on macOS with BlackHole 2ch and PortAudio:

```sh
python3 tests/sstv/tools/run_qsstv_pcm_app_lab.py a
python3 tests/sstv/tools/run_qsstv_pcm_app_lab.py b
```

The script prints the separate temporary artifact directory for each run.
The source signals were generated by QSSTV encoder stages, but the replay
process is not the full QSSTV application. This demonstrates one-way delivery
of these two fixtures over a local audio device for this one profile, not a
live QSSTV-to-Decodium application exchange, reverse-direction reception by
QSSTV, EasyPal interoperability or RF performance.

## QSSTV-Experimental application DRM TX into Decodium

On 2026-10-31 a locally built QSSTV-Experimental 10.0 at source revision
`a84584248d3307bfa081362031ff20f365337633` encoded two distinct 320×240
test images to DRM WAV. The build contains the one-line signed-PCM patch
`tests/sstv/fixtures/qsstv-experimental-10.0-signed-pcm.patch`; it is not
the official release binary. QSSTV's own batch DRM receiver recovered JPEG2000
objects of 4,998 and 5,021 bytes. Both were then replayed through PortAudio
and BlackHole 2ch into full, isolated Decodium application processes with
CAT disabled. Decodium reconstructed the exact respective JP2 bytes.
The QSSTV batch report identified E/2.5 kHz/high/16-QAM/long; its command
line had requested `--drm-interleave short`, so the observed FAC/profile,
not the requested flag, was used for Decodium RX. The discrepancy in QSSTV's
CLI/profile behavior remains unresolved.

The source WAVs and QSSTV-decoded JP2 files are under
`/tmp/decodium-qsstv-experimental/drm-large-{a,b}.wav` and
`/tmp/decodium-qsstv-experimental/drm-large-{a,b}_1_drm.jp2`; their WAV
SHA-256 hashes are `ed58cde38c579c6d3ba8a94dfb4a8c9d9e6c339f248d5e7415d74552e233a2f7`
and `8e2be29cecd21fb9308babbcc8a19dce9e208cbf9e2bc63e970c3d79e1985490`.
Decodium artifacts and logs are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-app-olvzabwg`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-app-d55elcmp`.
Replay either with `tests/sstv/tools/run_qsstv_exp_drm_app_lab.py WAV JP2`.

The first full-application attempt exposed two defects. On loss of lock the
RX backend reported progress 0 after 0.5, violating the controller's
monotonic progress contract and aborting the session. Progress now indicates
that synchronization has been reached and does not decrease when lock is
lost. Then FAC frames with valid CRC but nonzero unused bits were rejected:
the pinned QSSTV encoder writes no explicit value for those nine bits. The
decoder now ignores their value after checking the CRC; it still validates
profile fields, frame identity and payload CRC. A/B reserved-bit patterns
and bit corruption without a recomputed CRC are covered by the channel test;
the adapter test covers progress after a stream reset.

This is one-way, file-mediated application evidence for a particular profile
and a modified QSSTV build, with no RF or independent security implication.
It does not establish reverse reception, EasyPal, all profile combinations,
or performance under fading and clock mismatch.

## Decodium application TX into QSSTV-Experimental receiver

The first Decodium-to-QSSTV experiment sent a 225-byte JPEG in approximately
4.5 seconds. QSSTV-Experimental 10.0's unmodified macOS release synchronized
and checked some MSC CRCs but produced no image; it acquired too late to
recover the sole MOT header. Repeating that captured waveform did not recover
the image. A longer 7.4 kB JPEG likewise produced data segments but no header
or completed image. Repeating the header before the body allowed QSSTV to
recover headers, but not yet the image before transmission ended.

The native TX adapter now sends each MOT header eight times before the body
and eight times after it, keeping valid frames on air while the receiver
acquires and drains its buffers. The repeated groups count against the
existing bounded queue and byte limits. This increases airtime and reduces
available space within those limits. It does not promise recovery when data
segments are missed.

With this change, two full Decodium application transmissions using
`hamdrm-E-2500-high-16-QAM-long` were captured through BlackHole 2ch and fed
to the official, unmodified QSSTV-Experimental 10.0 receiver in batch mode.
QSSTV reconstructed both distinct 16×16 JPEGs, 225 bytes each, byte-for-byte
equal to the inputs. QSSTV reported 17/16 valid outer CRC blocks, zero bad
outer CRC blocks, and `pkt_bad=2` on each run; it nevertheless
completed the respective image. In this QSSTV version `pkt_bad` also counts
valid intermediate fragments, because `setupDataBlock()` returns false while
waiting for the last fragment; it is not a count of proven invalid packets.
TX durations were 10.816 and 10.621 seconds.
The audio captures and Decodium logs are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-a-63r1r6f0`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-b-8rvtvie8`;
the converted WAVs and QSSTV logs are
`/tmp/decodium-qsstv-experimental/decodium-drm-header-tail-{a,b}.wav` and
the corresponding `.log` files. WAV SHA-256 values are
`e83f0d07bdf4ddab880ec5bffab758567ce46f04ed205b6bbde47fe1a2fa59b3` and
`ee2ea52e77da01e5b77475ff2283e88a3ba3af33e6a850691508e1ac3f97edf9`.

Two additional A/B runs used separate full Decodium TX and RX applications
on the same profile; both JPEGs were recovered byte-for-byte. Artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-two-app-a-8jksjd5h`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-two-app-b-c8k1mxz5`.
The scripts `run_hamdrm_tx_capture_lab.py a|b` and
`run_hamdrm_two_app_lab.py a|b` reproduce those local transmission paths.
The QSSTV receiver ran on a recorded WAV, not a live sound input. These
small fixtures and clean virtual audio do not establish broad profile,
RF, noisy-channel or EasyPal interoperability.

Two further Decodium-to-QSSTV runs used distinct 320×240 JPEG images of
7,624 and 9,976 bytes. QSSTV recovered both byte-for-byte, with respectively
8 and 10 data segments and 104 and 131 successful outer CRC checks, zero
outer CRC failures. Its `pkt_bad` values were 82 and 107; these include
ordinary incomplete fragments as explained above and cannot be interpreted
as that many invalid packets. Transmissions lasted 46.034 and 56.248 seconds.
The source image SHA-256 values are
`5966159f4d284dd99227b8bf5bb097dc07ae303c7e67643bc46944ff0586b20` and
`5b33229631c2c56c9456926163e00057cd8ba48c3264f2f7aca6fee44676ccfb`.
Source images, converted WAVs, independent output images and logs are under
`/tmp/decodium-qsstv-experimental/decodium-image-{a,b}.jpg` and
`/tmp/decodium-qsstv-experimental/decodium-drm-large-{a,b}*`; the raw
Decodium captures and logs are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-a-vv49c9rm`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-b-njanc2h9`.
The larger transfers use the same profile and clean, file-mediated path;
they are not evidence for other profiles, interference or RF.
The separate helper `run_qsstv_exp_drm_decode_lab.py CAPTURE.f32 REFERENCE.jpg`
repeated both independent decodes and byte comparisons successfully; its
outputs are in
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-decode-0q1uwdsh`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-decode-378aj51b`.

### Second bidirectional profile: A/2.3/high/4-QAM/long

Two additional Decodium transmissions on `hamdrm-A-2300-high-4-QAM-long`
were captured through BlackHole; the unmodified QSSTV-Experimental 10.0
batch receiver recovered each distinct 225-byte JPEG exactly. It identified
the expected profile and reported 17 CRC-valid MSC blocks, zero CRC failures,
and `pkt_bad=2` for each run (the fragmented-packet caveat above applies).
Captures are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-a-lv4m_jar`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-b-fsstb7me`;
QSSTV outputs/logs are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-decode-ilyeucf5`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-decode-g9ccvmgv`.

In the reverse direction, the locally rebuilt QSSTV-Experimental application
with the signed-PCM patch encoded two different 320×240 images. Its own DRM
batch receiver identified A/2.3/high/4-QAM/long and recovered JP2 objects of
4,041 and 4,032 bytes; Decodium received both exact byte sequences over
BlackHole. The QSSTV CLI requested `2.2` kHz and `short`, but the actual
receiver report says 2.3/long; these observed values were used for Decodium.
QSSTV artifacts are `/tmp/decodium-qsstv-experimental/drm-mode-a-{a,b}*`;
Decodium artifacts are
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-app-0i_t8tir`
and `/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-app-xfyighxo`.
WAV SHA-256 values are
`fd71e1c745e38f0ba62d14baacfd786bf6379e4d34cbfe24815332878a730ffb` and
`0f7d32ec2f916a0d5808c2db7be7531b6e928d0a511299b3b209f510ab757b92`.
These are clean file-mediated local audio tests, not RF or proof for all
parameter combinations.

### Third profile, Decodium TX to independent QSSTV RX: B/2.5/high/16-QAM/long

Two full Decodium application transmissions through BlackHole were captured
and decoded by the pinned, unmodified QSSTV-Experimental 10.0 batch receiver.
Both distinct 225-byte JPEGs were reconstructed byte-for-byte. QSSTV reported
`B/2.5/Hi/16/Long`, respectively 14 and 15 valid MSC CRCs, and zero bad MSC
CRCs. Each log also reported `pkt_bad=1`, whose fragmented-packet limitation
is described above. Captures and transmitter logs are in
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-a-13sb82pp`
and `.../decodium-hamdrm-tx-b-fa60msy9`; receiver artifacts are in
`.../decodium-qsstv-drm-decode-_t28plt_` and
`.../decodium-qsstv-drm-decode-mchj4szs` under the same temporary directory.
QSSTV logged duplicate Qt framework warnings. The path is clean, local and
file-mediated; RF, other B profiles and larger images were not tested in
these runs.

In the reverse direction, the separately built QSSTV-Experimental revision
`a84584248d3307bfa081362031ff20f365337633`, with the signed-PCM patch
mentioned above, encoded two 320×240 images. Although its CLI was given long
interleaving, its receiver reported `B/2.5/Hi/16/Short` on both WAVs. QSSTV
reconstructed JP2 objects of 4,998 and 5,021 bytes with 83 valid MSC CRCs
and zero reported CRC errors per run. Replaying the WAVs through BlackHole
into two isolated Decodium RX runs configured for the observed short profile
reconstructed those respective JP2 objects byte-for-byte. Artifacts are
`/tmp/decodium-qsstv-experimental/drm-mode-b-{a,b}.wav` and matching
`_1_drm.jp2` files, plus Decodium logs and received objects under
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-app-sr1us3ke`
and `.../decodium-qsstv-drm-app-6j3moymj`. WAV SHA-256 values are
`d122c31a8d7786c82478f65c87e428d7b503f26d271d0af4069b2c16620c6fb2`
and `49572865a3fea8c5121859080f6884ee5650cf62df698a5b6839a91962c2fcfc`.
This does not demonstrate B/long in the QSSTV-to-Decodium direction, an
unmodified QSSTV transmitter, difficult channels or RF.

For the same B/2.5/high/16-QAM/short profile in the opposite direction,
two full Decodium TX applications sent the two distinct 225-byte JPEG
fixtures through BlackHole. The unmodified QSSTV-Experimental 10.0 batch
receiver reported `B/2.5/Hi/16/Short` and reconstructed both byte-for-byte.
It reported 16 valid MSC CRCs and zero bad CRCs per run; `pkt_bad=1` in each
log has the fragmented-packet interpretation described above. TX artifacts:
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-hamdrm-tx-a-bgm_u1yf`
and `.../decodium-hamdrm-tx-b-hcig16vs`. QSSTV artifacts:
`.../decodium-qsstv-drm-decode-u6mrvr8j` and
`.../decodium-qsstv-drm-decode-f68bmz9m` under the same temporary directory.
This is a clean file-mediated local test with small fixtures; it does not
establish RF or difficult-channel performance.

The missing B/2.5/high/16-QAM/long reverse direction was subsequently tested
with the same patched QSSTV TX and two 320×240 images. QSSTV decoded each WAV
as `B/2.5/Hi/16/Long`, with 79 valid MSC CRCs and no reported CRC failures;
two isolated Decodium RX runs reconstructed the respective 4,998- and
5,021-byte JP2 objects exactly. QSSTV was invoked with `--drm-interleave short`.
In source revision `a84584248d3307bfa081362031ff20f365337633`, `src/main.cpp`
maps short to index 0 and long to index 1, while
`src/drmtx/drmtransmitter.cpp` maps 0 to long and 1 to short, explaining the
observed inversion for this build. The WAVs are
`/tmp/decodium-qsstv-experimental/drm-mode-b-long-{a,b}.wav`, with SHA-256
`cc5b63dcd6c07dd0982e95f124c53e7511e5eef8afadf637ffd5a1a51da34b65`
and `50b0d462b7f2ea12827f858c1051fabc28dfef65a96679225b7074063b2c559c`.
Decodium artifacts are under
`/var/folders/8v/ygsh3p0x3gz2jmd2pm9wzz500000gn/T/decodium-qsstv-drm-app-eoe2amjq`
and `.../decodium-qsstv-drm-app-47ogxhhf`. These remain clean local,
file-mediated tests with a patched transmitter; RF and unmodified QSSTV TX
are not established.

## Native MOT packets into QSSTV receive methods

`tests/sstv/tools/run_qsstv_packet_rx.sh` builds native Decodium A/B objects,
segments them and packetizes their MOT groups using two different fixed packet
sizes. A separate temporary executable compiles the exact `setupDataBlock`
and `setupDataPacket` method bodies extracted from the pinned QSSTV
`src/drmrx/sourcedecoder.cpp`, plus QSSTV's CRC implementation. The shim
provides the fields and globals those methods use, with logging disabled;
it is not the complete QSSTV source decoder or application. The script
checks the source revision and both reference files before extracting them.

For 24 intact MOT groups (two JPEG images, two packet sizes), all parsed kinds,
segment numbers, last flags, transport IDs and payloads matched. The harness
then reassembled the parsed payloads itself into four objects and compared
all bytes with the original A/B JPEG files. It decoded each reconstructed
16×16 image and checked its center pixel. It also verified that one-bit
corruption in every outer packet fails the reference CRC, and that 24 altered
MOT groups with freshly generated valid packet CRCs are rejected by QSSTV's
inner CRC check. A further 12 A/B cases change a packet's continuity index
while recomputing its packet CRC; the QSSTV method rejects the sequence and
clears continuity state. This covers the indicated QSSTV receive methods and the
harness's assembly; it does not run QSSTV's full object assembly or RX chain.

```sh
QSSTV_REFERENCE_ROOT=/tmp/decodium-qsstv-reference \
  tests/sstv/tools/run_qsstv_packet_rx.sh
```

## BSR generator and repair-request parser comparison

`bash tests/sstv/tools/run_qsstv_bsr_oracle.sh` checks the pinned QSSTV
revision and extracts its unmodified `sourceDecoder::storeBSR` and
`drmTx::processFIX` method bodies into a temporary shim. Six representative
missing-segment sets plus all 255 nonempty patterns in each eight-segment half
of a 16-segment object are checked in both compact and extended forms, for
1,032 cases. The cases alternate two transport IDs, segment sizes and image
filenames. Decodium's generated bytes match
QSSTV's, except for an optional final newline after the extended profile;
both parsers expand the same requested segment numbers, and all truly missing
segments are included. Compact ranges can also request already received
segments, matching the pinned QSSTV generator's behavior. Two malformed
range cases are rejected by Decodium; malformed input was not passed to the
reference methods. This is a method-level differential comparison, not an
on-air BSR exchange, a full QSSTV application test, or independent EasyPal
evidence.
