# Ventilairsec VMI Pulse'R — radio protocol

Reverse-engineered in November 2026 by observing the radio traffic between a Ventilairsec
VMI Pulse'R (Cube) and its colour remote control (remote firmware VS V3.3 / VH V3.1, VMI
board VS V2.7 / VH V2.0). Everything below comes from that one installation; other units or
firmware versions may differ.

## Radio

| Parameter      | Value                                    |
|----------------|------------------------------------------|
| Frequency      | ~868.04 MHz                              |
| Modulation     | 2-FSK, ~100 kHz deviation                |
| Data rate      | 9600 baud                                |
| Sync word      | `2D D4`                                  |
| Trailer        | `AA`                                     |

The VMI never transmits on its own: it only answers requests from the remote. The remote
sends a request about once per second while its screen is on, and nothing while it sleeps.

## Framing

A frame is one block (short frame) or two blocks (long frame), followed by `AA`.

Each block is 7 bytes: 4 data bytes `D0..D3` and 3 parity bytes (byte-wise Hamming 7,4):

```
P0 = D1 ^ D2 ^ D3
P1 = D0 ^ D2 ^ D3
P2 = D0 ^ D1 ^ D3
```

## Decoding the data bytes

The 4 data bytes are interleaved. Take one bit position `p` (0..7) across the 4 data bytes,
`a = D0.p`, `b = D1.p`, `c = D2.p`, `d = D3.p`. They carry one 4-bit value:

```
n(p) = c | (b^c^d) << 1 | (a^c^d) << 2 | (a^b^d) << 3
```

Two neighbouring bit positions make one decoded byte: `byte k = n(2k) << 4 | n(2k+1)`.
Each block therefore carries 4 decoded bytes.

## Block 1 (all frames)

| Decoded byte | Meaning                                                          |
|--------------|------------------------------------------------------------------|
| 0            | Checksum — **algorithm unknown** (not a plain sum, XOR or CRC-8)  |
| 1            | High nibble: speed (1..10) or sub-type. Low nibble: frame type   |
| 2, 3         | Identifier of the remote/VMI pair (constant for an installation)  |

## Short frames (remote → VMI), decoded byte 1

| Byte 1 | Meaning                                         | VMI answer           |
|--------|-------------------------------------------------|----------------------|
| `06`   | Hello (sent when the remote wakes up)           | status               |
| `08`   | Request page boost / vacances                   | page type E          |
| `04`   | Request page filter                             | page type F          |
| `34`   | Request page manual / automatic                 | page type A          |
| `24`   | Request versions (installer menu)               | page type A, sub F   |
| `44`, `14` | Installer menu, counters                    | type 9, unknown      |
| `x0`   | Short speed command (speed x) — checksum unknown |                     |

## Long frames, decoded block 2

### VMI status (type D, speed in byte 1 high nibble)

| Byte | Meaning                                                     |
|------|-------------------------------------------------------------|
| 0    | Relative humidity inside the unit, % (not cross-checked)    |
| 1    | Temperature inside the unit, `(T + 40) * 2` °C              |
| 2    | Heating set-point in °C: 0 off, 12, 15, 18; 4 = frost guard |
| 3    | Flags: 01 / 02 seen, 80 during the installer self-test      |

### Page boost / vacances (type E)

| Byte | Meaning                                       |
|------|-----------------------------------------------|
| 0    | 0 none, 1 vacances, 2 boost                   |
| 1    | Heating power while heating (0..200, unit unknown) |
| 2, 3 | Operating time in days (big-endian)           |

### Page filter (type F)

| Byte | Meaning                                              |
|------|------------------------------------------------------|
| 0    | 00                                                   |
| 1    | Filter days in use                                   |
| 2    | Maximum temperature (stand-by threshold), °C         |
| 3    | 02, + 10 surventilation, + 20 turbochauffe           |

### Page manual / automatic (type A)

| Byte | Meaning                                                   |
|------|-----------------------------------------------------------|
| 0    | 00 manual, 09 automatic                                   |
| 1    | State code: 64 normal, 67 after a failed heater self-test |
| 2    | Unknown, moves with air temperature                       |
| 3    | 01                                                        |

Versions page (type A, sub-type F): `[VH] [VS] [nominal speed] [00]`, e.g. `20 27 01` = V2.0, V2.7, 1.

### Speed / heating command (remote → VMI, type 0, speed in byte 1)

| Byte | Meaning                                                        |
|------|----------------------------------------------------------------|
| 0    | Room temperature measured by the remote, `(T + 40) * 2` °C     |
| 1    | Room humidity measured by the remote, %                        |
| 2    | 00                                                             |
| 3    | High nibble heating 0 off, 2 = 12 °C, 5 = 15 °C, 8 = 18 °C; low nibble 2 manual, A automatic |

The remote sends it every 5 s while awake, and on every change. When it wakes up it sends its
own manual/automatic setting, which overrides one set by another transmitter.

### Mode command (remote → VMI, type 9, byte 1 = `09`)

Block 2 = `00 00 00 [flags]`, flags = 08 + 01 vacances + 02 boost + 20 surventilation + 40 turbochauffe.
Boost and vacances exclude each other. Boost stops by itself after 30 minutes.

### Maximum temperature command (remote → VMI, byte 1 = `03`)

Block 2 = `00 04 00 [°C]`, 20..45. The VMI answers with the filter page.

## Building frames without the checksum

The checksum is not understood, but on the observed frames changing one field always flips
the same bits of the raw data bytes, checksum included. The firmware therefore takes a valid
frame heard from the remote and applies these flip masks (raw data bytes, block 1 then block 2):

| Change                     | Mask                        |
|----------------------------|-----------------------------|
| Speed bit 0 / 1 / 2 / 3    | `05 07 04 03` / `06 02 00 06` / `01 05 01 04` / `05 04 00 05` (block 1) |
| Heating off → 12 °C        | `00 01 00 03 \| 40 00 00 40` |
| Heating off → 15 °C        | `02 01 03 01 \| 40 00 40 40` |
| Heating off → 18 °C        | `02 03 01 03 \| 40 40 00 40` |
| Manual ↔ automatic         | `01 02 02 02 \| 80 80 00 80` |
| Mode: boost                | `03 03 00 02 \| 80 00 00 80` |
| Mode: vacances             | `01 02 01 02 \| 80 80 80 00` |
| Mode: surventilation       | `00 01 00 03 \| 40 00 00 40` |
| Mode: turbochauffe         | `02 00 01 01 \| 00 40 00 40` |
| Max temp bit 0 / 1 / 2 / 3 | `03 02 02 01 \| 80 80 80 00` / `02 01 00 03 \| 80 00 00 80` / `01 03 00 03 \| 00 80 00 80` / `02 02 00 02 \| 80 80 00 80` |
| Max temp 0x1n ↔ 0x2n       | `01 02 03 03 \| 00 40 40 40` |

All the commands built this way were accepted by the VMI on the reference installation.
Whether the masks are the same for another identifier is not known yet.

## Open questions

- Checksum algorithm (would allow sending any frame, including the room temperature and
  humidity used by the automatic mode).
- Stand-by and "summer" (glasses) indicators.
- The meaning of the type 9 counter pages in the installer menu.
- Filter counter reset.
