/*
 * NEON-accelerated RIPEMD-160 for Apple Silicon M1/M2/M3/M4/M5
 *
 * 4-way parallel RIPEMD-160 using NEON uint32x4_t lanes.
 * Each lane processes an independent message simultaneously.
 * Mirrors the SSE ripemd160sse implementation for ARM.
 *
 * No hardware RIPEMD instructions exist on ARM, so we use
 * NEON SIMD to process 4 messages in parallel.
 *
 * License: GPLv3
 */

#include "ripemd160.h"

#if defined(__aarch64__)

#include <arm_neon.h>
#include <string.h>
#include <stdint.h>

namespace ripemd160neon {

static const uint32_t _init_vals[5] = {
    0x67452301ul, 0xEFCDAB89ul, 0x98BADCFEul, 0x10325476ul, 0xC3D2E1F0ul
};

// NEON RIPEMD-160 helper macros (4-way parallel)
#define ROL(x,n) vorrq_u32(vshlq_n_u32(x, n), vshrq_n_u32(x, 32 - n))
#define NOT(x)   vmvnq_u32(x)

#define f1(x,y,z) veorq_u32(x, veorq_u32(y, z))
#define f2(x,y,z) vorrq_u32(vandq_u32(x,y), vbicq_u32(z,x))
#define f3(x,y,z) veorq_u32(vorrq_u32(x,NOT(y)),z)
#define f4(x,y,z) vorrq_u32(vandq_u32(x,z), vbicq_u32(y,z))
#define f5(x,y,z) veorq_u32(x, vorrq_u32(y, NOT(z)))

#define add3(x0,x1,x2)    vaddq_u32(vaddq_u32(x0,x1),x2)
#define add4(x0,x1,x2,x3) vaddq_u32(vaddq_u32(x0,x1),vaddq_u32(x2,x3))

#define RndNeon(a,b,c,d,e,f,x,k,r) { \
    uint32x4_t u = add4(a,f,x,vdupq_n_u32(k)); \
    a = vaddq_u32(ROL(u,r),e); \
    c = ROL(c, 10); \
}

#define R11(a,b,c,d,e,x,r) RndNeon(a,b,c,d,e,f1(b,c,d),x,0,r)
#define R21(a,b,c,d,e,x,r) RndNeon(a,b,c,d,e,f2(b,c,d),x,0x5A827999ul,r)
#define R31(a,b,c,d,e,x,r) RndNeon(a,b,c,d,e,f3(b,c,d),x,0x6ED9EBA1ul,r)
#define R41(a,b,c,d,e,x,r) RndNeon(a,b,c,d,e,f4(b,c,d),x,0x8F1BBCDCul,r)
#define R51(a,b,c,d,e,x,r) RndNeon(a,b,c,d,e,f5(b,c,d),x,0xA953FD4Eul,r)
#define R12(a,b,c,d,e,x,r) RndNeon(a,b,c,d,e,f5(b,c,d),x,0x50A28BE6ul,r)
#define R22(a,b,c,d,e,x,r) RndNeon(a,b,c,d,e,f4(b,c,d),x,0x5C4DD124ul,r)
#define R32(a,b,c,d,e,x,r) RndNeon(a,b,c,d,e,f3(b,c,d),x,0x6D703EF3ul,r)
#define R42(a,b,c,d,e,x,r) RndNeon(a,b,c,d,e,f2(b,c,d),x,0x7A6D76E9ul,r)
#define R52(a,b,c,d,e,x,r) RndNeon(a,b,c,d,e,f1(b,c,d),x,0,r)

// Load word i from 4 blocks interleaved into NEON lanes
#define LOADW(blk, i) { \
    uint32_t tmp[4] = { \
        *((uint32_t*)blk[0] + i), \
        *((uint32_t*)blk[1] + i), \
        *((uint32_t*)blk[2] + i), \
        *((uint32_t*)blk[3] + i)  \
    }; \
    w[i] = vld1q_u32(tmp); \
}

void Initialize(uint32x4_t *s) {
    for (int i = 0; i < 5; i++) {
        s[i] = vdupq_n_u32(_init_vals[i]);
    }
}

