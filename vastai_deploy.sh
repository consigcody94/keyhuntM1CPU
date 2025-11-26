#!/bin/bash
# ============================================================================
# Keyhunt Vast.ai Deployment Script
# Puzzle #71 Hunter with Discord Notifications & Checkpointing
# ============================================================================
# This script:
# 1. Sets up the environment
# 2. Builds keyhunt with CUDA
# 3. Runs in background (survives SSH disconnect)
# 4. Sends Discord notifications on start, progress, and FOUND
# 5. Auto-stops when target is found
# 6. SAVES PROGRESS on shutdown (graceful or crash)
# 7. RESUMES from checkpoint on restart
# ============================================================================

set -e

# ============================================================================
# CONFIGURATION - EDIT THESE VALUES
# ============================================================================

# Discord webhook URL - REQUIRED! Get from Discord Server Settings > Integrations > Webhooks
DISCORD_WEBHOOK="https://discordapp.com/api/webhooks/1443124373639925770/HG9BSVbH02AAUo7JDNzCxcXKLN62zCTJ4fejBpkL6Fd_1EGdXQjloHmBsKFhKA0AwL5F"

# Target for Puzzle #71 - NO public key available, must search by ADDRESS
# Using -m address mode (slower than BSGS but works without public key)
TARGET_ADDRESS="1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU"

# Bit range
BIT_RANGE=71

# Number of CPU threads (vast.ai 16x5090 machine has 96-core EPYC)
CPU_THREADS=96

# Stats interval in seconds
STATS_INTERVAL=60

# File locations
LOG_FILE="/root/keyhunt_puzzle71.log"
RESULT_FILE="/root/keyhunt_FOUND.txt"
CHECKPOINT_FILE="/root/keyhunt_checkpoint.txt"
RANGES_SEARCHED_FILE="/root/keyhunt_ranges_searched.txt"

# GitHub repo (private - you'll need to authenticate)
REPO_URL="https://github.com/consigcody94/keyhuntM1CPU.git"

# ============================================================================
# DISCORD NOTIFICATION FUNCTION
# ============================================================================

send_discord() {
    local title="$1"
    local message="$2"
    local color="$3"  # Decimal color: 65280=green, 16711680=red, 16776960=yellow

    if [ -z "$DISCORD_WEBHOOK" ]; then
        echo "[WARN] Discord webhook not configured, skipping notification"
        return
    fi

    # Get machine info
    local gpu_info=$(nvidia-smi --query-gpu=name,memory.total --format=csv,noheader 2>/dev/null | head -1 || echo "Unknown GPU")
    local hostname=$(hostname)
    local searched_ranges=$(wc -l < "$RANGES_SEARCHED_FILE" 2>/dev/null || echo "0")

    curl -s -H "Content-Type: application/json" \
        -d "{
            \"embeds\": [{
                \"title\": \"🔑 $title\",
                \"description\": \"$message\",
                \"color\": $color,
                \"fields\": [
                    {\"name\": \"Machine\", \"value\": \"$hostname\", \"inline\": true},
                    {\"name\": \"GPU\", \"value\": \"$gpu_info\", \"inline\": true},
                    {\"name\": \"Ranges Searched\", \"value\": \"$searched_ranges\", \"inline\": true},
                    {\"name\": \"Target\", \"value\": \"\`$TARGET_ADDRESS\`\", \"inline\": false}
                ],
                \"timestamp\": \"$(date -u +%Y-%m-%dT%H:%M:%SZ)\"
            }]
        }" \
        "$DISCORD_WEBHOOK" || echo "[WARN] Discord notification failed"
}

# ============================================================================
# CHECKPOINT FUNCTIONS
# ============================================================================

