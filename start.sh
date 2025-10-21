#!/usr/bin/env bash
set -euo pipefail

CONTAINER_NAME="ubuntu-novnc"
DOCKER_IMAGE="thuonghai2711/ubuntu-novnc-pulseaudio:22.04"
HOST_PORT=8080
CLOUDFLARED_LOG="/tmp/cloudflared.log"
MARKER="$HOME/.cleanup_done"

info() { echo -e "\033[1;34m[INFO]\033[0m $*"; }
warn() { echo -e "\033[1;33m[WARN]\033[0m $*"; }

install_base_packages() {
  info "Installing dependencies (no Docker through apt)..."
  sudo apt-get update -y
  sudo apt-get install -y socat wget unzip sudo coreutils gnupg
}

install_cloudflared() {
  if ! command -v cloudflared >/dev/null 2>&1; then
    info "Installing cloudflared manually..."
    wget -q https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb -O /tmp/cloudflared.deb
    sudo dpkg -i /tmp/cloudflared.deb || sudo apt-get -f install -y
    rm -f /tmp/cloudflared.deb
  fi
}

one_time_cleanup() {
  if [ ! -f "$MARKER" ]; then
    info "Performing one-time cleanup in $HOME..."
    rm -rf "$HOME/.gradle" "$HOME/.emu" 2>/dev/null || true
    touch "$MARKER"
  fi
}

start_docker() {
  if ! docker info >/dev/null 2>&1; then
    warn "Docker daemon is not running. Starting Docker (Codespaces)..."
    sudo service docker start || warn "Docker service start failed, but might already be running."
  else
    info "Docker is already running."
  fi
}

create_or_start_container() {
  if ! docker ps -a --format '{{.Names}}' | grep -qx "$CONTAINER_NAME"; then
    info "Creating docker container: $CONTAINER_NAME"
    docker run --name "$CONTAINER_NAME" \
      --shm-size 1g -d \
      --cap-add=SYS_ADMIN \
      -p ${HOST_PORT}:10000 \
      -e VNC_PASSWD=12345678 \
      "$DOCKER_IMAGE"
  else
    info "Starting existing container..."
    docker start "$CONTAINER_NAME" || true
  fi
}

install_chrome_in_container() {
  info "Installing Chrome inside container..."
  docker exec "$CONTAINER_NAME" bash -lc "
    apt update &&
    apt remove -y firefox || true &&
    apt install -y wget gnupg &&
    wget -O /tmp/chrome.deb https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb &&
    apt install -y /tmp/chrome.deb &&
    rm -f /tmp/chrome.deb
  "
}

start_cloudflared() {
  info "Starting Cloudflare Tunnel..."
  pkill cloudflared 2>/dev/null || true
  nohup cloudflared tunnel --no-autoupdate --url http://localhost:${HOST_PORT} > "$CLOUDFLARED_LOG" 2>&1 &
  sleep 10
  URL=$(grep -o "https://[a-z0-9.-]*trycloudflare.com" "$CLOUDFLARED_LOG" | head -n1 || true)
  if [ -n "$URL" ]; then
    echo "✅ Your Remote Desktop is ready:"
    echo "   $URL"
  else
    warn "❌ Cloudflare Tunnel Failed. Check $CLOUDFLARED_LOG"
  fi
}

timer_loop() {
  local elapsed=0
  while true; do
    echo "Time elapsed: $elapsed min"
    ((elapsed++))
    sleep 60
  done
}

# Run
install_base_packages
install_cloudflared
one_time_cleanup
start_docker
create_or_start_container
install_chrome_in_container
start_cloudflared
timer_loop
