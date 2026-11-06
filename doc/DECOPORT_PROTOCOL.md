# DecoPort v1 — a radio on the network, whatever the radio is

DecoPort exposes an ordinary transceiver — one with a USB CAT port and a USB
audio codec — as a network device. A client that speaks DecoPort tunes, changes
mode, keys the transmitter and carries the audio both ways **without knowing
what radio is on the other end**, and without being told: the gateway finds the
radio by itself and announces it.

The packet layout is inspired by VITA-49 (a framed stream with timestamps, a
context packet describing the source, and a command packet controlling it), but
it is deliberately smaller and self-describing, and its control vocabulary is
neutral rather than vendor-specific.

## What makes it different from Hamlib

Hamlib is a library of radio dialects: excellent at speaking to a specific
radio, and it asks you which one. DecoPort asks nothing. The gateway enumerates
the serial ports and audio endpoints the operating system already knows about,
pairs the ones that belong to the same USB device, and publishes what it found.
A client sees `rigLabel` as a courtesy string to display — never as something to
branch on.

Underneath, the gateway may well use Hamlib, or a native driver, or OmniRig, or
TCI. That choice is the gateway's business and never crosses the wire.

## Transport

UDP. Two ports:

| port | purpose |
|---|---|
| 5559 | session: everything a client and a gateway say to each other |
| 5560 | announce: the gateway's beacon, broadcast on the local network |

UDP because the payload is real-time audio: a late packet is worse than a lost
one, and there is nothing to retransmit. Losses show up as a sequence gap and
the client conceals them.

## Common header

28 bytes, **big-endian**, on every packet.

| offset | size | field |
|---|---|---|
| 0 | 4 | magic `DPRT` (0x44505254) |
| 4 | 1 | version — 1 |
| 5 | 1 | type — see below |
| 6 | 2 | flags — bit 0: the timestamp is meaningful |
| 8 | 4 | streamId — the gateway's identifier, stable while it runs |
| 12 | 4 | sequence — per type, wraps |
| 16 | 4 | timestamp, whole seconds since the Unix epoch, UTC |
| 20 | 4 | timestamp, nanoseconds within the second |
| 24 | 2 | payload length in bytes |
| 26 | 2 | reserved, zero |

### Packet types

| value | name | direction | payload |
|---|---|---|---|
| 1 | ANNOUNCE | gateway → broadcast | context fields |
| 2 | HELLO | client → gateway | none |
| 3 | BYE | client → gateway | none |
| 4 | KEEPALIVE | client → gateway | none |
| 5 | CONTEXT | gateway → client | context fields |
| 6 | COMMAND | client → gateway | context fields |
| 7 | AUDIO_RX | gateway → client | PCM |
| 8 | AUDIO_TX | client → gateway | PCM |
| 9 | STATUS | gateway → client | context fields |

A client that has not been heard from for **twelve seconds** is dropped. A
gateway that has not announced for twelve seconds is considered gone.

## Context fields

CONTEXT, COMMAND, ANNOUNCE and STATUS all carry the same structure: a 32-bit
field mask, then the present fields in ascending bit order. A CONTEXT names
everything it knows; a COMMAND names only what it wants changed. There is no
separate grammar for reading and writing, which is the point — the same encoder
serves both.

| bit | field | encoding |
|---|---|---|
| 0 | rfFrequencyHz | int64, hertz |
| 1 | mode | uint8, neutral enum |
| 2 | ptt | uint8, 0 or 1 |
| 3 | sMeter | int16, dBm × 10 |
| 4 | audioSampleRate | uint32, hertz |
| 5 | audioChannels | uint8 |
| 6 | bandwidthHz | uint32 |
| 7 | rigLabel | uint8 length, then UTF-8 |
| 8 | stateFlags | uint32, see below |
| 9 | txAudioLeadMs | uint16 |
| 10 | sessionPort | uint16 |
| 11 | forwardPower | uint16, watts × 10 |
| 12 | swr | uint16, ratio × 100 (100 is 1.00) |
| 13 | alc | int16, per cent × 10 |
| 14 | drainVoltage | uint16, volts × 100 |
| 15 | drainCurrent | uint16, amperes × 100 |
| 16 | paTemperature | int16, degrees Celsius × 10 |
| 17 | compression | uint16, decibels × 10 |
| 18 | powerSetting | uint16, per cent × 10 |