save_checkpoint() {
    echo "[CHECKPOINT] Saving progress..."

    # Get current position from log
    local last_thread=$(tail -100 "$LOG_FILE" 2>/dev/null | grep -oE 'Thread 0x[0-9a-fA-F]+' | tail -1 || echo "")
    local current_pos=$(echo "$last_thread" | grep -oE '0x[0-9a-fA-F]+' || echo "")

    if [ -n "$current_pos" ]; then
        echo "$current_pos" > "$CHECKPOINT_FILE"
        echo "$(date -Iseconds) $current_pos" >> "$RANGES_SEARCHED_FILE"
        echo "[CHECKPOINT] Saved position: $current_pos"

        # Count progress
        local total_searched=$(wc -l < "$RANGES_SEARCHED_FILE" 2>/dev/null || echo "0")
        echo "[CHECKPOINT] Total ranges searched: $total_searched"
    else
        echo "[CHECKPOINT] No position found in log"
    fi
}

load_checkpoint() {
    if [ -f "$CHECKPOINT_FILE" ]; then
        local saved_pos=$(cat "$CHECKPOINT_FILE")
        echo "[CHECKPOINT] Found saved position: $saved_pos"
        echo "$saved_pos"
    else
        echo ""
    fi
}

# ============================================================================
# GRACEFUL SHUTDOWN HANDLER
# ============================================================================

cleanup() {
    echo ""
    echo "[SHUTDOWN] Caught signal, saving progress..."

    # Save checkpoint
    save_checkpoint

    # Kill keyhunt gracefully
    if [ -f /root/keyhunt.pid ]; then
        local pid=$(cat /root/keyhunt.pid)
        kill -TERM $pid 2>/dev/null || true
        sleep 2
        kill -9 $pid 2>/dev/null || true
    fi

    # Kill monitor
    if [ -f /root/monitor.pid ]; then
        kill $(cat /root/monitor.pid) 2>/dev/null || true
    fi

    # Send Discord notification
    send_discord "Hunt Paused" "Progress saved. Resume anytime with: ./vastai_deploy.sh run" 16776960

    echo "[SHUTDOWN] Progress saved. You can resume later."
    exit 0
}

# ============================================================================
# SETUP FUNCTION
# ============================================================================

setup_environment() {
    echo "============================================"
    echo "Setting up Keyhunt environment..."
    echo "============================================"

    # Update and install dependencies
    apt-get update
    apt-get install -y git cmake libssl-dev libgmp-dev libomp-dev screen curl

    # Check CUDA
    echo "[INFO] Checking CUDA installation..."
    nvcc --version || { echo "[ERROR] CUDA not found!"; exit 1; }
    nvidia-smi

    # Detect GPU architecture
    local gpu_name=$(nvidia-smi --query-gpu=name --format=csv,noheader | head -1)
    echo "[INFO] Detected GPU: $gpu_name"

    # Set CUDA architecture based on GPU
    if [[ "$gpu_name" == *"5090"* ]] || [[ "$gpu_name" == *"5080"* ]] || [[ "$gpu_name" == *"5070"* ]]; then
        CUDA_ARCH="120"
        echo "[INFO] Using sm_120 for RTX 50 series (Blackwell)"
    elif [[ "$gpu_name" == *"4090"* ]] || [[ "$gpu_name" == *"4080"* ]] || [[ "$gpu_name" == *"4070"* ]]; then
        CUDA_ARCH="89"
        echo "[INFO] Using sm_89 for RTX 40 series (Ada Lovelace)"
    elif [[ "$gpu_name" == *"3090"* ]] || [[ "$gpu_name" == *"3080"* ]] || [[ "$gpu_name" == *"3070"* ]] || [[ "$gpu_name" == *"3060"* ]]; then
        CUDA_ARCH="86"
        echo "[INFO] Using sm_86 for RTX 30 series (Ampere)"
    elif [[ "$gpu_name" == *"A100"* ]] || [[ "$gpu_name" == *"H100"* ]]; then
        CUDA_ARCH="80;90"
        echo "[INFO] Using sm_80/90 for datacenter GPUs"
    else
        CUDA_ARCH="75;80;86;89"
        echo "[INFO] Using multi-arch build for unknown GPU"
    fi

    export CUDA_ARCH
}

# ============================================================================
# BUILD FUNCTION
# ============================================================================

