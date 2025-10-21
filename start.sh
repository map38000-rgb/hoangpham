#!/usr/bin/env bash
set -euo pipefail

# ====== CONFIG ======
CONTAINER_NAME="ubuntu-novnc"
DOCKER_IMAGE="thuonghai2711/ubuntu-novnc-pulseaudio:22.04"
HOST_PORT=8080
CLOUDFLARED_LOG="/tmp/cloudflared.log"
MARKER="$HOME/.cleanup_done"
# ====================

info() { echo -e "\033[1;34m[INFO]\033[0m $*"; }
warn() { echo -e "\033[1;33m[WARN]\033[0m $*"; }

# Install Cloudflared (not available via apt)
install_cloudflared() {
  if ! command -v cloudflared >/dev/null 2>&1; then
    info "Installing cloudflared manually..."
    wget -q https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb -O /tmp/cloudflared.deb
    sudo dpkg -i /tmp/cloudflared.deb || sudo apt-get -f install -y
    rm -f /tmp/cloudflared.deb
  fi
}

# Basic required apps
install_base_packages() {
  info "Installing dependencies..."
  sudo apt-get update -y
  sudo apt-get install -y docker.io socat wget unzip sudo coreutils gnupg
}

# Cleanup workspace
one_time_cleanup() {
  if [ ! -f "$MARKER" ]; then
    info "Performing one-time cleanup in $HOME..."
    rm -rf "$HOME/.gradle" "$HOME/.emu" 2>/dev/null || true
    touch "$MARKER"
  fi
}

# Enable Docker
start_docker() {
  if ! pgrep dockerd >/dev/null 2>&1; then
    info "Starting Docker service..."
    sudo service docker start || true
  fi
}

# Create or start container
create_or_start_container() {
  if ! docker ps -a --format '{{.Names}}' | grep -qx "$CONTAINER_NAME"; then
    info "Creating docker container: $CONTAINER_NAME"
    docker run --name "$CONTAINER_NAME" \
      --shm-size 1g -d \
      --cap-add=SYS_ADMIN \
      -p ${HOST_PORT}:10000 \
      -e VNC_PASSWD=12345678 \
      -e SCREEN_WIDTH=1024 \
      -e SCREEN_HEIGHT=768 \
      -e SCREEN_DEPTH=24 \
      "$DOCKER_IMAGE"
  else
    info "Starting existing container..."
    docker start "$CONTAINER_NAME" || true
  fi
}

# Install Chrome inside container
install_chrome_in_container() {
  info "Installing Chrome inside container..."
  docker exec "$CONTAINER_NAME" bash -lc "
    sudo apt update &&
    sudo apt remove -y firefox || true &&
    sudo apt install -y wget gnupg &&
    wget -O /tmp/chrome.deb https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb &&
    sudo apt install -y /tmp/chrome.deb &&
    rm -f /tmp/chrome.deb
  "
}

# Start Cloudflare tunnel
start_cloudflared() {
  info "Starting Cloudflare Tunnel..."
  pkill cloudflared 2>/dev/null || true
  nohup cloudflared tunnel --no-autoupdate --url http://localhost:${HOST_PORT} > "$CLOUDFLARED_LOG" 2>&1 &
  sleep 10
  if grep -q "trycloudflare.com" "$CLOUDFLARED_LOG"; then
    URL=$(grep -o "https://[a-z0-9.-]*trycloudflare.com" "$CLOUDFLARED_LOG" | head -n1)
    echo "======================================"
    echo " ✅ VNC Web UI Available At:"
    echo "     $URL"
    echo "======================================"
  else
    warn "❌ Cloudflare Tunnel Failed!"
  fi
}

# Timer loop
timer_loop() {
  local elapsed=0
  while true; do
    echo "Time elapsed: $elapsed min"
    ((elapsed++))
    sleep 60
  done
}

# ========= MAIN =========
install_base_packages
install_cloudflared
one_time_cleanup
start_docker
create_or_start_container
install_chrome_in_container
start_cloudflared
timer_loop
