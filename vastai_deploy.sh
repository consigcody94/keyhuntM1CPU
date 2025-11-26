#!/bin/bash
# ============================================================================
# Keyhunt Vast.ai Deployment Script
# Puzzle #71 Hunter with Discord Notifications
# ============================================================================
# This script:
# 1. Sets up the environment
# 2. Builds keyhunt with CUDA
# 3. Runs in background (survives SSH disconnect)
# 4. Sends Discord notifications on start, progress, and FOUND
# 5. Auto-stops when target is found
# ============================================================================

set -e

# ============================================================================
# CONFIGURATION - EDIT THESE VALUES
# ============================================================================

# Discord webhook URL - REQUIRED! Get from Discord Server Settings > Integrations > Webhooks
DISCORD_WEBHOOK=""

# Target address for Puzzle #71
TARGET_ADDRESS="1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU"

# Bit range
BIT_RANGE=71

# Number of CPU threads (vast.ai 16x5090 machine has 96-core EPYC)
CPU_THREADS=96

# Stats interval in seconds
STATS_INTERVAL=60

# Log file location
LOG_FILE="/root/keyhunt_puzzle71.log"
RESULT_FILE="/root/keyhunt_FOUND.txt"

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

    curl -s -H "Content-Type: application/json" \
        -d "{
            \"embeds\": [{
                \"title\": \"🔑 $title\",
                \"description\": \"$message\",
                \"color\": $color,
                \"fields\": [
                    {\"name\": \"Machine\", \"value\": \"$hostname\", \"inline\": true},
                    {\"name\": \"GPU\", \"value\": \"$gpu_info\", \"inline\": true},
                    {\"name\": \"Target\", \"value\": \"\`$TARGET_ADDRESS\`\", \"inline\": false}
                ],
                \"timestamp\": \"$(date -u +%Y-%m-%dT%H:%M:%SZ)\"
            }]
        }" \
        "$DISCORD_WEBHOOK" || echo "[WARN] Discord notification failed"
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
    echo "[INFO] Target file created: /root/puzzle71_target.txt"
}

# ============================================================================
# RUN KEYHUNT WITH MONITORING
# ============================================================================

run_keyhunt() {
    echo "============================================"
    echo "Starting Keyhunt Puzzle #71 Hunt"
    echo "============================================"

    cd /root/keyhuntM1CPU

    # Send start notification
    send_discord "Hunt Started" "Keyhunt Puzzle #71 search beginning on vast.ai" 16776960

    # Create the monitoring script
    cat > /root/keyhunt_monitor.sh << 'MONITOR_EOF'
#!/bin/bash

LOG_FILE="$1"
RESULT_FILE="$2"
DISCORD_WEBHOOK="$3"
TARGET_ADDRESS="$4"

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
                    {\"name\": \"Address\", \"value\": \"\`$TARGET_ADDRESS\`\", \"inline\": false},
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
    curl -s -H "Content-Type: application/json" \
        -d "{
            \"embeds\": [{
                \"title\": \"📊 Keyhunt Progress Update\",
                \"description\": \"Search is running...\",
                \"color\": 3447003,
                \"fields\": [
                    {\"name\": \"Speed\", \"value\": \"$speed\", \"inline\": true},
                    {\"name\": \"Progress\", \"value\": \"$progress\", \"inline\": true}
                ],
                \"timestamp\": \"$(date -u +%Y-%m-%dT%H:%M:%SZ)\"
            }]
        }" \
        "$DISCORD_WEBHOOK"
}

last_progress_time=0
progress_interval=1800  # Send progress update every 30 minutes

while true; do
    # Check if keyhunt found the key
    if grep -q "FOUND" "$LOG_FILE" 2>/dev/null; then
        echo "[MONITOR] KEY FOUND!"

        # Extract the private key
        private_key=$(grep -A1 "FOUND" "$LOG_FILE" | grep -oE '[0-9a-fA-F]{64}' | head -1)

        if [ -n "$private_key" ]; then
            # Save to result file
            echo "FOUND AT: $(date)" > "$RESULT_FILE"
            echo "Private Key: $private_key" >> "$RESULT_FILE"
            echo "Address: $TARGET_ADDRESS" >> "$RESULT_FILE"

            # Send Discord notification
            send_found_notification "$private_key"

            # Kill keyhunt
            pkill -f "keyhunt.*bsgs" || true

            echo "[MONITOR] Notifications sent, keyhunt stopped"
            exit 0
        fi
    fi

    # Send periodic progress updates
    current_time=$(date +%s)
    if [ $((current_time - last_progress_time)) -ge $progress_interval ]; then
        if [ -f "$LOG_FILE" ]; then
            # Get latest speed from log
            speed=$(tail -20 "$LOG_FILE" | grep -oE '[0-9]+\.?[0-9]* [GMK]Key/s' | tail -1 || echo "calculating...")
            progress=$(tail -5 "$LOG_FILE" | grep -oE '[0-9]+\.?[0-9]*%' | tail -1 || echo "running...")

            if [ -n "$DISCORD_WEBHOOK" ]; then
                send_progress_notification "$speed" "$progress"
            fi
        fi
        last_progress_time=$current_time
    fi

    sleep 30
done
MONITOR_EOF

    chmod +x /root/keyhunt_monitor.sh

    # Start keyhunt in background with logging
    echo "[INFO] Starting keyhunt..."
    echo "Command: ./build/keyhunt -m bsgs -f /root/puzzle71_target.txt -b $BIT_RANGE -t $CPU_THREADS -R -s $STATS_INTERVAL"

    nohup ./build/keyhunt -m bsgs \
        -f /root/puzzle71_target.txt \
        -b $BIT_RANGE \
        -t $CPU_THREADS \
        -R \
        -s $STATS_INTERVAL \
        >> "$LOG_FILE" 2>&1 &

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
    echo "You can now safely disconnect from SSH!"
    echo ""
    echo "Useful commands:"
    echo "  tail -f $LOG_FILE          # Watch live output"
    echo "  cat /root/keyhunt.pid       # Get keyhunt PID"
    echo "  kill \$(cat /root/keyhunt.pid) # Stop keyhunt"
    echo "  nvidia-smi                  # Check GPU usage"
    echo ""
    echo "Discord notifications will be sent for:"
    echo "  - Progress updates (every 30 min)"
    echo "  - KEY FOUND (immediately)"
    echo ""
    echo "If key is found, it will be saved to: $RESULT_FILE"
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
    nvidia-smi --query-gpu=name,utilization.gpu,memory.used,memory.total --format=csv
}

# ============================================================================
# STOP FUNCTION
# ============================================================================

stop() {
    echo "Stopping keyhunt..."

    if [ -f /root/keyhunt.pid ]; then
        kill $(cat /root/keyhunt.pid) 2>/dev/null || true
        rm /root/keyhunt.pid
    fi

    if [ -f /root/monitor.pid ]; then
        kill $(cat /root/monitor.pid) 2>/dev/null || true
        rm /root/monitor.pid
    fi

    pkill -f "keyhunt.*bsgs" 2>/dev/null || true

    echo "Keyhunt stopped"
    send_discord "Hunt Stopped" "Keyhunt was manually stopped" 16711680
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
    run)
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
    *)
        echo "Usage: $0 {setup|build|run|status|stop}"
        echo ""
        echo "  setup  - Full setup: install deps, clone, build"
        echo "  build  - Just rebuild keyhunt"
        echo "  run    - Start keyhunt (runs setup if needed)"
        echo "  status - Check if keyhunt is running"
        echo "  stop   - Stop keyhunt"
        ;;
esac