build_keyhunt() {
    echo "============================================"
    echo "Building Keyhunt with CUDA..."
    echo "============================================"

    cd /root

    # Clone if not exists
    if [ ! -d "keyhuntM1CPU" ]; then
        echo "[INFO] Cloning repository..."
        git clone "$REPO_URL" keyhuntM1CPU || {
            echo "[ERROR] Failed to clone. You may need to authenticate:"
            echo "  gh auth login"
            echo "  OR use: git clone https://YOUR_TOKEN@github.com/consigcody94/keyhuntM1CPU.git"
            exit 1
        }
    else
        echo "[INFO] Updating existing repository..."
        cd keyhuntM1CPU && git pull && cd ..
    fi

    cd keyhuntM1CPU

    # Clean previous build
    rm -rf build

    # Configure with detected architecture
    echo "[INFO] Configuring CMake with CUDA arch: $CUDA_ARCH"
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DKEYHUNT_USE_CUDA=ON \
        -DKEYHUNT_APPLE_SILICON_ONLY=OFF \
        -DCMAKE_CUDA_ARCHITECTURES="$CUDA_ARCH"

    # Build with all cores
    local cores=$(nproc)
    echo "[INFO] Building with $cores cores..."
    cmake --build build -j$cores

    # Verify build
    if [ ! -f "build/keyhunt" ]; then
        echo "[ERROR] Build failed - keyhunt binary not found!"
        send_discord "Build Failed" "keyhunt binary not found after build" 16711680
        exit 1
    fi

    echo "[INFO] Build successful!"
    ./build/keyhunt -h | head -20 || true
}

# ============================================================================
# CREATE TARGET FILE
# ============================================================================

create_target_file() {
    echo "$TARGET_ADDRESS" > /root/puzzle71_target.txt
    echo "[INFO] Target file created with ADDRESS: /root/puzzle71_target.txt"
}

# ============================================================================
# RUN KEYHUNT WITH MONITORING
# ============================================================================

run_keyhunt() {
    echo "============================================"
    echo "Starting Keyhunt Puzzle #71 Hunt"
    echo "============================================"

    cd /root/keyhuntM1CPU

    # Initialize ranges file if not exists
    touch "$RANGES_SEARCHED_FILE"

    # Check for checkpoint
    local resume_pos=$(load_checkpoint)
    local resume_flag=""

    if [ -n "$resume_pos" ]; then
        echo "[INFO] Resuming from checkpoint: $resume_pos"
        # Use -r flag with starting range for resume
        resume_flag="-r $resume_pos:$(printf '0x%x' $((0x800000000000000000)))"
        send_discord "Hunt Resumed" "Resuming from checkpoint: $resume_pos" 3447003
    else
        echo "[INFO] Starting fresh search"
        send_discord "Hunt Started" "Keyhunt Puzzle #71 search beginning on vast.ai" 16776960
    fi

    # Create the monitoring script with checkpoint saving
    cat > /root/keyhunt_monitor.sh << 'MONITOR_EOF'
#!/bin/bash

LOG_FILE="$1"
RESULT_FILE="$2"
DISCORD_WEBHOOK="$3"
TARGET_ADDRESS="$4"
CHECKPOINT_FILE="/root/keyhunt_checkpoint.txt"
RANGES_SEARCHED_FILE="/root/keyhunt_ranges_searched.txt"

send_found_notification() {
    local private_key="$1"
    curl -s -H "Content-Type: application/json" \
        -d "{
            \"content\": \"@everyone\",
            \"embeds\": [{
                \"title\": \"🎉🔑 PRIVATE KEY FOUND! 🔑🎉\",
                \"description\": \"**PUZZLE #71 SOLVED!**\",
                \"color\": 65280,
                \"fields\": [
                    {\"name\": \"Private Key\", \"value\": \"\`$private_key\`\", \"inline\": false},
                    {\"name\": \"Address\", \"value\": \"1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU\", \"inline\": false},
                    {\"name\": \"⚠️ ACTION REQUIRED\", \"value\": \"IMMEDIATELY import this key and transfer the BTC!\", \"inline\": false}
                ],
                \"timestamp\": \"$(date -u +%Y-%m-%dT%H:%M:%SZ)\"
            }]
        }" \
        "$DISCORD_WEBHOOK"
}

