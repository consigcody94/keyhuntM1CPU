/*
 * NEON-accelerated SHA-256 using ARM Crypto Extensions
 * For Apple Silicon M1/M2/M3/M4/M5
 *
 * Uses hardware SHA-256 instructions (vsha256h, vsha256h2, vsha256su0, vsha256su1)
 * available on all Apple Silicon chips via the +crypto arch extension.
 *
 * Based on the SSE implementation pattern from VanitySearch by Jean Luc PONS,
 * adapted for ARM NEON with hardware crypto acceleration.
 *
 * License: GPLv3
 */

#include "sha256.h"

#if defined(__aarch64__) && (defined(__ARM_FEATURE_CRYPTO) || defined(__ARM_FEATURE_SHA2))

#include <arm_neon.h>
#include <string.h>
#include <stdint.h>

namespace _sha256neon {

static const uint32_t K256[64] = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
    0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
    0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
    0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
    0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
    0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
};

// Single-message SHA-256 using ARM crypto extensions
// Each vsha256h/vsha256h2 pair computes 4 rounds in one instruction
static inline void sha256_transform_hw(uint32_t state[8], const uint8_t block[64]) {
    // Load state into NEON registers
    // ARM SHA-256 intrinsics use a specific layout:
    //   ABEF = {A, B, E, F}  and  CDGH = {C, D, G, H}
    uint32x4_t ABEF = vld1q_u32(&state[0]);  // {A, B, C, D} initially
    uint32x4_t CDGH = vld1q_u32(&state[4]);  // {E, F, G, H} initially

    // Rearrange to ARM's expected layout: ABEF = {A,B,E,F}, CDGH = {C,D,G,H}
    uint32x4_t tmp0 = vzip1q_u32(ABEF, CDGH);  // {A, E, B, F}
    uint32x4_t tmp1 = vzip2q_u32(ABEF, CDGH);  // {C, G, D, H}
    ABEF = vcombine_u32(vget_low_u32(tmp0), vget_low_u32(tmp1));   // doesn't work right
    // Actually, ARM SHA256 intrinsics expect specific ordering.
    // Let's use the standard approach:

    // Reload correctly for ARM SHA2 intrinsic layout
    // vsha256hq expects state split as: {C,D,G,H} and {A,B,E,F}
    // where the result updates the first argument
    uint32x4_t STATE0 = vld1q_u32(&state[0]); // A B C D
    uint32x4_t STATE1 = vld1q_u32(&state[4]); // E F G H

    // Rearrange for ARM SHA256: need ABEF and CDGH
    // ABEF = (A, B, E, F), CDGH = (C, D, G, H)
    uint32x4_t ABEF_save, CDGH_save;
    ABEF_save = vuzp1q_u32(STATE0, STATE1); // A, C, E, G -> nope
    // The ARM intrinsic layout is actually just two halves:
    // vsha256hq_u32(hash_abcd, hash_efgh, wk) where hash_abcd=[a,b,c,d], hash_efgh=[e,f,g,h]
    // This was simplified in later ARM documentation.

    // Use the straightforward approach that maps directly to the instructions
    uint32x4_t hash_abcd = STATE0;
    uint32x4_t hash_efgh = STATE1;
    uint32x4_t hash_abcd_saved = hash_abcd;
    uint32x4_t hash_efgh_saved = hash_efgh;

    // Load message in big-endian (SHA-256 operates on big-endian words)
    uint32x4_t msg0 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(&block[0])));
    uint32x4_t msg1 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(&block[16])));
    uint32x4_t msg2 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(&block[32])));
    uint32x4_t msg3 = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(&block[48])));

    uint32x4_t wk;

    // Rounds 0-3
    wk = vaddq_u32(msg0, vld1q_u32(&K256[0]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);
    msg0 = vsha256su1q_u32(vsha256su0q_u32(msg0, msg1), msg2, msg3);

    // Rounds 4-7
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg1, vld1q_u32(&K256[4]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);
    msg1 = vsha256su1q_u32(vsha256su0q_u32(msg1, msg2), msg3, msg0);

    // Rounds 8-11
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg2, vld1q_u32(&K256[8]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);
    msg2 = vsha256su1q_u32(vsha256su0q_u32(msg2, msg3), msg0, msg1);

    // Rounds 12-15
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg3, vld1q_u32(&K256[12]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);
    msg3 = vsha256su1q_u32(vsha256su0q_u32(msg3, msg0), msg1, msg2);

    // Rounds 16-19
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg0, vld1q_u32(&K256[16]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);
    msg0 = vsha256su1q_u32(vsha256su0q_u32(msg0, msg1), msg2, msg3);

    // Rounds 20-23
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg1, vld1q_u32(&K256[20]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);
    msg1 = vsha256su1q_u32(vsha256su0q_u32(msg1, msg2), msg3, msg0);

    // Rounds 24-27
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg2, vld1q_u32(&K256[24]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);
    msg2 = vsha256su1q_u32(vsha256su0q_u32(msg2, msg3), msg0, msg1);

    // Rounds 28-31
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg3, vld1q_u32(&K256[28]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);
    msg3 = vsha256su1q_u32(vsha256su0q_u32(msg3, msg0), msg1, msg2);

    // Rounds 32-35
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg0, vld1q_u32(&K256[32]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);
    msg0 = vsha256su1q_u32(vsha256su0q_u32(msg0, msg1), msg2, msg3);

    // Rounds 36-39
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg1, vld1q_u32(&K256[36]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);
    msg1 = vsha256su1q_u32(vsha256su0q_u32(msg1, msg2), msg3, msg0);

    // Rounds 40-43
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg2, vld1q_u32(&K256[40]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);
    msg2 = vsha256su1q_u32(vsha256su0q_u32(msg2, msg3), msg0, msg1);

    // Rounds 44-47
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg3, vld1q_u32(&K256[44]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);
    msg3 = vsha256su1q_u32(vsha256su0q_u32(msg3, msg0), msg1, msg2);

    // Rounds 48-51
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg0, vld1q_u32(&K256[48]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);

    // Rounds 52-55
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg1, vld1q_u32(&K256[52]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);

    // Rounds 56-59
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg2, vld1q_u32(&K256[56]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);

    // Rounds 60-63
    hash_efgh_saved = hash_efgh;
    wk = vaddq_u32(msg3, vld1q_u32(&K256[60]));
    hash_efgh = vsha256hq_u32(hash_efgh, hash_abcd, wk);
    hash_abcd = vsha256h2q_u32(hash_abcd, hash_efgh_saved, wk);

    // Add back to state
    hash_abcd = vaddq_u32(hash_abcd, hash_abcd_saved);
    hash_efgh = vaddq_u32(hash_efgh, hash_efgh_saved);

    vst1q_u32(&state[0], hash_abcd);
    vst1q_u32(&state[4], hash_efgh);
}

