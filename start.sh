#!/bin/bash
set -e

# ========= CONFIG =========
CONTAINER_NAME="ubuntu-novnc"
IMAGE_NAME="thuonghai2711/ubuntu-novnc-pulseaudio:22.04"
CLOUDFLARED_DEB="https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb"

# ========= LOGGING =========
info() { echo -e "\033[1;34m[INFO]\033[0m $1"; }
error() { echo -e "\033[1;31m[ERROR]\033[0m $1"; }

# ========= STEP 1: Install dependencies =========
install_dependencies() {
  info "Installing dependencies..."
  apt update
  apt install -y wget curl sudo unzip gnupg coreutils
}

# ========= STEP 2: Install Cloudflared =========
install_cloudflared() {
  info "Installing cloudflared..."
  wget -O /tmp/cloudflared.deb "$CLOUDFLARED_DEB"
  apt install -y /tmp/cloudflared.deb || true
}

# ========= STEP 3: Check Docker =========
start_docker() {
  if ! command -v docker &>/dev/null; then
    error "Docker not installed! Please install Docker first."
    exit 1
  fi
  info "Docker found ✅"
}

# ========= STEP 4: Create container if not exists =========
create_container() {
  if docker ps -a --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    info "Container already exists → starting..."
    docker start "$CONTAINER_NAME"
  else
    info "Creating new container: $CONTAINER_NAME"
    docker run -d --name "$CONTAINER_NAME" \
      -p 6080:80 -p 5900:5900 \
      --shm-size=2g --privileged \
      "$IMAGE_NAME"
  fi
}

# ========= STEP 5: Install Chrome inside container =========
install_chrome_in_container() {
  info "Installing Google Chrome in container..."

  docker exec -u 0 "$CONTAINER_NAME" bash -lc "
    export DEBIAN_FRONTEND=noninteractive
    apt update &&
    apt install -y wget gnupg --no-install-recommends &&
    wget -O /tmp/chrome.deb https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb &&
    apt install -y /tmp/chrome.deb &&
    rm -f /tmp/chrome.deb
  " || error "⚠️ Chrome install failed (bỏ qua nếu đã cài)"
}

# ========= STEP 6: Start Cloudflare tunnel & show URL =========
create_tunnel() {
  info "Starting Cloudflare Tunnel for noVNC..."

  cloudflared tunnel --url http://localhost:6080 --no-autoupdate > /tmp/cloudflared.log 2>&1 &
  sleep 5

  TUNNEL_URL=$(grep -o "https://[0-9a-zA-Z.-]*trycloudflare.com" /tmp/cloudflared.log | head -n1)

  if [ -n "$TUNNEL_URL" ]; then
    echo "======================================"
    echo " 🌍 Public URL (Cloudflare): $TUNNEL_URL"
    echo "======================================"
  else
    echo "⚠️ Không tìm thấy link Cloudflare trong log."
  fi
}

# ========= MAIN FLOW =========
install_dependencies
install_cloudflared
start_docker
create_container
install_chrome_in_container
create_tunnel

info "✅ DONE! Truy cập noVNC tại http://localhost:6080 (local hoặc qua Cloudflare link)"
