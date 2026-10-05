# FEC-RTTY protocol specification

## Project preamble

FEC-RTTY began as an experiment to build a digital mode in 24 hours, with a
FEC text modem, usable GUI, and actual-audio demonstration. The challenge is
the project's origin, not a certification or measured completion claim.
See the [complete project guide](PROJECT_GUIDE.md) for operation, architecture,
history, testing, and current limitations. All formats described here are
deliberately unencrypted; error correction and encoding are not encryption.

## 1. Scope and status

Application version 0.46.0 and wire-frame version 0 are different identifiers.
This is the current implementation's technical specification, not an operator
setup guide. Use [Building](BUILDING.md) and [GUI operation](GUI_PACKAGE.md)
to run the program; legacy addressed formats are explicitly separated below.

This document describes the wire protocol implemented by the current FEC-RTTY
modem source. It is intended to let another implementation generate and decode
compatible modem audio without depending on the C++ classes in this project.

The protocol is currently an experimental v0 protocol. The frame version field
is `0`. The active GUI path uses unaddressed `Text` frames: application bytes
are sent as plaintext and decoded as they arrive, with no link establishment,
HELLO exchange, ACK wait, retry loop, or encryption. There is deliberately no
authentication, key exchange, or encryption. The GUI exposes one fixed modem
profile only.

Unaddressed text frames are the normal GUI and dependency-free bench path.
The addressed ARQ envelope described later remains an optional legacy/core
compatibility format; the current GUI does not generate or require it.

## 2. Protocol at a glance

| Property | Current value |
|---|---|
| Sample rate | 48,000 samples/s |
| Audio format | Mono; WinMM uses signed 16-bit PCM, PortAudio uses float32; internal DSP uses `float` |
| Symbol rate | 50 symbols/s |
| Samples per symbol | 960 |
| Modulation | 4-FSK, two coded bits per symbol |
| Tones | 1,425 / 1,475 / 1,525 / 1,575 Hz |
| Tone spacing | 50 Hz |
| Acquisition pattern | 25 known symbols |
| Frame sync | 32-bit `0xD391C5A7`, sent MSB first |
| FEC | Rate-1/2, constraint length 7 convolutional code |
| FEC polynomials | Octal `171` and `133` |
| FEC termination | Six zero input bits |
| Interleaver | Row/column block interleaver with 16 columns |
| Integrity check | CRC-16/CCITT-FALSE, initial value `0xFFFF`, polynomial `0x1021` |
| Maximum payload | 8 bytes per frame |
| Text encoding | UTF-8 bytes, chunked at 8 bytes |
| Sequence number | 4 bits, modulo 16 |

The complete transmit chain is:

```text
UTF-8 text
    ↓ split into 8-byte payloads
frame header + payload + CRC
    ↓ MSB-first bit conversion
rate-1/2 convolutional encoder with six tail bits
    ↓ 16-column interleaver
32-bit sync word + interleaved coded bits
    ↓ two bits per symbol
Gray-mapped 4-FSK at 50 symbols/s
    ↓ 48 kHz audio
mono modem waveform
```

The receive chain performs the inverse operations, with frequency acquisition,
soft demodulation, sync validation, Viterbi decoding, length selection, and CRC
validation before delivering payload bytes.

## 3. Audio and physical layer

The modem uses a fixed 48 kHz sample rate and a 50 baud symbol rate:

```text
samples_per_symbol = 48,000 / 50 = 960
symbol_period      = 1 / 50 = 20 ms
```

Each symbol is represented by 960 samples of a real sinusoid. The modulator
keeps an oscillator phase accumulator while generating the symbols in one
modulation run. The current implementation starts a fresh phase accumulator
for the acquisition waveform and for each frame's modulation call, so phase is
continuous within each generated segment but is not a separate protocol field
or a guaranteed phase-continuous boundary between all segments.

The four tones and Gray mapping are:

| Coded bit pair | Tone index | Frequency |
|---|---:|---:|
| `00` | 0 | 1,425 Hz |
| `01` | 1 | 1,475 Hz |
| `11` | 2 | 1,525 Hz |
| `10` | 3 | 1,575 Hz |