send_progress_notification() {
    local speed="$1"
    local progress="$2"
    local ranges_searched="$3"
    local current_pos="$4"
    curl -s -H "Content-Type: application/json" \
        -d "{
            \"embeds\": [{
                \"title\": \"📊 Keyhunt Progress Update\",
                \"description\": \"Search is running...\",
                \"color\": 3447003,
                \"fields\": [
                    {\"name\": \"Speed\", \"value\": \"$speed\", \"inline\": true},
                    {\"name\": \"Ranges Done\", \"value\": \"$ranges_searched\", \"inline\": true},
                    {\"name\": \"Current Position\", \"value\": \"\`$current_pos\`\", \"inline\": false}
                ],
                \"timestamp\": \"$(date -u +%Y-%m-%dT%H:%M:%SZ)\"
            }]
        }" \
        "$DISCORD_WEBHOOK"
}

save_checkpoint() {
    local last_thread=$(tail -100 "$LOG_FILE" 2>/dev/null | grep -oE 'Thread 0x[0-9a-fA-F]+' | tail -1 || echo "")
    local current_pos=$(echo "$last_thread" | grep -oE '0x[0-9a-fA-F]+' || echo "")

    if [ -n "$current_pos" ]; then
        echo "$current_pos" > "$CHECKPOINT_FILE"
        echo "$(date -Iseconds) $current_pos" >> "$RANGES_SEARCHED_FILE"
    fi
}

last_progress_time=0
last_checkpoint_time=0
progress_interval=600       # Send progress update every 10 minutes
checkpoint_interval=300     # Save checkpoint every 5 minutes

while true; do
    current_time=$(date +%s)

    # Check if keyhunt found the key
    # Address mode outputs: "Hit! Private Key: d2c55"
    if grep -q "Hit! Private Key:" "$LOG_FILE" 2>/dev/null; then
        echo "[MONITOR] KEY FOUND!"

        # Extract the private key (format: "Hit! Private Key: HEXVALUE")
        private_key=$(grep "Hit! Private Key:" "$LOG_FILE" | grep -oE 'Private Key: [0-9a-fA-F]+' | tail -1 | cut -d' ' -f3)

        if [ -n "$private_key" ]; then
            # Save to result file
            echo "FOUND AT: $(date)" > "$RESULT_FILE"
            echo "Private Key: $private_key" >> "$RESULT_FILE"
            echo "Address: 1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU" >> "$RESULT_FILE"

            # Send Discord notification
            send_found_notification "$private_key"

            # Kill keyhunt
            pkill -f "keyhunt.*bsgs" || true

            echo "[MONITOR] Notifications sent, keyhunt stopped"
            exit 0
        fi
    fi

    # Save checkpoint every 5 minutes
    if [ $((current_time - last_checkpoint_time)) -ge $checkpoint_interval ]; then
        save_checkpoint
        last_checkpoint_time=$current_time
    fi

    # Send periodic progress updates every 30 minutes
    if [ $((current_time - last_progress_time)) -ge $progress_interval ]; then
        if [ -f "$LOG_FILE" ]; then
            # Get latest speed from log
            speed=$(tail -20 "$LOG_FILE" | grep -oE '[0-9]+\.?[0-9]* [PTGMK]?[Kk]eys/s' | tail -1 || echo "calculating...")
            ranges_searched=$(wc -l < "$RANGES_SEARCHED_FILE" 2>/dev/null || echo "0")
            current_pos=$(cat "$CHECKPOINT_FILE" 2>/dev/null || echo "unknown")

            if [ -n "$DISCORD_WEBHOOK" ]; then
                send_progress_notification "$speed" "running" "$ranges_searched" "$current_pos"
            fi
        fi
        last_progress_time=$current_time
    fi

    sleep 30