// 4-way parallel RIPEMD-160 transform
void Transform(uint32x4_t *s, uint8_t *blk[4]) {
    uint32x4_t a1 = s[0], b1 = s[1], c1 = s[2], d1 = s[3], e1 = s[4];
    uint32x4_t a2 = a1, b2 = b1, c2 = c1, d2 = d1, e2 = e1;
    uint32x4_t w[16];

    LOADW(blk, 0);  LOADW(blk, 1);  LOADW(blk, 2);  LOADW(blk, 3);
    LOADW(blk, 4);  LOADW(blk, 5);  LOADW(blk, 6);  LOADW(blk, 7);
    LOADW(blk, 8);  LOADW(blk, 9);  LOADW(blk, 10); LOADW(blk, 11);
    LOADW(blk, 12); LOADW(blk, 13); LOADW(blk, 14); LOADW(blk, 15);

    // Left rounds
    R11(a1,b1,c1,d1,e1,w[0],11);  R11(e1,a1,b1,c1,d1,w[1],14);
    R11(d1,e1,a1,b1,c1,w[2],15);  R11(c1,d1,e1,a1,b1,w[3],12);
    R11(b1,c1,d1,e1,a1,w[4],5);   R11(a1,b1,c1,d1,e1,w[5],8);
    R11(e1,a1,b1,c1,d1,w[6],7);   R11(d1,e1,a1,b1,c1,w[7],9);
    R11(c1,d1,e1,a1,b1,w[8],11);  R11(b1,c1,d1,e1,a1,w[9],13);
    R11(a1,b1,c1,d1,e1,w[10],14); R11(e1,a1,b1,c1,d1,w[11],15);
    R11(d1,e1,a1,b1,c1,w[12],6);  R11(c1,d1,e1,a1,b1,w[13],7);
    R11(b1,c1,d1,e1,a1,w[14],9);  R11(a1,b1,c1,d1,e1,w[15],8);

    R21(e1,a1,b1,c1,d1,w[7],7);   R21(d1,e1,a1,b1,c1,w[4],6);
    R21(c1,d1,e1,a1,b1,w[13],8);  R21(b1,c1,d1,e1,a1,w[1],13);
    R21(a1,b1,c1,d1,e1,w[10],11); R21(e1,a1,b1,c1,d1,w[6],9);
    R21(d1,e1,a1,b1,c1,w[15],7);  R21(c1,d1,e1,a1,b1,w[3],15);
    R21(b1,c1,d1,e1,a1,w[12],7);  R21(a1,b1,c1,d1,e1,w[0],12);
    R21(e1,a1,b1,c1,d1,w[9],15);  R21(d1,e1,a1,b1,c1,w[5],9);
    R21(c1,d1,e1,a1,b1,w[2],11);  R21(b1,c1,d1,e1,a1,w[14],7);
    R21(a1,b1,c1,d1,e1,w[11],13); R21(e1,a1,b1,c1,d1,w[8],12);

    R31(d1,e1,a1,b1,c1,w[3],11);  R31(c1,d1,e1,a1,b1,w[10],13);
    R31(b1,c1,d1,e1,a1,w[14],6);  R31(a1,b1,c1,d1,e1,w[4],7);
    R31(e1,a1,b1,c1,d1,w[9],14);  R31(d1,e1,a1,b1,c1,w[15],9);
    R31(c1,d1,e1,a1,b1,w[8],13);  R31(b1,c1,d1,e1,a1,w[1],15);
    R31(a1,b1,c1,d1,e1,w[2],14);  R31(e1,a1,b1,c1,d1,w[7],8);
    R31(d1,e1,a1,b1,c1,w[0],13);  R31(c1,d1,e1,a1,b1,w[6],6);
    R31(b1,c1,d1,e1,a1,w[13],5);  R31(a1,b1,c1,d1,e1,w[11],12);
    R31(e1,a1,b1,c1,d1,w[5],7);   R31(d1,e1,a1,b1,c1,w[12],5);

    R41(c1,d1,e1,a1,b1,w[1],11);  R41(b1,c1,d1,e1,a1,w[9],12);
    R41(a1,b1,c1,d1,e1,w[11],14); R41(e1,a1,b1,c1,d1,w[10],15);
    R41(d1,e1,a1,b1,c1,w[0],14);  R41(c1,d1,e1,a1,b1,w[8],15);
    R41(b1,c1,d1,e1,a1,w[12],9);  R41(a1,b1,c1,d1,e1,w[4],8);
    R41(e1,a1,b1,c1,d1,w[13],9);  R41(d1,e1,a1,b1,c1,w[3],14);
    R41(c1,d1,e1,a1,b1,w[7],5);   R41(b1,c1,d1,e1,a1,w[15],6);
    R41(a1,b1,c1,d1,e1,w[14],8);  R41(e1,a1,b1,c1,d1,w[5],6);
    R41(d1,e1,a1,b1,c1,w[6],5);   R41(c1,d1,e1,a1,b1,w[2],12);

    R51(b1,c1,d1,e1,a1,w[4],9);   R51(a1,b1,c1,d1,e1,w[0],15);
    R51(e1,a1,b1,c1,d1,w[5],5);   R51(d1,e1,a1,b1,c1,w[9],11);
    R51(c1,d1,e1,a1,b1,w[7],6);   R51(b1,c1,d1,e1,a1,w[12],8);
    R51(a1,b1,c1,d1,e1,w[2],13);  R51(e1,a1,b1,c1,d1,w[10],12);
    R51(d1,e1,a1,b1,c1,w[14],5);  R51(c1,d1,e1,a1,b1,w[1],12);
    R51(b1,c1,d1,e1,a1,w[3],13);  R51(a1,b1,c1,d1,e1,w[8],14);
    R51(e1,a1,b1,c1,d1,w[11],11); R51(d1,e1,a1,b1,c1,w[6],8);
    R51(c1,d1,e1,a1,b1,w[15],5);  R51(b1,c1,d1,e1,a1,w[13],6);

    // Right rounds
    R12(a2,b2,c2,d2,e2,w[5],8);   R12(e2,a2,b2,c2,d2,w[14],9);
    R12(d2,e2,a2,b2,c2,w[7],9);   R12(c2,d2,e2,a2,b2,w[0],11);
    R12(b2,c2,d2,e2,a2,w[9],13);  R12(a2,b2,c2,d2,e2,w[2],15);
    R12(e2,a2,b2,c2,d2,w[11],15); R12(d2,e2,a2,b2,c2,w[4],5);
    R12(c2,d2,e2,a2,b2,w[13],7);  R12(b2,c2,d2,e2,a2,w[6],7);
    R12(a2,b2,c2,d2,e2,w[15],8);  R12(e2,a2,b2,c2,d2,w[8],11);
    R12(d2,e2,a2,b2,c2,w[1],14);  R12(c2,d2,e2,a2,b2,w[10],14);
    R12(b2,c2,d2,e2,a2,w[3],12);  R12(a2,b2,c2,d2,e2,w[12],6);

    R22(e2,a2,b2,c2,d2,w[6],9);   R22(d2,e2,a2,b2,c2,w[11],13);
    R22(c2,d2,e2,a2,b2,w[3],15);  R22(b2,c2,d2,e2,a2,w[7],7);
    R22(a2,b2,c2,d2,e2,w[0],12);  R22(e2,a2,b2,c2,d2,w[13],8);
    R22(d2,e2,a2,b2,c2,w[5],9);   R22(c2,d2,e2,a2,b2,w[10],11);
    R22(b2,c2,d2,e2,a2,w[14],7);  R22(a2,b2,c2,d2,e2,w[15],7);
    R22(e2,a2,b2,c2,d2,w[8],12);  R22(d2,e2,a2,b2,c2,w[12],7);
    R22(c2,d2,e2,a2,b2,w[4],6);   R22(b2,c2,d2,e2,a2,w[9],15);
    R22(a2,b2,c2,d2,e2,w[1],13);  R22(e2,a2,b2,c2,d2,w[2],11);

    R32(d2,e2,a2,b2,c2,w[15],9);  R32(c2,d2,e2,a2,b2,w[5],7);
    R32(b2,c2,d2,e2,a2,w[1],15);  R32(a2,b2,c2,d2,e2,w[3],11);
    R32(e2,a2,b2,c2,d2,w[7],8);   R32(d2,e2,a2,b2,c2,w[14],6);
    R32(c2,d2,e2,a2,b2,w[6],6);   R32(b2,c2,d2,e2,a2,w[9],14);
    R32(a2,b2,c2,d2,e2,w[11],12); R32(e2,a2,b2,c2,d2,w[8],13);
    R32(d2,e2,a2,b2,c2,w[12],5);  R32(c2,d2,e2,a2,b2,w[2],14);
    R32(b2,c2,d2,e2,a2,w[10],13); R32(a2,b2,c2,d2,e2,w[0],13);
    R32(e2,a2,b2,c2,d2,w[4],7);   R32(d2,e2,a2,b2,c2,w[13],5);

    R42(c2,d2,e2,a2,b2,w[8],15);  R42(b2,c2,d2,e2,a2,w[6],5);
    R42(a2,b2,c2,d2,e2,w[4],8);   R42(e2,a2,b2,c2,d2,w[1],11);
    R42(d2,e2,a2,b2,c2,w[3],14);  R42(c2,d2,e2,a2,b2,w[11],14);
    R42(b2,c2,d2,e2,a2,w[15],6);  R42(a2,b2,c2,d2,e2,w[0],14);
    R42(e2,a2,b2,c2,d2,w[5],6);   R42(d2,e2,a2,b2,c2,w[12],9);
    R42(c2,d2,e2,a2,b2,w[2],12);  R42(b2,c2,d2,e2,a2,w[13],9);
    R42(a2,b2,c2,d2,e2,w[9],12);  R42(e2,a2,b2,c2,d2,w[7],5);
    R42(d2,e2,a2,b2,c2,w[10],15); R42(c2,d2,e2,a2,b2,w[14],8);

    R52(b2,c2,d2,e2,a2,w[12],8);  R52(a2,b2,c2,d2,e2,w[15],5);
    R52(e2,a2,b2,c2,d2,w[10],12); R52(d2,e2,a2,b2,c2,w[4],9);
    R52(c2,d2,e2,a2,b2,w[1],12);  R52(b2,c2,d2,e2,a2,w[5],5);
    R52(a2,b2,c2,d2,e2,w[8],14);  R52(e2,a2,b2,c2,d2,w[7],6);
    R52(d2,e2,a2,b2,c2,w[6],8);   R52(c2,d2,e2,a2,b2,w[2],13);
    R52(b2,c2,d2,e2,a2,w[13],6);  R52(a2,b2,c2,d2,e2,w[14],5);
    R52(e2,a2,b2,c2,d2,w[0],15);  R52(d2,e2,a2,b2,c2,w[3],13);
    R52(c2,d2,e2,a2,b2,w[9],11);  R52(b2,c2,d2,e2,a2,w[11],11);

    // Final addition
    uint32x4_t t = vaddq_u32(s[1], vaddq_u32(c1, d2));
    s[1] = vaddq_u32(s[2], vaddq_u32(d1, e2));
    s[2] = vaddq_u32(s[3], vaddq_u32(e1, a2));
    s[3] = vaddq_u32(s[4], vaddq_u32(a1, b2));
    s[4] = vaddq_u32(s[0], vaddq_u32(b1, c2));
    s[0] = t;
}

} // namespace ripemd160neon