The Gray ordering means adjacent tones differ by one coded bit. An
implementation must preserve this mapping exactly; ordinary binary ordering
would not be interoperable.

The GUI center-frequency setting shifts all four tones equally, preserving
their 50 Hz separation and Gray mapping. The table gives the default 1500 Hz
center. A receiver's RX center must match the sender's TX center within the
acquisition search range. TX tones are frozen for each send. RX can retune
while listening; doing so resets acquisition/decoder counters and waits for
a new complete burst. Linked RX/TX tuning is locked during a send.

The WinMM and PortAudio adapters are transport layers around this waveform.
They open mono 48 kHz audio and convert between device PCM and internal
floating-point samples. The audio backend, VB-Audio device IDs, output gain,
and radio CAT/PTT settings are not part of the over-the-air protocol.

## 4. Burst layout

A normal transmission is one acquisition waveform followed by one or more
frames:

```text
┌──────────────────────┬─────────────┬─────────────┬─────────────┐
│ 25-symbol acquisition│ frame 0     │ frame 1     │ frame 2 ...  │
│ pattern              │ sync + data │ sync + data │ sync + data  │
└──────────────────────┴─────────────┴─────────────┴─────────────┘
```

The acquisition pattern is the tone-index sequence:

```text
0, 1, 2, 3, 0, 2, 1, 3, 1, 0, 3, 2, 2, 3, 0, 1, 3, 1, 2, 0, 0, 3, 1, 2, 3
```

It occupies `25 × 960 = 24,000` samples, or 500 ms. It is generated with the
nominal tone frequencies and is used by the receiver to locate the burst and
estimate frequency offset.

Each frame begins immediately after the acquisition waveform or the preceding
frame. The frame's 32-bit sync word is inside the frame waveform and is
followed by the FEC-coded frame bytes. There is no separate end-of-message
marker. The receiver determines the frame length from the protected length
field and recognizes the end of a burst through the lack of further valid
frames and/or silence.

For a message longer than 8 bytes, the transmitter emits multiple frames with
one acquisition waveform at the start of the complete transmission. It does
not insert a new acquisition waveform between ordinary consecutive frames.

## 5. Frame format before FEC

The raw frame is between 4 and 12 bytes long:

```text
header (2 bytes) + payload (0..8 bytes) + CRC (2 bytes)
```

### Byte 0: version, type, and flags

```text
bit:  7       6 5       4 3                   0
      +---------+---------+---------------------+
      | version |  type   |       flags         |
      +---------+---------+---------------------+
```

- Bits 7–6: 2-bit protocol version. Current value: `0`.
- Bits 5–4: 2-bit frame type.
- Bit 0: addressed ARQ envelope is present.
- Bit 1: this is the final chunk or final control response.
- Bits 3–2: reserved and transmitted as zero.

The defined frame-type values are:

| Value | Name | Current status |
|---:|---|---|
| 0 | `Text` | Unaddressed text or addressed ARQ data |
| 1 | `Control` | Addressed ARQ ACK/NAK/BUSY control |
| 2 | `Id` | Reserved for future station identification |
| 3 | `Reserved` | Must not be used by current implementations |

### Byte 1: sequence and payload length

```text
bit:  7            4 3              0
      +--------------+----------------+
      |   sequence   | payload length |
      +--------------+----------------+
```

- Bits 7–4: 4-bit sequence number, modulo 16.
- Bits 3–0: payload length in bytes, from 0 through 8.

The sequence number increments once per transmitted frame. It is used for
diagnostics: the receiver reports a sequence gap when a valid frame is not the
next expected value. There is currently no retransmission associated with a
gap.

### Payload

The payload is an arbitrary byte string of length 0–8. The application treats
text payloads as UTF-8. The unaddressed modem path chunks a `std::string_view`
by bytes, not by Unicode code points; a multi-byte UTF-8 character may
therefore cross a frame boundary, but the byte stream is reassembled in its
original order.