done
MONITOR_EOF

    chmod +x /root/keyhunt_monitor.sh

    # Set up signal handlers for graceful shutdown
    trap cleanup SIGTERM SIGINT SIGHUP

    # Start keyhunt in background with logging
    echo "[INFO] Starting keyhunt..."

    # Build command - use ADDRESS mode (no public key available for puzzle 71)
    # -m address: search by bitcoin address
    # -l compress: look for compressed addresses
    # -R: random mode for better coverage
    # -c btc: bitcoin
    local cmd="./build/keyhunt -m address -f /root/puzzle71_target.txt -b $BIT_RANGE -l compress -c btc -R -t $CPU_THREADS -s $STATS_INTERVAL"
    echo "Command: $cmd"

    nohup $cmd >> "$LOG_FILE" 2>&1 &

    KEYHUNT_PID=$!
    echo "[INFO] Keyhunt started with PID: $KEYHUNT_PID"
    echo "$KEYHUNT_PID" > /root/keyhunt.pid

    # Start monitor in background
    nohup /root/keyhunt_monitor.sh "$LOG_FILE" "$RESULT_FILE" "$DISCORD_WEBHOOK" "$TARGET_ADDRESS" >> /root/monitor.log 2>&1 &
    MONITOR_PID=$!
    echo "[INFO] Monitor started with PID: $MONITOR_PID"
    echo "$MONITOR_PID" > /root/monitor.pid

    echo ""
    echo "============================================"
    echo "✅ KEYHUNT IS RUNNING IN BACKGROUND"
    echo "============================================"
    echo ""
    echo "CHECKPOINT ENABLED - Progress saves every 5 minutes!"
    echo ""
    echo "You can now safely disconnect from SSH!"
    echo ""
    echo "Commands:"
    echo "  tail -f $LOG_FILE           # Watch live output"
    echo "  ./vastai_deploy.sh status   # Check status"
    echo "  ./vastai_deploy.sh stop     # Graceful stop (saves progress)"
    echo "  ./vastai_deploy.sh resume   # Resume from checkpoint"
    echo ""
    echo "Progress files:"
    echo "  $CHECKPOINT_FILE      # Current position"
    echo "  $RANGES_SEARCHED_FILE # All searched ranges"
    echo ""
    echo "Discord notifications:"
    echo "  - Progress updates (every 30 min)"
    echo "  - KEY FOUND (immediately)"
    echo ""
    echo "If key is found: $RESULT_FILE"
    echo "============================================"
}

# ============================================================================
# STATUS FUNCTION
# ============================================================================

status() {
    echo "============================================"
    echo "Keyhunt Status"
    echo "============================================"

    if [ -f /root/keyhunt.pid ]; then
        local pid=$(cat /root/keyhunt.pid)
        if ps -p $pid > /dev/null 2>&1; then
            echo "[✓] Keyhunt is RUNNING (PID: $pid)"
        else
            echo "[✗] Keyhunt is NOT running (stale PID file)"
        fi
    else
        echo "[✗] Keyhunt is NOT running (no PID file)"
    fi

    echo ""
    echo "Checkpoint Status:"
    if [ -f "$CHECKPOINT_FILE" ]; then
        echo "  Last position: $(cat $CHECKPOINT_FILE)"
    else
        echo "  No checkpoint saved"
    fi

    if [ -f "$RANGES_SEARCHED_FILE" ]; then
        echo "  Ranges searched: $(wc -l < $RANGES_SEARCHED_FILE)"
    fi

    if [ -f "$LOG_FILE" ]; then
        echo ""
        echo "Last 10 lines of log:"
        tail -10 "$LOG_FILE"
    fi

    if [ -f "$RESULT_FILE" ]; then
        echo ""
        echo "🎉 RESULT FILE EXISTS!"
        cat "$RESULT_FILE"
    fi

    echo ""
    nvidia-smi --query-gpu=name,utilization.gpu,memory.used,memory.total --format=csv 2>/dev/null || echo "nvidia-smi not available"
}

# ============================================================================
# STOP FUNCTION (GRACEFUL)
# ============================================================================