// Public API: 4-way parallel RIPEMD-160 of 32-byte inputs
void ripemd160neon_32(uint8_t *i0, uint8_t *i1, uint8_t *i2, uint8_t *i3,
                      uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3) {

    // Prepare padded 64-byte blocks for each 32-byte input
    alignas(16) uint8_t blk0[64], blk1[64], blk2[64], blk3[64];

    memcpy(blk0, i0, 32); memset(blk0+32, 0, 32); blk0[32]=0x80; blk0[62]=0x01; blk0[63]=0x00;
    memcpy(blk1, i1, 32); memset(blk1+32, 0, 32); blk1[32]=0x80; blk1[62]=0x01; blk1[63]=0x00;
    memcpy(blk2, i2, 32); memset(blk2+32, 0, 32); blk2[32]=0x80; blk2[62]=0x01; blk2[63]=0x00;
    memcpy(blk3, i3, 32); memset(blk3+32, 0, 32); blk3[32]=0x80; blk3[62]=0x01; blk3[63]=0x00;

    // RIPEMD-160 uses little-endian length: 32 bytes = 256 bits = 0x0100
    // Actually 256 = 0x100, stored little-endian at bytes 56-63 of padding
    // For 32-byte input: bit length = 256 = 0x100
    // Little-endian at offset 56: 0x00, 0x01, 0x00, ...
    blk0[56]=0x00; blk0[57]=0x01; blk1[56]=0x00; blk1[57]=0x01;
    blk2[56]=0x00; blk2[57]=0x01; blk3[56]=0x00; blk3[57]=0x01;
    // Zero out the wrong positions we set above
    blk0[62]=0x00; blk0[63]=0x00; blk1[62]=0x00; blk1[63]=0x00;
    blk2[62]=0x00; blk2[63]=0x00; blk3[62]=0x00; blk3[63]=0x00;

    uint32x4_t state[5];
    ripemd160neon::Initialize(state);

    uint8_t *blks[4] = { blk0, blk1, blk2, blk3 };
    ripemd160neon::Transform(state, blks);

    // Extract results from NEON lanes
    alignas(16) uint32_t out[4];
    for (int i = 0; i < 5; i++) {
        vst1q_u32(out, state[i]);
        memcpy(d0 + i*4, &out[0], 4);
        memcpy(d1 + i*4, &out[1], 4);
        memcpy(d2 + i*4, &out[2], 4);
        memcpy(d3 + i*4, &out[3], 4);
    }
}

#endif // __aarch64__