Bits 11 to 18 are meters, and a meter is only ever sent when it has been read.
A rig that does not report its drain current simply leaves bit 15 clear, and
the absence is the answer: there is no reserved value meaning "unknown", so a
client can never mistake one for a reading. `powerSetting` is the exception in
kind — it is where the operator put the knob, not what the antenna received —
and it is here because on most rigs it is the only one of the eight that can
be read while the transmitter is at rest.

`stateFlags`: bit 0 CAT online, bit 1 audio input online, bit 2 audio output
online, bit 3 the gateway will key the transmitter, bit 4 a client already holds
the transmitter.

`txAudioLeadMs` is the gateway telling the client how far ahead of the intended
playing time it wants the transmit audio. It is a request, not a promise.

### Neutral modes

| value | name | meaning |
|---|---|---|
| 0 | UNKNOWN | |
| 1 | USB | upper sideband, voice |
| 2 | LSB | lower sideband, voice |
| 3 | CW | |
| 4 | CWR | |
| 5 | AM | |
| 6 | FM | |
| 7 | DIGU | **the mode in which the USB codec feeds the modulator, on the upper sideband** |
| 8 | DIGL | the same, lower sideband |
| 9 | RTTY | the radio's own FSK |
| 10 | RTTYR | |
| 11 | PKTFM | |

`DIGU` is the important one and it is defined by what it does, not by its name
on any front panel. On a Yaesu it is DATA-USB, on an Icom USB-D, elsewhere PKT.
A client asking for `DIGU` is saying "put the sound card into the transmitter
with the speech processing out of the way" and the gateway is responsible for
knowing how that is spelled on the radio it found.

## Audio

**PCM, signed 16-bit, little-endian, mono, 48 000 Hz** by default — the rate and
channel count are declared in the context, so a gateway may publish another, but
this is what a client should expect. No codec: Opus and the automatic gain of a
voice codec destroy digital modes.

One packet carries **10 ms**, that is 480 samples, 960 bytes of payload and 988
bytes on the wire. That fits inside any sane MTU with room to spare.

### Receive

The gateway sends AUDIO_RX to every registered client as the samples arrive. The
timestamp is when the first sample of the packet was captured.

### Transmit — the part that matters

The timestamp on AUDIO_TX is not when the packet was sent. It is **when the
first sample must reach the modulator**. The gateway holds the audio and plays
it out at that instant.

This is the whole reason for having timestamps at all. A modem transmitting FT8
must start within a few tens of milliseconds of the slot boundary, and FT2 is
tighter still; if the gateway simply played whatever arrived whenever it
arrived, network jitter would be added directly to the slot alignment. Sending
early with a stated playing time moves the jitter into a buffer where it is
harmless.

The client should send `txAudioLeadMs` ahead of time. A packet that arrives
after its playing time is dropped and counted, not played late.

Keying follows the same rule: a COMMAND that sets `ptt` carries the instant the
transmitter should be keyed, and the gateway keys it then — not on arrival.

## Clocks

The gateway's audio codec and the client's modem do not share a clock, and over
minutes they drift. The timestamps make the drift measurable: a client comparing
its own idea of time with the arrival timestamps sees the rate error and can
resample. DecoPort states the problem and gives the numbers to fix it; it does
not fix it for you.

Both ends should be synchronised to NTP. Decodium already checks this.

## Discovery

The gateway broadcasts ANNOUNCE on port 5560 every two seconds, carrying its
`streamId`, `rigLabel`, `sessionPort` and `stateFlags`. A client listens, shows
what it found, and connects with HELLO to the session port on the address the
announcement came from. No configuration, no address to type.

Where broadcast does not reach — across subnets, or over the internet — a client
may be pointed at an address by hand. The session protocol is identical.

## What DecoPort deliberately does not do

It does not carry raw serial bytes. A tunnel would be transparent and would
solve nothing: the client would still need the radio's dialect, and the
compatibility problem would simply have moved. The translation happens once, at
the gateway, and the wire stays neutral.

It does not describe an SDR. There is no IQ, no spectrum, no receiver bandwidth
beyond the audio passband. This is a bridge for radios that hand you audio.
