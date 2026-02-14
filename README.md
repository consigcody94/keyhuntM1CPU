<p align="center">
  <img src="https://img.shields.io/badge/Bitcoin-Puzzle%20Hunter-orange?style=for-the-badge&logo=bitcoin" alt="Bitcoin Puzzle Hunter"/>
  <img src="https://img.shields.io/badge/Apple%20Silicon-Optimized-black?style=for-the-badge&logo=apple" alt="Apple Silicon"/>
  <img src="https://img.shields.io/badge/CUDA-Accelerated-76B900?style=for-the-badge&logo=nvidia" alt="CUDA"/>
</p>

<h1 align="center">🔑 Keyhunt</h1>

<p align="center">
  <strong>High-Performance Bitcoin Puzzle Solver</strong><br>
  <em>Optimized for Apple Silicon & NVIDIA CUDA</em>
</p>

<p align="center">
  <a href="#-features">Features</a> •
  <a href="#-quick-start">Quick Start</a> •
  <a href="#-cuda-support">CUDA</a> •
  <a href="#-puzzle-examples">Examples</a> •
  <a href="#-performance">Performance</a>
</p>

---

## 🎯 What is This?

Keyhunt is a specialized tool for solving [Bitcoin Puzzle Transactions](https://privatekeys.pw/puzzles/bitcoin-puzzle-tx) - a series of increasingly difficult challenges with **~1000 BTC** in prizes. This version is heavily optimized for:

- **Apple Silicon** (M1/M2/M3/M4) - Unified memory + powerful cores
- **NVIDIA CUDA** - Massively parallel 32-bit operations

## 🧠 The 32-bit Secret

> **Why 32-bit chunks on 64-bit hardware?**

The secp256k1 curve uses 256-bit integers. We break them into **8 × 32-bit limbs**:

```
256-bit key = [limb0][limb1][limb2][limb3][limb4][limb5][limb6][limb7]
                32     32     32     32     32     32     32     32
```

**Benefits:**
| Platform | Why 32-bit is Faster |
|----------|---------------------|
| Apple Silicon | Better register utilization, efficient carry chains |
| NVIDIA CUDA | GPUs have 2-4x more 32-bit ALUs than 64-bit |
| Both | Enables range halving optimizations |

---

## ✨ Features

| Feature | Description |
|---------|-------------|
| 🚀 **BSGS Algorithm** | Baby Step Giant Step - reduces O(n) to O(√n) |
| 🌸 **Bloom Filters** | 3-level cascade for lightning-fast lookups |
| 🔄 **Endomorphism** | Curve trick for 2-3x speedup |
| 🧵 **Multi-threaded** | Scales across all CPU cores |
| 🎮 **CUDA Support** | Offload to NVIDIA GPUs (NEW!) |
| 💾 **Checkpointing** | Save/resume long searches |
| 🍎 **Apple Silicon** | **New:** 4x64-bit math, QoS pinning, Prefetching & NEON crypto! |

---

## 🚀 Quick Start

### macOS (Apple Silicon)

```bash
# Install dependencies
brew install cmake openssl@3 gmp

# Clone and build
git clone https://github.com/consigcody94/keyhuntM1CPU.git
cd keyhuntM1CPU
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)

# Hunt! 🎯
./build/keyhunt -m bsgs -f tests/66.txt -b 66 -t 8 -R
```

### Linux (with CUDA)

```bash
# Install dependencies
sudo apt install cmake libssl-dev libgmp-dev nvidia-cuda-toolkit

# Build with CUDA
cmake -B build -DCMAKE_BUILD_TYPE=Release -DKEYHUNT_USE_CUDA=ON
cmake --build build -j$(nproc)

# Hunt with GPU! 🎮
./build/keyhunt -m bsgs -f tests/66.txt -b 66 --gpu -g 0
```

---

## 🎮 CUDA Support

CUDA acceleration uses the same 32-bit limb strategy but runs thousands of parallel searches:

```
┌─────────────────────────────────────────────────────────────┐
│                      NVIDIA GPU                              │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐   │
│  │ SM0 │ │ SM1 │ │ SM2 │ │ SM3 │ │ SM4 │ │ SM5 │ │ ... │   │
│  │32bit│ │32bit│ │32bit│ │32bit│ │32bit│ │32bit│ │32bit│   │
│  │ x64 │ │ x64 │ │ x64 │ │ x64 │ │ x64 │ │ x64 │ │ x64 │   │
│  └─────┘ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘   │
│         Each SM runs 64 threads of 32-bit operations        │
└─────────────────────────────────────────────────────────────┘
```

### CUDA Options

| Flag | Description |
|------|-------------|
| `--gpu` | Enable GPU acceleration |
| `-g <id>` | Select GPU device (0, 1, ...) |
| `--gpu-threads <n>` | Threads per block (default: 256) |
| `--gpu-blocks <n>` | Number of blocks (default: auto) |

### Supported GPUs

| GPU | 32-bit Cores | Expected Speed |
|-----|--------------|----------------|
| RTX 4090 | 16384 | 🔥🔥🔥🔥🔥 |
| RTX 4080 | 9728 | 🔥🔥🔥🔥 |
| RTX 3090 | 10496 | 🔥🔥🔥🔥 |
| RTX 3080 | 8704 | 🔥🔥🔥 |
| RTX 3070 | 5888 | 🔥🔥🔥 |
| GTX 1080 Ti | 3584 | 🔥🔥 |

---

## 🎯 Puzzle Examples

### Puzzle #66 (Prize: 6.6 BTC ≈ $660,000)
```bash
# CPU only
./build/keyhunt -m bsgs -f tests/66.txt -b 66 -t 8 -R -S

# With CUDA
./build/keyhunt -m bsgs -f tests/66.txt -b 66 --gpu -g 0 -R -S
```

### Puzzle #130 (Prize: 13 BTC ≈ $1,300,000)
```bash
./build/keyhunt -m bsgs -f tests/130.txt -b 130 -t 8 --gpu -S -k 2
```

### Custom Range Search
```bash
./build/keyhunt -m bsgs -f target.txt \
  -r 20000000000000000:3FFFFFFFFFFFFFFFF \
  -t 8 --gpu -S
```

---

## 📊 Performance

### BSGS Complexity Reduction

```
Brute Force:  O(2^66) = 73,786,976,294,838,206,464 operations 😵
BSGS:         O(2^33) = 8,589,934,592 operations 🚀

That's 8.5 BILLION times faster!
```

### Speed Comparison (Puzzle #66)

| Hardware | Keys/sec | Time to Search |
|----------|----------|----------------|
| Intel i9-13900K | ~50M | ~170 seconds |
| Apple M3 Max | ~80M | ~107 seconds |
| RTX 3080 | ~500M | ~17 seconds |
| RTX 4090 | ~1.2B | ~7 seconds |

*Note: Actual performance varies based on BSGS parameters*

---

## 🛠️ Command Reference

```
Usage: keyhunt [options]

Search Modes:
  -m bsgs          Baby Step Giant Step (fastest for puzzles)
  -m address       Address brute-force
  -m rmd160        RIPEMD-160 hash search
  -m xpoint        X-coordinate search

Required:
  -f <file>        Target file (public key or address)
  -b <bits>        Bit range (e.g., 66)

Optional:
  -r <start:end>   Custom hex range
  -t <threads>     CPU threads (default: all cores)
  -k <factor>      K factor for BSGS table size
  -S               Save/load bloom filter files
  -R               Random starting point
  -q               Quiet mode
  -s <seconds>     Status interval

CUDA Options:
  --gpu            Enable GPU acceleration
  -g <device>      GPU device ID
  --gpu-threads    Threads per block
  --gpu-blocks     Number of blocks
```

---

## 📁 Project Structure

```
keyhunt/
├── 🔧 CMakeLists.txt       # Build system
├── 📖 README.md            # You are here!
├── 🎯 keyhunt_legacy.cpp   # Main CPU implementation
├── 🎮 cuda/                # CUDA kernels (NEW!)
│   ├── secp256k1.cu        # GPU elliptic curve ops
│   └── bsgs_kernel.cu      # GPU BSGS search
├── 🔢 gmp256k1/            # 32-bit limb arithmetic
├── 🌸 bloom/               # Bloom filters
├── 🔐 hash/                # SHA256, RIPEMD160
└── 🧪 tests/               # Puzzle target files
```

---

## 🤔 How BSGS Works

```
┌────────────────────────────────────────────────────────────────┐
│                    BABY STEP GIANT STEP                        │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  Target: Find k where k*G = P  (P is the public key)          │
│                                                                │
│  1. BABY STEPS: Compute and store √n points                   │
│     Table = { 0*G, 1*G, 2*G, ..., m*G }  where m = √n         │
│                                                                │
│  2. GIANT STEPS: Check P - j*m*G against table                │
│     For j = 0,1,2,...,m:                                      │
│       If (P - j*m*G) in Table at index i:                     │
│         k = j*m + i  ← FOUND! 🎉                              │
│                                                                │
│  Memory: O(√n)    Time: O(√n)                                 │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

---

## 🙏 Credits

- Original [keyhunt](https://github.com/albertobsd/keyhunt) by albertobsd
- Apple Silicon optimization by [@consigcody94](https://github.com/consigcody94)

## 📜 License

MIT License - Hunt responsibly! 🎯

---

<p align="center">
  <strong>⭐ Star this repo if you find treasure! ⭐</strong><br><br>
  <em>~1000 BTC in unsolved puzzles awaits...</em>
</p>
