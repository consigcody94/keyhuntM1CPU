# Keyhunt on Vast.ai - 16x RTX 5090 Setup Guide

## Machine Specs
- **GPU**: 16x NVIDIA RTX 5090 (1721.3 TFLOPS total)
- **VRAM**: 32 GB per GPU (512 GB total)
- **CPU**: AMD EPYC 9654 96-Core
- **RAM**: 1032 GB
- **Storage**: 6125.5 GB NVMe
- **Cost**: $8.846/hour
- **CUDA**: 13.0

## Time & Cost Estimates for Puzzle #71

### Performance Projections
- **RTX 5090 per GPU**: ~15-20 GKey/s (estimated, 32-bit limb optimized)
- **16x RTX 5090**: ~240-320 GKey/s total
- **Puzzle #71 keyspace**: 2^70 keys (~1.18 x 10^21 keys)
- **BSGS with 512GB VRAM**: Can hold ~2^37 baby steps (~137 billion entries)

### Expected Search Time
With BSGS algorithm using massive RAM/VRAM:
- **Baby step table**: 2^37 entries in GPU memory
- **Giant steps needed**: 2^70 / 2^37 = 2^33 (~8.6 billion)
- **At 320 GKey/s**: ~27 seconds per full sweep
- **50% probability hit**: ~30 minutes to 2 hours

### Cost Estimate
| Scenario | Time | Cost |
|----------|------|------|
| Lucky (25% search) | 30 min | ~$4.50 |
| Average (50% search) | 1-2 hours | ~$9-18 |
| Unlucky (75% search) | 3-4 hours | ~$27-36 |
| Worst case (full search) | 6-8 hours | ~$54-72 |

**Expected average cost: $20-50**

---

## Setup Instructions

### Step 1: Rent the Machine on Vast.ai

1. Go to [vast.ai](https://vast.ai)
2. Find machine #28017812 (16x RTX 5090, Taiwan)
3. Select "PyTorch" or "Ubuntu" template
4. Rent the instance

### Step 2: Connect via SSH

```bash
# Vast.ai will provide SSH command like:
ssh -p 51010 root@<instance-ip>
```

### Step 3: Clone and Build Keyhunt

```bash
# Install dependencies
apt-get update
apt-get install -y git cmake libssl-dev libgmp-dev libomp-dev

# Clone the repo (you'll need to authenticate for private repo)
git clone https://github.com/consigcody94/keyhuntM1CPU.git
cd keyhuntM1CPU

# Build with CUDA for RTX 5090 (Blackwell = sm_100)
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DKEYHUNT_USE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES="90;100"

cmake --build build -j$(nproc)
```

### Step 4: Create Target File

```bash
# Puzzle #71 target address
echo "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU" > puzzle71.txt
```

### Step 5: Run Keyhunt

```bash
# BSGS mode with maximum RAM utilization
./build/keyhunt -m bsgs \
  -f puzzle71.txt \
  -b 71 \
  -t 96 \
  -n 137438953472 \
  -k 512 \
  -R \
  -s 10

# Parameters:
# -m bsgs      : Baby-step Giant-step algorithm
# -f puzzle71.txt : Target address file
# -b 71        : 71-bit keyspace
# -t 96        : 96 threads (match CPU cores)
# -n 137438953472 : 2^37 baby steps (use available RAM)
# -k 512       : Factor for GPU memory usage
# -R           : Random starting point
# -s 10        : Stats every 10 seconds
```

### Step 6: Monitor Progress

```bash
# Watch GPU usage
watch -n 1 nvidia-smi

# Monitor keyhunt output for key speed and progress
```

### Step 7: If Found - STOP IMMEDIATELY!

When keyhunt finds the key, it will output:
```
FOUND: Private Key: <hex>
Address: 1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU
```

1. **COPY THE PRIVATE KEY IMMEDIATELY**
2. Stop the instance to save money
3. Import key into a Bitcoin wallet
4. Transfer the BTC to your own secure wallet ASAP

---

## Important Notes

### Security
- The repo is now PRIVATE - don't share the URL
- When you find a key, move the BTC immediately
- Don't leave keys on the vast.ai machine

### Billing
- Vast.ai charges per-second
- Stop the instance when not in use
- Check your balance before starting

### If Build Fails
```bash
# Check CUDA version
nvcc --version

# Adjust architectures if needed (RTX 5090 = Blackwell = sm_100)
# If sm_100 not supported, use sm_90 (Hopper)
cmake -B build -DCMAKE_CUDA_ARCHITECTURES="89;90"
```

### Troubleshooting
- **Out of memory**: Reduce -n parameter
- **Slow speed**: Check GPU utilization with nvidia-smi
- **Build errors**: Ensure CUDA toolkit matches GPU

---

## Quick Start Checklist

- [ ] Rent vast.ai instance #28017812
- [ ] SSH into the machine
- [ ] Clone repo: `git clone https://github.com/consigcody94/keyhuntM1CPU.git`
- [ ] Build: `cmake -B build -DKEYHUNT_USE_CUDA=ON && cmake --build build -j96`
- [ ] Create target: `echo "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU" > puzzle71.txt`
- [ ] Run: `./build/keyhunt -m bsgs -f puzzle71.txt -b 71 -t 96 -R -s 10`
- [ ] Monitor and wait for FOUND output
- [ ] Transfer BTC immediately when found
- [ ] Stop instance

**Good luck hunting!**