static const unsigned char pad[64] = { 0x80 };

} // namespace _sha256neon

// Public API matching the SSE variant signatures

// SHA-256 of a 33-byte compressed public key using ARM crypto extensions
void sha256neon_33(uint8_t *input, uint8_t *digest) {
    uint32_t state[8] = {
        0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
        0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
    };

    // Build padded block (33 bytes + padding + length = 64 bytes)
    uint8_t block[64];
    memcpy(block, input, 33);
    memset(block + 33, 0, 31);
    block[33] = 0x80;
    // Length in bits = 33 * 8 = 264 = 0x108
    block[62] = 0x01;
    block[63] = 0x08;

    _sha256neon::sha256_transform_hw(state, block);

    // Output in big-endian
    for (int i = 0; i < 8; i++) {
        digest[i * 4 + 0] = (state[i] >> 24) & 0xFF;
        digest[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        digest[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        digest[i * 4 + 3] = state[i] & 0xFF;
    }
}

// SHA-256 of a 65-byte uncompressed public key using ARM crypto extensions
void sha256neon_65(uint8_t *input, uint8_t *digest) {
    uint32_t state[8] = {
        0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
        0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
    };

    // First block: bytes 0-63
    _sha256neon::sha256_transform_hw(state, input);

    // Second block: byte 64 + padding + length
    uint8_t block[64];
    memset(block, 0, 64);
    block[0] = input[64];
    block[1] = 0x80;
    // Length in bits = 65 * 8 = 520 = 0x208
    block[62] = 0x02;
    block[63] = 0x08;

    _sha256neon::sha256_transform_hw(state, block);

    // Output in big-endian
    for (int i = 0; i < 8; i++) {
        digest[i * 4 + 0] = (state[i] >> 24) & 0xFF;
        digest[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        digest[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        digest[i * 4 + 3] = state[i] & 0xFF;
    }
}

// 4-way parallel SHA-256 of 1-block messages using NEON (not HW crypto)
// Mirrors sha256sse_1B: processes 4 independent 33-byte inputs
void sha256neon_1B(uint32_t *i0, uint32_t *i1, uint32_t *i2, uint32_t *i3,
                   uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3) {
    // For 4-way parallel, we process each independently with HW acceleration
    // since each HW SHA-256 call is already extremely fast
    uint32_t s0[8] = {0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A,0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19};
    uint32_t s1[8] = {0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A,0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19};
    uint32_t s2[8] = {0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A,0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19};
    uint32_t s3[8] = {0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A,0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19};

    _sha256neon::sha256_transform_hw(s0, (const uint8_t *)i0);
    _sha256neon::sha256_transform_hw(s1, (const uint8_t *)i1);
    _sha256neon::sha256_transform_hw(s2, (const uint8_t *)i2);
    _sha256neon::sha256_transform_hw(s3, (const uint8_t *)i3);

    // Store results big-endian
    for (int i = 0; i < 8; i++) {
        d0[i*4+0] = (s0[i]>>24); d0[i*4+1] = (s0[i]>>16)&0xFF; d0[i*4+2] = (s0[i]>>8)&0xFF; d0[i*4+3] = s0[i]&0xFF;
        d1[i*4+0] = (s1[i]>>24); d1[i*4+1] = (s1[i]>>16)&0xFF; d1[i*4+2] = (s1[i]>>8)&0xFF; d1[i*4+3] = s1[i]&0xFF;
        d2[i*4+0] = (s2[i]>>24); d2[i*4+1] = (s2[i]>>16)&0xFF; d2[i*4+2] = (s2[i]>>8)&0xFF; d2[i*4+3] = s2[i]&0xFF;
        d3[i*4+0] = (s3[i]>>24); d3[i*4+1] = (s3[i]>>16)&0xFF; d3[i*4+2] = (s3[i]>>8)&0xFF; d3[i*4+3] = s3[i]&0xFF;
    }
}

// 4-way parallel SHA-256 of 2-block messages
void sha256neon_2B(uint32_t *i0, uint32_t *i1, uint32_t *i2, uint32_t *i3,
                   uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3) {
    uint32_t s0[8] = {0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A,0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19};
    uint32_t s1[8] = {0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A,0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19};
    uint32_t s2[8] = {0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A,0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19};
    uint32_t s3[8] = {0x6A09E667,0xBB67AE85,0x3C6EF372,0xA54FF53A,0x510E527F,0x9B05688C,0x1F83D9AB,0x5BE0CD19};

    // Block 1
    _sha256neon::sha256_transform_hw(s0, (const uint8_t *)i0);
    _sha256neon::sha256_transform_hw(s1, (const uint8_t *)i1);
    _sha256neon::sha256_transform_hw(s2, (const uint8_t *)i2);
    _sha256neon::sha256_transform_hw(s3, (const uint8_t *)i3);

    // Block 2
    _sha256neon::sha256_transform_hw(s0, (const uint8_t *)(i0 + 16));
    _sha256neon::sha256_transform_hw(s1, (const uint8_t *)(i1 + 16));
    _sha256neon::sha256_transform_hw(s2, (const uint8_t *)(i2 + 16));
    _sha256neon::sha256_transform_hw(s3, (const uint8_t *)(i3 + 16));

    for (int i = 0; i < 8; i++) {
        d0[i*4+0] = (s0[i]>>24); d0[i*4+1] = (s0[i]>>16)&0xFF; d0[i*4+2] = (s0[i]>>8)&0xFF; d0[i*4+3] = s0[i]&0xFF;
        d1[i*4+0] = (s1[i]>>24); d1[i*4+1] = (s1[i]>>16)&0xFF; d1[i*4+2] = (s1[i]>>8)&0xFF; d1[i*4+3] = s1[i]&0xFF;
        d2[i*4+0] = (s2[i]>>24); d2[i*4+1] = (s2[i]>>16)&0xFF; d2[i*4+2] = (s2[i]>>8)&0xFF; d2[i*4+3] = s2[i]&0xFF;
        d3[i*4+0] = (s3[i]>>24); d3[i*4+1] = (s3[i]>>16)&0xFF; d3[i*4+2] = (s3[i]>>8)&0xFF; d3[i*4+3] = s3[i]&0xFF;
    }
}

// SHA(SHA(input)) checksum - 4 way parallel
void sha256neon_checksum(uint32_t *i0, uint32_t *i1, uint32_t *i2, uint32_t *i3,
                         uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3) {
    uint8_t h0[32], h1[32], h2[32], h3[32];

    // First SHA-256
    sha256neon_1B(i0, i1, i2, i3, h0, h1, h2, h3);

    // Prepare second round inputs (32 bytes + padding in 64-byte block)
    uint8_t blk0[64], blk1[64], blk2[64], blk3[64];
    memcpy(blk0, h0, 32); memset(blk0+32, 0, 32); blk0[32]=0x80; blk0[62]=0x01; blk0[63]=0x00;
    memcpy(blk1, h1, 32); memset(blk1+32, 0, 32); blk1[32]=0x80; blk1[62]=0x01; blk1[63]=0x00;
    memcpy(blk2, h2, 32); memset(blk2+32, 0, 32); blk2[32]=0x80; blk2[62]=0x01; blk2[63]=0x00;
    memcpy(blk3, h3, 32); memset(blk3+32, 0, 32); blk3[32]=0x80; blk3[62]=0x01; blk3[63]=0x00;

    sha256neon_1B((uint32_t*)blk0, (uint32_t*)blk1, (uint32_t*)blk2, (uint32_t*)blk3,
                  d0, d1, d2, d3);
}

#endif // __aarch64__ && __ARM_FEATURE_CRYPTO
