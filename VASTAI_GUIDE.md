# Keyhunt on Vast.ai - 16x RTX 5090 Setup Guide

## Machine Specs
- **GPU**: 16x NVIDIA RTX 5090 (1721.3 TFLOPS total)
- **VRAM**: 32 GB per GPU (512 GB total)
- **CPU**: AMD EPYC 9654 96-Core
- **RAM**: 1032 GB
- **Storage**: 6125.5 GB NVMe
- **Cost**: $8.846/hour
- **CUDA**: 12.8+ required (sm_120 for Blackwell)

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

## 🚀 QUICK START (Automated Script)

The easiest way to run keyhunt is with the automated deployment script:

### Step 1: Rent the Machine
1. Go to [vast.ai](https://vast.ai)
2. Find machine #28017812 (16x RTX 5090, Taiwan)
3. Select **"PyTorch"** template (has CUDA 12.8+)
4. Rent the instance

### Step 2: SSH and Run
```bash
# SSH into machine (vast.ai provides the command)
ssh -p <port> root@<ip>

# Download and run the deploy script
curl -O https://raw.githubusercontent.com/consigcody94/keyhuntM1CPU/main/vastai_deploy.sh
chmod +x vastai_deploy.sh

# IMPORTANT: Edit the script to add your Discord webhook
nano vastai_deploy.sh
# Find DISCORD_WEBHOOK="" and add your webhook URL

# Run it!
./vastai_deploy.sh run
```

### Step 3: Disconnect and Wait
You can now close the SSH connection. The script:
- Runs keyhunt in background (survives disconnect)
- Sends Discord notifications every 30 minutes
- **IMMEDIATELY notifies you when key is FOUND**
- Auto-stops and saves the key

### Step 4: Check Status (optional)
```bash
./vastai_deploy.sh status    # Check if running
tail -f /root/keyhunt_puzzle71.log  # Watch live output
./vastai_deploy.sh stop      # Stop if needed
```

---

## Manual Setup (Alternative)

If you prefer manual setup:

### Step 1: Install Dependencies
```bash
apt-get update
apt-get install -y git cmake libssl-dev libgmp-dev libomp-dev screen
```

### Step 2: Clone and Build
```bash
# Clone the repo (private - authenticate first)
gh auth login  # or use personal access token
git clone https://github.com/consigcody94/keyhuntM1CPU.git
cd keyhuntM1CPU

# Build with CUDA for RTX 5090 (Blackwell = sm_120)
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DKEYHUNT_USE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES="120"

cmake --build build -j$(nproc)
```

### Step 3: Run in Screen (survives disconnect)
```bash
# Create target file
echo "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU" > puzzle71.txt

# Start screen session
screen -S keyhunt

# Run keyhunt
./build/keyhunt -m bsgs \
  -f puzzle71.txt \
  -b 71 \
  -t 96 \
  -R \
  -s 60 | tee keyhunt.log

# Detach: Ctrl+A, then D
# Reattach later: screen -r keyhunt
```

---

## GPU Architecture Reference

| GPU Series | Architecture | CUDA Arch |
|------------|--------------|-----------|
| RTX 5090/5080/5070 | Blackwell | sm_120 |
| RTX 4090/4080/4070 | Ada Lovelace | sm_89 |
| RTX 3090/3080/3060 | Ampere | sm_86 |
| A100/H100 | Hopper | sm_90 |

**Important**: RTX 5090 requires CUDA 12.8+ with sm_120 support!

The deploy script auto-detects your GPU and uses the correct architecture.

---

## Discord Webhook Setup

1. Open Discord, go to your server
2. Server Settings → Integrations → Webhooks
3. Create New Webhook
4. Copy the webhook URL
5. Paste into `vastai_deploy.sh` at line `DISCORD_WEBHOOK=""`

You'll receive:
- 🟡 **Hunt Started** - when keyhunt begins
- 🔵 **Progress Updates** - every 30 minutes
- 🟢 **KEY FOUND** - immediately with private key!

---

## If Found - STOP IMMEDIATELY!

When keyhunt finds the key:

1. **Discord will ping you** with the private key
2. **Key saved to** `/root/keyhunt_FOUND.txt`
3. **COPY THE PRIVATE KEY** before doing anything else
4. Stop the vast.ai instance
5. Import into Electrum or other wallet
6. **Transfer BTC immediately** to your secure wallet

---

## Troubleshooting

### Build Fails with "sm_120 not supported"
The CUDA toolkit is too old. Use a newer vast.ai template:
```bash
# Check CUDA version
nvcc --version
# Need 12.8+, if older try sm_89 fallback:
cmake -B build -DCMAKE_CUDA_ARCHITECTURES="89"
```

### "no kernel image available"
Wrong CUDA architecture. The deploy script auto-detects, but manually:
```bash
nvidia-smi  # Check GPU model
# Then rebuild with correct sm_XX
```

### Out of Memory
Reduce the baby step table:
```bash
./build/keyhunt -m bsgs -f puzzle71.txt -b 71 -t 96 -n 68719476736 -R -s 60
# -n 68719476736 = 2^36 instead of 2^37
```

### Slow Speed
1. Check GPU utilization: `nvidia-smi`
2. Ensure using CUDA build (not CPU-only)
3. Verify all 16 GPUs are being used

---

## Security Reminders

- ⚠️ Repository is **PRIVATE** - don't share URL
- ⚠️ Move BTC **immediately** when found
- ⚠️ Don't leave private keys on vast.ai machine
- ⚠️ Consider using a fresh wallet for receiving

---

## Quick Reference

| Command | Description |
|---------|-------------|
| `./vastai_deploy.sh run` | Start hunting |
| `./vastai_deploy.sh status` | Check status |
| `./vastai_deploy.sh stop` | Stop hunting |
| `tail -f /root/keyhunt_puzzle71.log` | Watch live log |
| `cat /root/keyhunt_FOUND.txt` | Check if key found |
| `nvidia-smi` | GPU status |

**Good luck hunting! 🎯**