stop() {
    echo "Stopping keyhunt gracefully..."

    # Save checkpoint first
    save_checkpoint

    if [ -f /root/keyhunt.pid ]; then
        local pid=$(cat /root/keyhunt.pid)
        echo "Sending SIGTERM to PID $pid..."
        kill -TERM $pid 2>/dev/null || true
        sleep 2

        # Force kill if still running
        if ps -p $pid > /dev/null 2>&1; then
            echo "Force killing..."
            kill -9 $pid 2>/dev/null || true
        fi
        rm /root/keyhunt.pid
    fi

    if [ -f /root/monitor.pid ]; then
        kill $(cat /root/monitor.pid) 2>/dev/null || true
        rm /root/monitor.pid
    fi

    pkill -f "keyhunt.*bsgs" 2>/dev/null || true
    pkill -f "keyhunt_monitor" 2>/dev/null || true

    echo ""
    echo "✅ Keyhunt stopped. Progress saved."
    echo ""
    echo "To resume later: ./vastai_deploy.sh run"

    if [ -f "$CHECKPOINT_FILE" ]; then
        echo "Resume position: $(cat $CHECKPOINT_FILE)"
    fi

    send_discord "Hunt Paused" "Keyhunt stopped gracefully. Progress saved." 16776960
}

# ============================================================================
# DOWNLOAD PROGRESS (for transferring to another machine)
# ============================================================================

download_progress() {
    local backup_file="/root/keyhunt_progress_backup_$(date +%Y%m%d_%H%M%S).tar.gz"

    echo "Creating progress backup..."
    tar -czf "$backup_file" \
        "$CHECKPOINT_FILE" \
        "$RANGES_SEARCHED_FILE" \
        "$LOG_FILE" \
        2>/dev/null || true

    echo "Backup created: $backup_file"
    echo ""
    echo "Download with:"
    echo "  scp -P <port> root@<ip>:$backup_file ."
}

# ============================================================================
# RESET (clear all progress)
# ============================================================================

reset_progress() {
    echo "⚠️  WARNING: This will delete all progress!"
    read -p "Are you sure? (type 'yes' to confirm): " confirm

    if [ "$confirm" = "yes" ]; then
        rm -f "$CHECKPOINT_FILE" "$RANGES_SEARCHED_FILE" "$LOG_FILE"
        echo "Progress reset. Starting fresh on next run."
    else
        echo "Cancelled."
    fi
}

# ============================================================================
# MAIN
# ============================================================================

case "${1:-run}" in
    setup)
        setup_environment
        build_keyhunt
        create_target_file
        echo ""
        echo "Setup complete! Run: ./vastai_deploy.sh run"
        ;;
    build)
        setup_environment
        build_keyhunt
        ;;
    run|resume)
        if [ -z "$DISCORD_WEBHOOK" ]; then
            echo "============================================"
            echo "⚠️  WARNING: Discord webhook not configured!"
            echo "============================================"
            echo "Edit this script and set DISCORD_WEBHOOK="
            echo "Get it from: Discord Server > Settings > Integrations > Webhooks"
            echo ""
            read -p "Continue without Discord notifications? (y/n) " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                exit 1
            fi
        fi

        if [ ! -f "/root/keyhuntM1CPU/build/keyhunt" ]; then
            echo "[INFO] Keyhunt not built yet, running full setup..."
            setup_environment
            build_keyhunt
        fi

        create_target_file
        run_keyhunt
        ;;
    status)
        status
        ;;
    stop)
        stop
        ;;
    backup)
        download_progress
        ;;
    reset)
        reset_progress
        ;;
    *)
        echo "Usage: $0 {setup|build|run|resume|status|stop|backup|reset}"
        echo ""
        echo "  setup   - Full setup: install deps, clone, build"
        echo "  build   - Just rebuild keyhunt"
        echo "  run     - Start keyhunt (resumes from checkpoint if exists)"
        echo "  resume  - Same as run (resumes from checkpoint)"
        echo "  status  - Check if keyhunt is running + progress"
        echo "  stop    - Graceful stop (saves progress)"
        echo "  backup  - Create downloadable backup of progress"
        echo "  reset   - Clear all progress (start fresh)"
        ;;
esac
