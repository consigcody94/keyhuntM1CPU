/*
 * Keyhunt CUDA - secp256k1 32-bit Limb Implementation
 *
 * This implements secp256k1 elliptic curve operations using 32-bit limbs
 * for optimal GPU performance. NVIDIA GPUs have 2-4x more 32-bit ALUs
 * than 64-bit, making this representation significantly faster.
 *
 * 256-bit integers are represented as 8 x 32-bit limbs:
 * value = limb[0] + limb[1]*2^32 + limb[2]*2^64 + ... + limb[7]*2^224
 */

#ifndef KEYHUNT_CUDA_SECP256K1_CUH
#define KEYHUNT_CUDA_SECP256K1_CUH

#include <cuda_runtime.h>
#include <stdint.h>

// ============================================================================
// 256-bit Integer Type (8 x 32-bit limbs)
// ============================================================================

struct uint256_t {
    uint32_t limbs[8];
};

// ============================================================================
// Elliptic Curve Point
// ============================================================================

struct Point {
    uint256_t x;
    uint256_t y;
    uint256_t z;  // For Jacobian coordinates
};

// ============================================================================
// secp256k1 Curve Constants (stored in constant memory)
// ============================================================================

// Prime: p = 2^256 - 2^32 - 977
__constant__ uint32_t SECP256K1_P[8] = {
    0xFFFFFC2F, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

// Order: n = FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141
__constant__ uint32_t SECP256K1_N[8] = {
    0xD0364141, 0xBFD25E8C, 0xAF48A03B, 0xBAAEDCE6,
    0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

// Generator point G.x
__constant__ uint32_t SECP256K1_GX[8] = {
    0x16F81798, 0x59F2815B, 0x2DCE28D9, 0x029BFCDB,
    0xCE870B07, 0x55A06295, 0xF9DCBBAC, 0x79BE667E
};

// Generator point G.y
__constant__ uint32_t SECP256K1_GY[8] = {
    0xFB10D4B8, 0x9C47D08F, 0xA6855419, 0xFD17B448,
    0x0E1108A8, 0x5DA4FBFC, 0x26A3C465, 0x483ADA77
};

// ============================================================================
// Basic 256-bit Operations (32-bit limbs)
// ============================================================================

// Add two 256-bit numbers: r = a + b (returns carry)
__device__ __forceinline__ uint32_t add256(uint256_t* r, const uint256_t* a, const uint256_t* b) {
    uint64_t carry = 0;

    #pragma unroll
    for (int i = 0; i < 8; i++) {
        carry += (uint64_t)a->limbs[i] + (uint64_t)b->limbs[i];
        r->limbs[i] = (uint32_t)carry;
        carry >>= 32;
    }

    return (uint32_t)carry;
}

// Subtract two 256-bit numbers: r = a - b (returns borrow)
__device__ __forceinline__ uint32_t sub256(uint256_t* r, const uint256_t* a, const uint256_t* b) {
    int64_t borrow = 0;

    #pragma unroll
    for (int i = 0; i < 8; i++) {
        borrow = (int64_t)a->limbs[i] - (int64_t)b->limbs[i] + borrow;
        r->limbs[i] = (uint32_t)borrow;
        borrow >>= 32;
    }

    return (uint32_t)(borrow & 1);
}

// Compare: returns -1 if a < b, 0 if a == b, 1 if a > b
__device__ __forceinline__ int cmp256(const uint256_t* a, const uint256_t* b) {
    #pragma unroll
    for (int i = 7; i >= 0; i--) {
        if (a->limbs[i] < b->limbs[i]) return -1;
        if (a->limbs[i] > b->limbs[i]) return 1;
    }
    return 0;
}

// Check if zero
__device__ __forceinline__ bool isZero256(const uint256_t* a) {
    uint32_t r = 0;
    #pragma unroll
    for (int i = 0; i < 8; i++) {
        r |= a->limbs[i];
    }
    return r == 0;
}

// Copy
__device__ __forceinline__ void copy256(uint256_t* dst, const uint256_t* src) {
    #pragma unroll
    for (int i = 0; i < 8; i++) {
        dst->limbs[i] = src->limbs[i];
    }
}

// Set from constant memory
__device__ __forceinline__ void set256FromConst(uint256_t* dst, const uint32_t* src) {
    #pragma unroll
    for (int i = 0; i < 8; i++) {
        dst->limbs[i] = src[i];
    }
}

// ============================================================================
// Modular Arithmetic (mod p)
// ============================================================================

// Modular reduction for secp256k1
// Uses the special form: p = 2^256 - 2^32 - 977
__device__ void mod_p(uint256_t* r, const uint256_t* a) {
    uint256_t p;
    set256FromConst(&p, SECP256K1_P);

    // Simple reduction - subtract p while a >= p
    uint256_t temp;
    copy256(&temp, a);

    while (cmp256(&temp, &p) >= 0) {
        sub256(&temp, &temp, &p);
    }

    copy256(r, &temp);
}

// Modular addition: r = (a + b) mod p
__device__ void modAdd(uint256_t* r, const uint256_t* a, const uint256_t* b) {
    uint256_t p;
    set256FromConst(&p, SECP256K1_P);

    uint32_t carry = add256(r, a, b);

    if (carry || cmp256(r, &p) >= 0) {
        sub256(r, r, &p);
    }
}

// Modular subtraction: r = (a - b) mod p
__device__ void modSub(uint256_t* r, const uint256_t* a, const uint256_t* b) {
    uint256_t p;
    set256FromConst(&p, SECP256K1_P);

    uint32_t borrow = sub256(r, a, b);

    if (borrow) {
        add256(r, r, &p);
    }
}

// Modular multiplication: r = (a * b) mod p
// Uses schoolbook multiplication with 32-bit limbs
__device__ void modMul(uint256_t* r, const uint256_t* a, const uint256_t* b) {
    uint64_t product[16] = {0};  // 512-bit intermediate result

    // Schoolbook multiplication
    #pragma unroll
    for (int i = 0; i < 8; i++) {
        uint64_t carry = 0;
        #pragma unroll
        for (int j = 0; j < 8; j++) {
            uint64_t mul = (uint64_t)a->limbs[i] * (uint64_t)b->limbs[j];
            uint64_t sum = product[i + j] + mul + carry;
            product[i + j] = sum & 0xFFFFFFFF;
            carry = sum >> 32;
        }
        product[i + 8] = carry;
    }

    // Reduction using secp256k1 special form
    // p = 2^256 - 2^32 - 977 = 2^256 - c where c = 0x1000003D1
    const uint64_t c = 0x1000003D1ULL;

    // Reduce high 256 bits
    uint64_t carry = 0;
    #pragma unroll
    for (int i = 0; i < 8; i++) {
        uint64_t sum = product[i] + product[i + 8] * c + carry;
        product[i] = sum & 0xFFFFFFFF;
        carry = sum >> 32;
    }

    // Handle remaining carry
    while (carry) {
        uint64_t sum = product[0] + carry * c;
        product[0] = sum & 0xFFFFFFFF;
        carry = sum >> 32;

        for (int i = 1; i < 8 && carry; i++) {
            sum = product[i] + carry;
            product[i] = sum & 0xFFFFFFFF;
            carry = sum >> 32;
        }
    }

    // Final reduction if needed
    uint256_t p;
    set256FromConst(&p, SECP256K1_P);

    #pragma unroll
    for (int i = 0; i < 8; i++) {
        r->limbs[i] = (uint32_t)product[i];
    }

    if (cmp256(r, &p) >= 0) {
        sub256(r, r, &p);
    }
}

// Modular squaring (optimized)
__device__ void modSqr(uint256_t* r, const uint256_t* a) {
    modMul(r, a, a);  // Can be optimized further
}

// ============================================================================
// Modular Inversion using Fermat's Little Theorem
// a^(-1) = a^(p-2) mod p
// ============================================================================

__device__ void modInv(uint256_t* r, const uint256_t* a) {
    // p - 2 for secp256k1
    uint256_t exp;
    set256FromConst(&exp, SECP256K1_P);
    exp.limbs[0] -= 2;  // p - 2

    uint256_t base, result;
    copy256(&base, a);

    // Set result to 1
    result.limbs[0] = 1;
    for (int i = 1; i < 8; i++) result.limbs[i] = 0;

    // Binary exponentiation
    for (int i = 0; i < 256; i++) {
        int limbIdx = i / 32;
        int bitIdx = i % 32;

        if ((exp.limbs[limbIdx] >> bitIdx) & 1) {
            modMul(&result, &result, &base);
        }
        modSqr(&base, &base);
    }

    copy256(r, &result);
}

// ============================================================================
// Elliptic Curve Point Operations (Jacobian Coordinates)
// ============================================================================

// Point doubling: R = 2*P
__device__ void pointDouble(Point* R, const Point* P) {
    if (isZero256(&P->y) || isZero256(&P->z)) {
        // Point at infinity
        R->x.limbs[0] = 0; R->y.limbs[0] = 1; R->z.limbs[0] = 0;
        for (int i = 1; i < 8; i++) {
            R->x.limbs[i] = R->y.limbs[i] = R->z.limbs[i] = 0;
        }
        return;
    }

    uint256_t S, M, T, Y2, Z2;

    // S = 4*X*Y^2
    modSqr(&Y2, &P->y);
    modMul(&S, &P->x, &Y2);
    modAdd(&S, &S, &S);
    modAdd(&S, &S, &S);

    // M = 3*X^2 (since a=0 for secp256k1)
    modSqr(&M, &P->x);
    modAdd(&T, &M, &M);
    modAdd(&M, &T, &M);

    // X' = M^2 - 2*S
    modSqr(&R->x, &M);
    modSub(&R->x, &R->x, &S);
    modSub(&R->x, &R->x, &S);

    // Y' = M*(S - X') - 8*Y^4
    modSub(&T, &S, &R->x);
    modMul(&R->y, &M, &T);
    modSqr(&T, &Y2);
    modAdd(&T, &T, &T);
    modAdd(&T, &T, &T);
    modAdd(&T, &T, &T);
    modSub(&R->y, &R->y, &T);

    // Z' = 2*Y*Z
    modMul(&R->z, &P->y, &P->z);
    modAdd(&R->z, &R->z, &R->z);
}

// Point addition: R = P + Q (P != Q)
__device__ void pointAdd(Point* R, const Point* P, const Point* Q) {
    if (isZero256(&P->z)) {
        copy256(&R->x, &Q->x);
        copy256(&R->y, &Q->y);
        copy256(&R->z, &Q->z);
        return;
    }
    if (isZero256(&Q->z)) {
        copy256(&R->x, &P->x);
        copy256(&R->y, &P->y);
        copy256(&R->z, &P->z);
        return;
    }

    uint256_t U1, U2, S1, S2, H, R_val, HH, HHH, V;
    uint256_t Z1Z1, Z2Z2;

    // Z1Z1 = Z1^2, Z2Z2 = Z2^2
    modSqr(&Z1Z1, &P->z);
    modSqr(&Z2Z2, &Q->z);

    // U1 = X1*Z2Z2, U2 = X2*Z1Z1
    modMul(&U1, &P->x, &Z2Z2);
    modMul(&U2, &Q->x, &Z1Z1);

    // S1 = Y1*Z2*Z2Z2, S2 = Y2*Z1*Z1Z1
    modMul(&S1, &P->y, &Q->z);
    modMul(&S1, &S1, &Z2Z2);
    modMul(&S2, &Q->y, &P->z);
    modMul(&S2, &S2, &Z1Z1);

    // H = U2 - U1
    modSub(&H, &U2, &U1);

    // R = S2 - S1
    modSub(&R_val, &S2, &S1);

    if (isZero256(&H)) {
        if (isZero256(&R_val)) {
            // P == Q, use doubling
            pointDouble(R, P);
            return;
        } else {
            // P == -Q, result is infinity
            R->x.limbs[0] = 0; R->y.limbs[0] = 1; R->z.limbs[0] = 0;
            for (int i = 1; i < 8; i++) {
                R->x.limbs[i] = R->y.limbs[i] = R->z.limbs[i] = 0;
            }
            return;
        }
    }

    // HH = H^2, HHH = H*HH
    modSqr(&HH, &H);
    modMul(&HHH, &H, &HH);

    // V = U1*HH
    modMul(&V, &U1, &HH);

    // X3 = R^2 - HHH - 2*V
    modSqr(&R->x, &R_val);
    modSub(&R->x, &R->x, &HHH);
    modSub(&R->x, &R->x, &V);
    modSub(&R->x, &R->x, &V);

    // Y3 = R*(V - X3) - S1*HHH
    modSub(&R->y, &V, &R->x);
    modMul(&R->y, &R_val, &R->y);
    modMul(&S1, &S1, &HHH);
    modSub(&R->y, &R->y, &S1);

    // Z3 = Z1*Z2*H
    modMul(&R->z, &P->z, &Q->z);
    modMul(&R->z, &R->z, &H);
}

// Scalar multiplication: R = k * P
__device__ void scalarMult(Point* R, const uint256_t* k, const Point* P) {
    // Initialize R to infinity
    R->x.limbs[0] = 0; R->y.limbs[0] = 1; R->z.limbs[0] = 0;
    for (int i = 1; i < 8; i++) {
        R->x.limbs[i] = R->y.limbs[i] = R->z.limbs[i] = 0;
    }

    Point Q;
    copy256(&Q.x, &P->x);
    copy256(&Q.y, &P->y);
    copy256(&Q.z, &P->z);

    // Double-and-add
    for (int i = 0; i < 256; i++) {
        int limbIdx = i / 32;
        int bitIdx = i % 32;

        if ((k->limbs[limbIdx] >> bitIdx) & 1) {
            pointAdd(R, R, &Q);
        }
        pointDouble(&Q, &Q);
    }
}

// Convert from Jacobian to affine coordinates
__device__ void toAffine(Point* P) {
    if (isZero256(&P->z)) return;

    uint256_t zInv, zInv2, zInv3;

    modInv(&zInv, &P->z);
    modSqr(&zInv2, &zInv);
    modMul(&zInv3, &zInv2, &zInv);

    modMul(&P->x, &P->x, &zInv2);
    modMul(&P->y, &P->y, &zInv3);

    P->z.limbs[0] = 1;
    for (int i = 1; i < 8; i++) P->z.limbs[i] = 0;
}

#endif // KEYHUNT_CUDA_SECP256K1_CUH
