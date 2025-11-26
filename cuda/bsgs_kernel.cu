/*
 * Keyhunt CUDA - BSGS (Baby Step Giant Step) Kernel
 *
 * This kernel performs the "giant step" phase of BSGS on the GPU.
 * Each thread checks a different starting point in parallel.
 *
 * The 32-bit limb representation allows us to maximize GPU throughput
 * since NVIDIA GPUs have significantly more 32-bit ALUs than 64-bit.
 */

#include "secp256k1.cuh"
#include <stdio.h>

// ============================================================================
// Configuration
// ============================================================================

#define BLOCK_SIZE 256
#define POINTS_PER_THREAD 256

// ============================================================================
// Shared Memory for Baby Step Table Lookup
// ============================================================================

// We store a compressed hash of X coordinates for quick lookup
// Full verification happens on CPU for matches
struct BabyStepEntry {
    uint32_t hash;      // 32-bit hash of X coordinate
    uint32_t index;     // Index in baby step table
};

// ============================================================================
// Hash function for X coordinate (for bloom-like lookup)
// ============================================================================

__device__ __forceinline__ uint32_t hashXCoord(const uint256_t* x) {
    // Simple hash combining all limbs
    uint32_t h = 0x811c9dc5;  // FNV offset basis
    #pragma unroll
    for (int i = 0; i < 8; i++) {
        h ^= x->limbs[i];
        h *= 0x01000193;  // FNV prime
    }
    return h;
}

// ============================================================================
// Check if X coordinate might be in baby step table (bloom filter style)
// ============================================================================

__device__ __forceinline__ bool checkBloom(
    const uint256_t* x,
    const uint8_t* bloomFilter,
    uint32_t bloomSize
) {
    uint32_t h1 = hashXCoord(x);
    uint32_t h2 = (h1 >> 16) | (h1 << 16);

    uint32_t bit1 = h1 % (bloomSize * 8);
    uint32_t bit2 = h2 % (bloomSize * 8);
    uint32_t bit3 = (h1 ^ h2) % (bloomSize * 8);

    bool b1 = (bloomFilter[bit1 / 8] >> (bit1 % 8)) & 1;
    bool b2 = (bloomFilter[bit2 / 8] >> (bit2 % 8)) & 1;
    bool b3 = (bloomFilter[bit3 / 8] >> (bit3 % 8)) & 1;

    return b1 && b2 && b3;
}

// ============================================================================
// BSGS Giant Step Kernel
// ============================================================================

__global__ void bsgsGiantStepKernel(
    // Target public key (what we're looking for)
    const uint32_t* targetX,
    const uint32_t* targetY,

    // Giant step parameters
    const uint32_t* giantStepX,    // X coord of m*G (giant step increment)
    const uint32_t* giantStepY,    // Y coord of m*G
    const uint32_t* startOffsetLimbs, // Starting offset in range

    // Bloom filter for baby step table
    const uint8_t* bloomFilter,
    uint32_t bloomSize,

    // Output: potential matches
    uint32_t* matchFlags,          // 1 if potential match found
    uint32_t* matchIndices,        // Giant step index where match found
    uint32_t* matchXCoords,        // X coordinate of potential match

    // Search parameters
    uint64_t numGiantSteps,
    uint64_t giantStepsPerThread
) {
    uint64_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    uint64_t totalThreads = gridDim.x * blockDim.x;

    // Load target point
    Point target;
    for (int i = 0; i < 8; i++) {
        target.x.limbs[i] = targetX[i];
        target.y.limbs[i] = targetY[i];
    }
    target.z.limbs[0] = 1;
    for (int i = 1; i < 8; i++) target.z.limbs[i] = 0;

    // Load giant step (negative, for subtraction)
    Point giantStep;
    for (int i = 0; i < 8; i++) {
        giantStep.x.limbs[i] = giantStepX[i];
        giantStep.y.limbs[i] = giantStepY[i];
    }
    giantStep.z.limbs[0] = 1;
    for (int i = 1; i < 8; i++) giantStep.z.limbs[i] = 0;

    // Negate Y for subtraction (P - Q = P + (-Q))
    uint256_t p;
    set256FromConst(&p, SECP256K1_P);
    modSub(&giantStep.y, &p, &giantStep.y);

    // Calculate starting point for this thread
    uint64_t startIdx = tid * giantStepsPerThread;
    if (startIdx >= numGiantSteps) return;

    uint64_t endIdx = min(startIdx + giantStepsPerThread, numGiantSteps);

    // Compute starting point: target - startIdx * giantStep
    Point current;
    copy256(&current.x, &target.x);
    copy256(&current.y, &target.y);
    copy256(&current.z, &target.z);

    // Skip to our starting position
    if (startIdx > 0) {
        uint256_t skipAmount;
        skipAmount.limbs[0] = (uint32_t)(startIdx & 0xFFFFFFFF);
        skipAmount.limbs[1] = (uint32_t)(startIdx >> 32);
        for (int i = 2; i < 8; i++) skipAmount.limbs[i] = 0;

        // Compute skip * giantStep and subtract from target
        Point skipPoint;
        Point giantStepPositive;
        copy256(&giantStepPositive.x, &giantStep.x);
        // Use positive Y for scalar mult
        modSub(&giantStepPositive.y, &p, &giantStep.y);
        copy256(&giantStepPositive.z, &giantStep.z);

        scalarMult(&skipPoint, &skipAmount, &giantStepPositive);

        // Negate for subtraction
        modSub(&skipPoint.y, &p, &skipPoint.y);

        pointAdd(&current, &target, &skipPoint);
    }

    // Giant step loop
    for (uint64_t i = startIdx; i < endIdx; i++) {
        // Convert to affine for X comparison
        Point affine;
        copy256(&affine.x, &current.x);
        copy256(&affine.y, &current.y);
        copy256(&affine.z, &current.z);
        toAffine(&affine);

        // Check bloom filter
        if (checkBloom(&affine.x, bloomFilter, bloomSize)) {
            // Potential match! Record it for CPU verification
            uint32_t idx = atomicAdd(&matchFlags[0], 1);
            if (idx < 1024) {  // Limit matches to avoid overflow
                matchIndices[idx] = (uint32_t)i;
                // Store X coordinate for verification
                for (int j = 0; j < 8; j++) {
                    matchXCoords[idx * 8 + j] = affine.x.limbs[j];
                }
            }
        }

        // Move to next giant step: current = current - giantStep
        pointAdd(&current, &current, &giantStep);
    }
}