The direct text API and GUI display only unaddressed version-0 `Text` frames
with zero flags. Valid legacy/control/future-version frames remain available
through `take_frames()` for inspection but their raw payloads are not treated
as operator text. The GUI retains incomplete UTF-8 between frames/timer ticks
and hides binary control characters while preserving tabs and line breaks.

### Addressed ARQ envelope

An addressed `Text` frame sets flag bit 0 and uses the following five-byte
payload envelope. The remaining zero to three bytes are application data:

```text
destination | source | transfer_id | chunk_index | total_chunks | data...
```

`destination` and `source` are one-byte station IDs. `0xff` is the wildcard /
broadcast ID. A receiver configured with peer ID `0xff` accepts a transfer from
any source, while a transmitted destination of `0xff` can be received by a
station configured to accept that destination. `transfer_id` identifies a
message segment, `chunk_index` starts at zero, and `total_chunks` is one through
255. One segment carries at most 765 data bytes. Longer plaintext messages are
split into up to 255 sequential segments, with the transfer ID advanced for
each segment, for a maximum of 195,075 bytes in one `ArqSender` operation.
The final flag is set on the last chunk of each segment; the receiver delivers
the segment bytes in order to the application, so a legacy session application
can reassemble the complete message. The current direct GUI does not use this
envelope.

An addressed `Control` frame always has a five-byte payload:

```text
code | destination | source | transfer_id | chunk_index
```

The implemented control codes are `1=ACK`, `2=NAK`, `3=BUSY`, `4=HELLO`, and
`5=DONE`. ACK repeats the acknowledged chunk, NAK requests the indicated
chunk, and BUSY tells a sender to defer while the receiver is holding another
transfer. HELLO encodes protocol version in the `transfer_id` field and the
capability bitmap in `chunk_index`; the current bitmap explicitly includes
plaintext, FEC, interleaving, ARQ, and wildcard-address support. DONE is
reserved as an explicit completion acknowledgement for future session control.
HELLO is informational and optional; it does not add encryption or
authentication and it does not prevent a peer from transmitting.

The optional legacy ARQ receiver accepts data only when the destination matches
its configured station ID (or an explicitly accepted broadcast) and the source
matches its configured peer. It appends the expected chunk and ACKs it. A
repeated earlier chunk is not appended a second time but is ACKed again, which
recovers from a lost ACK. A future chunk produces a NAK for the expected index.
The legacy sender allows four retries after the first attempt and applies a
deterministic exponential slot backoff, capped at 32 slots of 250 ms. The
current GUI does not use this session layer or wait for carrier sense; it sends
direct FEC frames.

### CRC

The CRC is two bytes, most significant byte first, calculated over byte 0,
byte 1, and every payload byte. It uses CRC-16/CCITT-FALSE:

```text
width       = 16
polynomial  = 0x1021
initial     = 0xFFFF
input       = not reflected
output      = not reflected
final xor   = 0x0000
```

A frame is not delivered as valid unless the decoded header length, decoded
payload length, and CRC all agree.

### Golden raw-frame example

For a text frame with version 0, type `Text`, flags 0, sequence 3, and payload
`AB`:

```text
version/type/flags  = 00
sequence/length    = 32
payload             = 41 42
CRC-16              = 84 C0

complete raw frame  = 00 32 41 42 84 C0
```

The raw bytes are converted to bits MSB first before FEC encoding. The sync
word is not included in this CRC calculation.

## 6. FEC and interleaving

### Bit conversion

Every raw frame byte is emitted most significant bit first. For example,
`0x32` becomes:

```text
00110010
```

The raw frame contains `8 × (payload_length + 4)` input bits.

### Convolutional code

The encoder is a rate-1/2, constraint-length-7 convolutional code:

- initial shift-register state: zero;
- generator polynomial 0: octal `171`;
- generator polynomial 1: octal `133`;
- two parity bits emitted for every input bit;
- six zero input bits appended to terminate the trellis at the zero state.

