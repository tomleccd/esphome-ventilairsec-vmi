// Ventilairsec VMI Pulse'R — radio frame helpers for the ESPHome firmware.
// See PROTOCOL.md for how the frames are built.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

namespace vmi {

// One 4-bit value is carried by one bit position across the 4 data bytes of a block.
inline int nib(const uint8_t *p, int pos) {
  const int a = (p[0] >> pos) & 1, b = (p[1] >> pos) & 1, c = (p[2] >> pos) & 1, d = (p[3] >> pos) & 1;
  return c | ((b ^ c ^ d) << 1) | ((a ^ c ^ d) << 2) | ((a ^ b ^ d) << 3);
}

// 4 data bytes of a block -> 4 decoded bytes (bit positions 0-1 -> byte 0, 2-3 -> byte 1, ...).
inline void decode(const uint8_t *p, uint8_t *out) {
  for (int k = 0; k < 4; k++) out[k] = (uint8_t) ((nib(p, 2 * k) << 4) | nib(p, 2 * k + 1));
}

// Inverse of decode(): 4 decoded bytes -> 4 data bytes.
inline void encode(const uint8_t *v, uint8_t *r) {
  r[0] = r[1] = r[2] = r[3] = 0;
  for (int k = 0; k < 4; k++) {
    for (int h = 0; h < 2; h++) {
      const int p = 2 * k + h;
      const int n = h == 0 ? (v[k] >> 4) : (v[k] & 15);
      const int c = n & 1, a = ((n >> 3) ^ (n >> 1) ^ c) & 1;
      const int d = (a ^ (n >> 2) ^ c) & 1, b = ((n >> 1) ^ c ^ d) & 1;
      r[0] |= a << p; r[1] |= b << p; r[2] |= c << p; r[3] |= d << p;
    }
  }
}

// Byte-wise Hamming(7,4) parity of the block starting at offset o.
inline bool block_ok(const std::vector<uint8_t> &x, int o) {
  return x.size() >= (size_t) (o + 7) && x[o + 4] == (uint8_t) (x[o + 1] ^ x[o + 2] ^ x[o + 3]) &&
         x[o + 5] == (uint8_t) (x[o + 0] ^ x[o + 2] ^ x[o + 3]) &&
         x[o + 6] == (uint8_t) (x[o + 0] ^ x[o + 1] ^ x[o + 3]);
}

// Data bytes (4 per block) -> packet to send: blocks with parity, padded with 0xAA to 16 bytes.
inline std::vector<uint8_t> build(const uint8_t *data, int blocks) {
  std::vector<uint8_t> out;
  for (int b = 0; b < blocks; b++) {
    const uint8_t *d = data + 4 * b;
    out.push_back(d[0]);
    out.push_back(d[1]);
    out.push_back(d[2]);
    out.push_back(d[3]);
    out.push_back(d[1] ^ d[2] ^ d[3]);
    out.push_back(d[0] ^ d[2] ^ d[3]);
    out.push_back(d[0] ^ d[1] ^ d[3]);
  }
  while (out.size() < 16) out.push_back(0xAA);
  return out;
}

inline uint32_t pack4(const uint8_t *p) {
  return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | p[3];
}
inline void unpack4(uint32_t v, uint8_t *p) {
  for (int i = 0; i < 4; i++) p[i] = (uint8_t) (v >> (24 - 8 * i));
}
// Long frames are stored as block 1 data then block 2 data.
inline uint64_t pack8(const uint8_t *b1, const uint8_t *b2) {
  return ((uint64_t) pack4(b1) << 32) | pack4(b2);
}
inline void unpack8(uint64_t v, uint8_t *p) {
  unpack4((uint32_t) (v >> 32), p);
  unpack4((uint32_t) v, p + 4);
}
inline void xor8(uint8_t *f, const uint8_t *m) {
  for (int i = 0; i < 8; i++) f[i] ^= m[i];
}

// ---------------------------------------------------------------------------------------------
// What a change of one field flips in the raw data bytes (block 1 then block 2). These masks
// include the change of the checksum byte, so they can be applied to any valid frame of the same
// kind. They were measured on one installation; see PROTOCOL.md.
// ---------------------------------------------------------------------------------------------

// Speed/heating command: each bit of the speed number (block 1 only).
static const uint8_t SPEED[4][4] = {
    {0x05, 0x07, 0x04, 0x03}, {0x06, 0x02, 0x00, 0x06}, {0x01, 0x05, 0x01, 0x04}, {0x05, 0x04, 0x00, 0x05}};
// Heating level relative to "off" (0 off, 1 = 12 C, 2 = 15 C, 3 = 18 C).
static const uint8_t HEAT1[4][4] = {
    {0x00, 0x00, 0x00, 0x00}, {0x00, 0x01, 0x00, 0x03}, {0x02, 0x01, 0x03, 0x01}, {0x02, 0x03, 0x01, 0x03}};
static const uint8_t HEAT2[4][4] = {
    {0x00, 0x00, 0x00, 0x00}, {0x40, 0x00, 0x00, 0x40}, {0x40, 0x00, 0x40, 0x40}, {0x40, 0x40, 0x00, 0x40}};
// Manual <-> automatic.
static const uint8_t AUTO1[4] = {0x01, 0x02, 0x02, 0x02};
static const uint8_t AUTO2[4] = {0x80, 0x80, 0x00, 0x80};

// Mode command: each mode relative to "all off".
static const uint8_t M_BOOST[8] = {0x03, 0x03, 0x00, 0x02, 0x80, 0x00, 0x00, 0x80};
static const uint8_t M_VACANCES[8] = {0x01, 0x02, 0x01, 0x02, 0x80, 0x80, 0x80, 0x00};
static const uint8_t M_SURVENT[8] = {0x00, 0x01, 0x00, 0x03, 0x40, 0x00, 0x00, 0x40};
static const uint8_t M_TURBO[8] = {0x02, 0x00, 0x01, 0x01, 0x00, 0x40, 0x00, 0x40};
// Decoded flags of the mode command (last decoded byte of block 2).
static const uint8_t F_VACANCES = 0x01, F_BOOST = 0x02, F_SURVENT = 0x20, F_TURBO = 0x40;

// Maximum temperature command: bits 0..3 of the value, and 0x1n <-> 0x2n.
static const uint8_t TMAX_LO[4][8] = {{0x03, 0x02, 0x02, 0x01, 0x80, 0x80, 0x80, 0x00},
                                      {0x02, 0x01, 0x00, 0x03, 0x80, 0x00, 0x00, 0x80},
                                      {0x01, 0x03, 0x00, 0x03, 0x00, 0x80, 0x00, 0x80},
                                      {0x02, 0x02, 0x00, 0x02, 0x80, 0x80, 0x00, 0x80}};
static const uint8_t TMAX_HI[8] = {0x01, 0x02, 0x03, 0x03, 0x00, 0x40, 0x40, 0x40};

// Moves a max-temperature command between 0x20 (32 C) and value v (20..45), in both directions.
inline void tmax_flip(uint8_t *f, int v) {
  if ((v >> 4) == 1) xor8(f, TMAX_HI);
  for (int b = 0; b < 4; b++)
    if (v & (1 << b)) xor8(f, TMAX_LO[b]);
}

// ---------------------------------------------------------------------------------------------
// Room temperature and humidity in the speed command (remote -> VMI).
// The checksum (decoded byte 0 of block 1) is XOR-linear within a frame family: each changed bit
// of the decoded data flips fixed bits of the decoded checksum. Table for the speed command,
// worked out from 30+ frames of the remote (7.5 to 34 C, 33 to 97 %).
// ---------------------------------------------------------------------------------------------
static const uint8_t CK_TEMP[6] = {0x21, 0x42, 0x84, 0x08, 0x31, 0x62};  // temperature bits 0-5
static const uint8_t CK_TEMP_HI = 0x4C;                                  // bits 6-7: 01 <-> 10
static const uint8_t CK_HUM[6] = {0x31, 0x62, 0xC4, 0xA9, 0x73, 0xE6};   // humidity bits 0-5

// Humidity level carried in bits 0-2 of the flags byte (as seen: 33-37 % -> 1, 43-59 % -> 2,
// 60-69 % -> 3, 79 % and more -> 4).
inline int hum_level(int h) { return h < 40 ? 1 : (h < 60 ? 2 : (h < 75 ? 3 : 4)); }

// Checksum offset of (humidity bit 6, level), for the combinations seen; -1 = unknown.
inline int ck_group(int h6, int lvl) {
  if (h6 == 0 && lvl == 1) return 0xDC;
  if (h6 == 0 && lvl == 2) return 0x00;
  if (h6 == 0 && lvl == 3) return 0xB4;
  if (h6 == 1 && lvl == 3) return 0x78;
  if (h6 == 1 && lvl == 4) return 0x55;
  return -1;
}

// Puts temperature t (C) and humidity h (%) into a speed command (raw data bytes, block 1 then
// block 2). Returns false, frame unchanged, when the combination is outside the known table.
inline bool set_room(uint8_t *f, float t, float h) {
  uint8_t v1[4], v2[4];
  decode(f, v1);
  decode(f + 4, v2);
  int T = (int) lroundf((t + 40.0f) * 2.0f);
  if (T < 0x40) T = 0x40;
  if (T > 0xBF) T = 0xBF;
  int H = (int) lroundf(h);
  if (H < 1) H = 1;
  if (H > 100) H = 100;
  const int lvl = hum_level(H);
  const int T0 = v2[0], H0 = v2[1], L0 = v2[3] & 7;
  const int g0 = ck_group((H0 >> 6) & 1, L0), g1 = ck_group((H >> 6) & 1, lvl);
  const int t0 = (T0 >> 6) & 3;
  if (g0 < 0 || g1 < 0 || !(t0 == 1 || t0 == 2) || H0 >= 0x80) return false;
  int ck = 0;
  for (int i = 0; i < 6; i++) {
    if (((T0 ^ T) >> i) & 1) ck ^= CK_TEMP[i];
    if (((H0 ^ H) >> i) & 1) ck ^= CK_HUM[i];
  }
  if (t0 != ((T >> 6) & 3)) ck ^= CK_TEMP_HI;
  ck ^= g0 ^ g1;
  v1[0] ^= ck;
  v2[0] = T;
  v2[1] = H;
  v2[3] = (v2[3] & 0xF8) | lvl;
  encode(v1, f);
  encode(v2, f + 4);
  return true;
}

}  // namespace vmi