// ============================================================================
// Batch Point Generation Kernel (for baby steps)
// ============================================================================

__global__ void generateBabyStepsKernel(
    uint32_t* outputX,      // Array of X coordinates
    uint32_t* outputHash,   // Array of hashes
    uint64_t numPoints,
    uint64_t startIndex
) {
    uint64_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= numPoints) return;

    uint64_t pointIndex = startIndex + tid;

    // Compute pointIndex * G
    uint256_t k;
    k.limbs[0] = (uint32_t)(pointIndex & 0xFFFFFFFF);
    k.limbs[1] = (uint32_t)(pointIndex >> 32);
    for (int i = 2; i < 8; i++) k.limbs[i] = 0;

    // Generator point
    Point G;
    set256FromConst(&G.x, SECP256K1_GX);
    set256FromConst(&G.y, SECP256K1_GY);
    G.z.limbs[0] = 1;
    for (int i = 1; i < 8; i++) G.z.limbs[i] = 0;

    Point result;
    scalarMult(&result, &k, &G);
    toAffine(&result);

    // Store X coordinate
    for (int i = 0; i < 8; i++) {
        outputX[tid * 8 + i] = result.x.limbs[i];
    }

    // Store hash
    outputHash[tid] = hashXCoord(&result.x);
}

// ============================================================================
// Host-side wrapper functions
// ============================================================================

extern "C" {

// Initialize CUDA device
int cudaInit(int deviceId) {
    cudaError_t err = cudaSetDevice(deviceId);
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA Error: %s\n", cudaGetErrorString(err));
        return -1;
    }

    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, deviceId);
    printf("[CUDA] Using device: %s\n", prop.name);
    printf("[CUDA] Compute capability: %d.%d\n", prop.major, prop.minor);
    printf("[CUDA] Multiprocessors: %d\n", prop.multiProcessorCount);
    printf("[CUDA] Max threads per block: %d\n", prop.maxThreadsPerBlock);
    printf("[CUDA] Total global memory: %.2f GB\n",
           prop.totalGlobalMem / (1024.0 * 1024.0 * 1024.0));

    return 0;
}

// Get number of CUDA devices
int cudaGetDeviceCount() {
    int count;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess) return 0;
    return count;
}

// Allocate GPU memory for BSGS
void* cudaAllocateBSGSMemory(size_t size) {
    void* ptr;
    cudaError_t err = cudaMalloc(&ptr, size);
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA Malloc Error: %s\n", cudaGetErrorString(err));
        return NULL;
    }
    return ptr;
}

// Free GPU memory
void cudaFreeMemory(void* ptr) {
    cudaFree(ptr);
}

// Copy to GPU
int cudaCopyToDevice(void* dst, const void* src, size_t size) {
    cudaError_t err = cudaMemcpy(dst, src, size, cudaMemcpyHostToDevice);
    return (err == cudaSuccess) ? 0 : -1;
}

// Copy from GPU
int cudaCopyFromDevice(void* dst, const void* src, size_t size) {
    cudaError_t err = cudaMemcpy(dst, src, size, cudaMemcpyDeviceToHost);
    return (err == cudaSuccess) ? 0 : -1;
}

// Launch BSGS giant step search
int cudaLaunchBSGS(
    void* d_targetX, void* d_targetY,
    void* d_giantStepX, void* d_giantStepY,
    void* d_startOffset,
    void* d_bloomFilter, uint32_t bloomSize,
    void* d_matchFlags, void* d_matchIndices, void* d_matchXCoords,
    uint64_t numGiantSteps,
    int numBlocks, int threadsPerBlock
) {
    uint64_t giantStepsPerThread = (numGiantSteps + numBlocks * threadsPerBlock - 1) /
                                   (numBlocks * threadsPerBlock);

    bsgsGiantStepKernel<<<numBlocks, threadsPerBlock>>>(
        (uint32_t*)d_targetX, (uint32_t*)d_targetY,
        (uint32_t*)d_giantStepX, (uint32_t*)d_giantStepY,
        (uint32_t*)d_startOffset,
        (uint8_t*)d_bloomFilter, bloomSize,
        (uint32_t*)d_matchFlags, (uint32_t*)d_matchIndices, (uint32_t*)d_matchXCoords,
        numGiantSteps, giantStepsPerThread
    );

    cudaError_t err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA Kernel Error: %s\n", cudaGetErrorString(err));
        return -1;
    }

    return 0;
}

} // extern "C"