If `R` is the number of raw frame bits, the encoded length is:

```text
C = 2 × (R + 6)
```

The receiver uses soft Viterbi decoding. Each demodulated coded bit is carried
as a signed value in approximately `-127..+127`; the Viterbi metric compares
that value with the expected hard bit level rather than discarding confidence
information immediately. The six termination bits are removed from the
decoded output before converting the bits back to bytes.

### Interleaver

The coded bits are interleaved with 16 columns. The input is placed row by row
in a matrix with 16 columns, and the output is read column by column:

```text
input order:  b[0], b[1], b[2], ... across rows
output order: column 0 top-to-bottom, column 1 top-to-bottom, ...
```

The final row may be incomplete. The receiver applies the exact inverse
permutation to the soft values before Viterbi decoding. The interleaver spreads
short adjacent bursts of audio corruption across the decoder's input rather
than leaving them concentrated in one contiguous coded-bit region.

## 7. Sync word and frame waveform

The 32-bit sync word is:

```text
0xD391C5A7
```

It is emitted MSB first. The frame bit stream sent to the 4-FSK modulator is:

```text
sync_bits || interleave(convolutional_encode(raw_frame_bits))
```

The sync word is not convolutionally encoded and is not covered by the CRC.
It is a fast frame-boundary and false-lock filter. The current decoder accepts
a sync candidate when its hard-decision Hamming distance is at most four bits.
The FEC and CRC checks still have to pass before the payload is delivered.

For payload length `n`, define:

```text
R       = 8 × (n + 4)       raw frame bits
C       = 2 × (R + 6)       convolutionally coded bits
T       = 32 + C             sync plus coded bits
symbols = T / 2
samples = symbols × 960
```

The values for common payload sizes are:

| Payload | Raw frame | Coded bits | Frame symbols | Frame time | Including 500 ms acquisition |
|---:|---:|---:|---:|---:|---:|
| 0 bytes | 4 bytes | 76 | 54 | 1.08 s | 1.58 s |
| 5 bytes (`HELLO`) | 9 bytes | 156 | 94 | 1.88 s | 2.38 s |
| 8 bytes | 12 bytes | 204 | 118 | 2.36 s | 2.86 s |

These times exclude any external audio-device startup delay or inter-message
pause.

## 8. Receiver operation

The reliable live receiver accepts arbitrary-sized audio chunks. It keeps a
bounded sample buffer and runs the following process:

1. **Search** — retain enough samples to look for the 25-symbol acquisition
   pattern; quiet input is not repeatedly sent through the expensive tone
   search.
2. **Acquire** — compare the known tone pattern against the input while
   searching frequency offsets from -100 Hz to +100 Hz in 5 Hz steps. The
   live search tests half-symbol-spaced positions first, then refines detected
   candidates on a 1/16-symbol grid. The coarse search uses every fourth
   sample when all searched tones are below the resulting Nyquist limit;
   boundary refinement and frame demodulation use full-rate audio.
   acquisition score is the wanted-tone energy divided by total tone energy;
   the current lock threshold is greater than 0.70. At least 20 of the 25
   symbol intervals must contain non-silent audio; this rejects a matching
   short tail followed by silence rather than mistaking it for a full field.
   The streaming receiver commits a lock only after the following complete
   sync word is available and validates within four bit errors. A candidate
   awaiting sync is retained, not used as a premature symbol grid.
   Failed searches retain 42 symbols: the 25-symbol preamble, 16-symbol sync,
   and one symbol of search overlap. Large callers are internally consumed
   in at most two-symbol blocks. A relative search cursor visits each newly
   eligible half-symbol position, rather than restricting search to the first
   above-threshold noise sample or repeatedly rescanning old background noise.
3. **Boundary refinement** — when possible, inspect the following sync word to
   refine the sample boundary around the coarse acquisition result using the
   tones corrected by the acquired frequency offset. Among positions with
   equal sync distance, prefer the highest full-rate acquisition score.
4. **Locked** — discard the acquisition samples, adjust the four demodulator
   tones by the measured frequency offset, and wait for a complete frame.
