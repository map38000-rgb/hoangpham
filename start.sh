#!/bin/bash
set -e

# ========== CONFIG ==========
CONTAINER_NAME="ubuntu-novnc"
IMAGE_NAME="thuonghai2711/ubuntu-novnc-pulseaudio:22.04"
CLOUDFLARED_DEB="https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb"

# ========== LOGGING ==========
info()   { echo -e "\033[1;34m[INFO]\033[0m $1"; }
error()  { echo -e "\033[1;31m[ERROR]\033[0m $1"; }

# ========== STEP 1: Cập nhật & cài gói cần thiết ==========
install_dependencies() {
  info "Installing dependencies..."

  apt update
  apt install -y wget curl sudo unzip gnupg coreutils
}

# ========== STEP 2: Cài cloudflared ==========
install_cloudflared() {
  info "Installing cloudflared..."
  wget -O /tmp/cloudflared.deb "$CLOUDFLARED_DEB"
  apt install -y /tmp/cloudflared.deb || true
}

# ========== STEP 3: Khởi động Docker nếu chưa chạy ==========
start_docker() {
  info "Ensuring Docker service is running..."
  if ! command -v docker &>/dev/null; then
    error "Docker is not installed! Install Docker first."
    exit 1
  fi
}

# ========== STEP 4: Tạo container ==========
create_container() {
  info "Creating Docker container: $CONTAINER_NAME"

  docker rm -f "$CONTAINER_NAME" 2>/dev/null || true

  docker run -d --name "$CONTAINER_NAME" \
    -p 6080:80 -p 5900:5900 \
    --shm-size=2g --privileged \
    "$IMAGE_NAME"
}

# ========== STEP 5: Cài Google Chrome bên trong container ==========
install_chrome_in_container() {
  info "Installing Chrome inside container..."

  docker exec -u 0 "$CONTAINER_NAME" bash -lc "
    export DEBIAN_FRONTEND=noninteractive
    apt update &&
    apt install -y wget gnupg --no-install-recommends &&
    wget -O /tmp/chrome.deb https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb &&
    apt install -y /tmp/chrome.deb &&
    rm -f /tmp/chrome.deb
  "
}

# ========== STEP 6: Tạo Cloudflare Tunnel ===============
create_tunnel() {
  info "Starting Cloudflare Tunnel for noVNC (port 6080)..."
  cloudflared tunnel --url http://localhost:6080 --no-autoupdate &
}

# ========== MAIN FLOW ==========
install_dependencies
install_cloudflared
start_docker
create_container
install_chrome_in_container
create_tunnel

info "DONE! Truy cập VNC tại:"
echo " - http://localhost:6080 (local)"
echo " - Hoặc link Cloudflare xuất hiện bên trên."