5. **Frame** — demodulate the available candidate waveform, try the legal
   payload lengths, Viterbi-decode each candidate, and accept only a candidate
   whose header length and CRC validate.
6. **Track** — append compatible direct text, update frame/CRC/sequence counters, and
   continue at the next frame boundary without reacquiring.
7. **Lost/Reacquire** — after a failed or interrupted stream, look for a new
   acquisition pattern, with or without a quiet gap. A new validated preamble
   resets sequence continuity for that burst and increments the acquisition
   and, when appropriate, reacquisition counters. Recovery within a burst by
   a later frame's sync retains sequence tracking to expose missing frames.

The acquisition energy calculation uses Goertzel tone-energy measurements
to reduce CPU cost. These search optimizations do not change the waveform,
frequency search grid, score definition, or lock threshold. A damaged final
frame with valid sync reports a CRC failure once. Recovery also tries a fresh
acquisition at legal frame ends when a new burst follows without a quiet gap.

The legal payload-length search is important because the length field is inside
the protected frame. The decoder first determines the largest frame that fits
in the samples available, demodulates that common waveform once, then tries
lengths from 8 down to 0 using the corresponding soft-bit prefix. Sync,
Viterbi, decoded length, and CRC together select the valid frame.

At an explicit end-of-stream flush, a candidate missing no more than
1/16 symbol (60 samples at the fixed profile) may be zero-padded for decoding.
This handles a slightly late acquisition boundary under noise or PCM
quantization. Its decoded header and CRC must still validate; larger missing
tails are not filled, and no protocol bytes are added.

The live receiver exposes these diagnostics:

| Metric | Meaning |
|---|---|
| Valid frames | Frames that passed decoding and CRC |
| CRC failures | Candidate frames that could not pass integrity validation |
| Sequence gaps | Valid frame sequence was not the expected modulo-16 value within one continuous burst |
| Acquisitions | Successful locks on an acquisition pattern |
| Reacquisitions | Locks after the first acquisition or a lost stream |
| Frequency offset | Measured offset applied to the current demodulator tones |
| Samples buffered | Audio retained while waiting for enough data |
| Dropped samples | Audio chunks discarded by the backend queue |
| Capture interruptions | PortAudio-reported input overflow/underflow events; missing sample counts are not available, and WinMM exposes no equivalent event count |
| Receive state | Current Search/Acquire/Locked/Frame/Track/Lost/Reacquire state |

## 9. Audio-level and application behavior

The modem protocol operates on normalized floating-point samples. The GUI
applies modem gain and then its final output-volume setting before sending the
waveform to the selected audio backend:

```text
sample_to_device = clamp(modem_sample × modem_gain × output_volume, -1, +1)
```

WinMM converts the result to signed 16-bit PCM; PortAudio writes float32
samples and leaves conversion to the host audio driver. Output
volume changes the amplitude only; it does not change symbol timing, tone
frequency, frame format, FEC, or CRC.

The GUI generates one frame at a time and supplies short sample blocks to one
continuous output device session. WinMM uses a bounded ring of PCM buffers;
PortAudio writes short blocks to the same stream. Volume/gain is read as each
new block is queued, and Stop cancels queued playback. This retains the same
sample sequence as the original whole-burst modulator without allocating the
entire message's audio in advance.

WinMM prefills its output ring before starting playback. The GUI adds 100 ms
of silence after the modem burst to drain a final partial device buffer. That
quiet tail contains no application or protocol bytes.

The GUI's CAT/PTT setting and `NullRig` bench mode are outside the modem
protocol. The current bench acceptance path is:

```text
station A TX: CABLE Input  →  VB-Audio cable  →  CABLE Output: station B RX
```

Radio control is intentionally disabled during these tests.

The waterfall is an independent view of captured RX audio: a 4096-point Hann
FFT with 2048-sample hop, 11.71875 Hz bins, 0–3500 Hz display, and 240 rows of
history. Teal and orange markers indicate the configured RX and TX four-tone
centers. Linked offsets move together; split offsets configure RX and TX
separately. Live RX retuning resets acquisition and its counters; TX uses a
fixed tone configuration for the whole send. The 0.1 Hz numeric control
precision does not imply finer FFT resolution or guaranteed clock accuracy.
Display analysis, palette, and sensitivity do not change FEC, framing, input
gain, output volume, encryption behavior, or the over-the-air mode.

## 10. Interoperability checklist

An independent implementation must match all of the following:

1. 48,000 Hz sample rate and 50 symbols/s.
2. 960 samples per symbol.
3. Tone frequencies 1,425/1,475/1,525/1,575 Hz.
4. Gray mapping `00, 01, 11, 10` to increasing tone index.
5. The exact 25-symbol acquisition pattern.
6. Sync word `0xD391C5A7`, MSB first.
7. Raw frame bit layout and 0–8 byte payload limit.
8. CRC-16/CCITT-FALSE over the two header bytes and payload, sent big-endian.
9. Rate-1/2 K=7 FEC with octal generators `171` and `133`, six zero tail bits.
10. 16-column row-to-column interleaver.
11. MSB-first byte/bit conversion.
12. Four-FSK symbol grouping from the coded bit stream.
13. For addressed ARQ, the five-byte envelope and control payload described in
    Section 5, plus the configured station/peer IDs; peer ID `0xff` is the
    wildcard source selector.
14. For legacy addressed plaintext long messages, segment boundaries advance `transfer_id` and
    restart `chunk_index` at zero; reassemble segment payloads in order.
15. Legacy HELLO, when used, carries protocol version and the plaintext capability
    bitmap in the control fields. It is informational, not authentication.

Items 13–15 apply only to the optional legacy addressed session format, not
to current direct GUI text interoperability. HELLO is an optional legacy
on-air capability advertisement, but it is not a required
negotiation step. A receiver must still know the waveform settings in advance;
the advertisement does not provide identity proof, privacy, or policy control.

## 11. Current limitations and future protocol work

- The sequence field is only four bits and is diagnostic; it does not provide
  reliable delivery or retransmission. The receiver deliberately starts a new
  sequence check after every validated new-burst preamble, with or without
  a quiet gap, because a separate transmission may legitimately restart at
  sequence zero. Gaps inside one continuous burst remain reported.
- Callsign exchange, authentication, encryption, and key management are not
  implemented by design. HELLO carries only a plaintext protocol/capability
  bitmap; it is not an identity claim and must not be treated as one.
- The direct GUI path has no delivery acknowledgement or automatic
  retransmission. Simultaneous transmissions can collide; hidden-terminal
  mitigation and a network scheduler are not implemented.
- The current receiver has frequency acquisition and reacquisition, but not a
  production closed-loop sample-clock/timing tracker. Clock-drift helpers in
  the source are test infrastructure; long-duration timing tolerance still
  needs a dedicated on-air validation gate.
- The direct GUI has no application semantics for non-Text frame types; legacy
  Control frame semantics are described in the optional ARQ section above.
- The GUI has an application-level energy wake-up optimization before it invokes
  the receiver. Its threshold is below the supported low-volume bench range,
  so reducing output volume to avoid overdriving a radio does not suppress a
  valid modem burst.
- UTF-8 is transported as bytes. The GUI incrementally decodes complete
  multi-byte sequences, preserving newlines and blank lines. Incomplete or
  corrupted received text cannot be reconstructed after a frame is lost.
- The optional legacy ARQ receiver holds one active transfer at a time. The
  direct GUI path has no ARQ message-size or transfer-ID limit beyond the
  available audio run and application memory.

The authoritative implementation pieces for this document are in
`source/include/fectty/frame.hpp`, `fsk4.hpp`, `convolutional.hpp`,
`interleaver.hpp`, `sync.hpp`, `acquisition.hpp`, `frame_receiver.hpp`, and
`reliable_receiver.hpp`, `arq.hpp`, and `interop.hpp`, with their corresponding
files under `source/src/`.
