#!/usr/bin/env bash
set -euo pipefail

# CONFIGURABLE
DOCKER_IMAGE="thuonghai2711/ubuntu-novnc-pulseaudio:22.04"
CONTAINER_NAME="ubuntu-novnc"
HOST_PORT=8080          # host port mà container publish (host:container -> 8080:10000)
CONTAINER_PORT=10000
VNC_PASSWD="12345678"
AUDIO_PORT=1699
WEBSOCKIFY_PORT=6900
VNC_PORT=5900
SCREEN_WIDTH=1024
SCREEN_HEIGHT=768
SCREEN_DEPTH=24
CLOUDFLARED_LOG="/tmp/cloudflared.log"
SOCAT_PROXY_PORT="${PORT:-8081}"   # preview proxy listen port; mặc định 8081 nếu PORT không set
ELAPSED_FILE="/tmp/novnc_elapsed"

# Packages to ensure installed (APT names)
APT_PACKAGES=(docker.io cloudflared socat coreutils grep sudo wget unzip)

info() { echo -e "\033[1;34m[INFO]\033[0m $*"; }
warn() { echo -e "\033[1;33m[WARN]\033[0m $*"; }
err() { echo -e "\033[1;31m[ERROR]\033[0m $*"; }

ensure_root() {
  if [ "$EUID" -ne 0 ]; then
    warn "Some operations need sudo; continuing but you may be prompted for your password."
  fi
}

apt_install_packages() {
  info "Updating apt and installing packages: ${APT_PACKAGES[*]}"
  sudo apt-get update -y
  # Try install, but don't fail whole script for missing packages (e.g. cloudflared may be missing)
  if ! sudo apt-get install -y "${APT_PACKAGES[@]}"; then
    warn "apt-get install returned non-zero. You may need to install some packages manually (e.g. cloudflared)."
  fi
}

enable_start_docker() {
  if command -v systemctl >/dev/null 2>&1; then
    info "Enabling and starting docker service via systemctl"
    sudo systemctl enable --now docker || warn "Failed enabling/starting docker via systemctl"
  else
    warn "systemctl not available — ensure docker daemon is running"
  fi
}

one_time_cleanup() {
  local marker="/home/$USER/.cleanup_done"
  if [ ! -f "$marker" ]; then
    info "Performing one-time cleanup in /home/$USER"
    sudo rm -rf "/home/$USER/.gradle/"* "/home/$USER/.emu/"* || true
    # remove all top-level entries except idx-ubuntu22-gui and dotfiles
    shopt -s extglob
    for entry in /home/"$USER"/*; do
      base=$(basename "$entry")
      if [ "$base" = "idx-ubuntu22-gui" ] || [[ "$base" == .* ]]; then
        continue
      fi
      sudo rm -rf "/home/$USER/$base" || true
    done
    sudo touch "$marker"
  else
    info "Cleanup already done (marker exists)"
  fi
}

create_or_start_container() {
  if ! command -v docker >/dev/null 2>&1; then
    err "Docker command not found. Please install Docker first."
    return 1
  fi

  if docker ps -a --format '{{.Names}}' | grep -qx "$CONTAINER_NAME"; then
    info "Container $CONTAINER_NAME exists — starting it"
    docker start "$CONTAINER_NAME" || {
      warn "docker start failed; attempting rm+create"
      docker rm -f "$CONTAINER_NAME" || true
    }
  fi

  # If after start it doesn't exist (or was removed), create it
  if ! docker ps -a --format '{{.Names}}' | grep -qx "$CONTAINER_NAME"; then
    info "Creating container $CONTAINER_NAME from image $DOCKER_IMAGE"
    docker run --name "$CONTAINER_NAME" \
      --shm-size 1g -d \
      --cap-add=SYS_ADMIN \
      -p "${HOST_PORT}:${CONTAINER_PORT}" \
      -e VNC_PASSWD="$VNC_PASSWD" \
      -e PORT="$CONTAINER_PORT" \
      -e AUDIO_PORT="$AUDIO_PORT" \
      -e WEBSOCKIFY_PORT="$WEBSOCKIFY_PORT" \
      -e VNC_PORT="$VNC_PORT" \
      -e SCREEN_WIDTH="$SCREEN_WIDTH" \
      -e SCREEN_HEIGHT="$SCREEN_HEIGHT" \
      -e SCREEN_DEPTH="$SCREEN_DEPTH" \
      "$DOCKER_IMAGE"
    info "Container created."
  else
    info "Container is now running/existing."
  fi
}

install_chrome_in_container() {
  # Run apt inside container to install chrome. Use non-interactive flags.
  info "Installing Chrome inside container (this may take a while)"
  docker exec -it "$CONTAINER_NAME" bash -lc "
    set -e
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -y || true
    apt-get remove -y firefox || true
    apt-get install -y wget gnupg lsb-release || true
    wget -O /tmp/chrome.deb https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb || true
    if [ -f /tmp/chrome.deb ]; then
      apt-get install -y /tmp/chrome.deb || true
      rm -f /tmp/chrome.deb
    else
      echo 'Chrome .deb not downloaded — skipping chrome install'
    fi
  " || warn "Chrome installation in container returned non-zero (continuing)"
}

start_cloudflared_tunnel() {
  # stop previous instance
  pkill -f "cloudflared tunnel --no-autoupdate --url http://localhost:${HOST_PORT}" || true
  rm -f "$CLOUDFLARED_LOG" || true
  info "Starting cloudflared tunnel -> http://localhost:${HOST_PORT} (logs -> $CLOUDFLARED_LOG)"
  nohup cloudflared tunnel --no-autoupdate --url "http://localhost:${HOST_PORT}" > "$CLOUDFLARED_LOG" 2>&1 &
  sleep 10
  if grep -q "trycloudflare.com" "$CLOUDFLARED_LOG" 2>/dev/null; then
    URL=$(grep -o "https://[a-z0-9.-]*trycloudflare.com" "$CLOUDFLARED_LOG" | head -n1 || true)
    if [ -n "$URL" ]; then
      echo "========================================="
      echo " 🌍 Your Cloudflared tunnel is ready:"
      echo "     $URL"
      echo "========================================="
    else
      warn "Couldn't extract URL from cloudflared logs though trycloudflare.com found."
      warn "Check $CLOUDFLARED_LOG"
    fi
  else
    warn "Cloudflared tunnel failed to produce trycloudflare URL. Check $CLOUDFLARED_LOG"
  fi
}

start_socat_preview() {
  # This mirrors idx.previews.novnc.command: socat TCP-LISTEN:$PORT,fork,reuseaddr TCP:127.0.0.1:8080
  info "Starting socat preview proxy listening on $SOCAT_PROXY_PORT -> 127.0.0.1:${HOST_PORT}"
  # run in background with nohup so main script continues; redirect logs
  nohup socat TCP-LISTEN:"$SOCAT_PROXY_PORT",fork,reuseaddr TCP:127.0.0.1:"$HOST_PORT" > /tmp/socat_novnc.log 2>&1 &
  info "socat started (logs -> /tmp/socat_novnc.log)"
}

print_elapsed_loop() {
  # prints elapsed minutes every 60s (same behaviour as original)
  info "Entering elapsed-time loop (Ctrl+C to exit)"
  elapsed=0
  while true; do
    printf "Time elapsed: %d min\n" "$elapsed"
    ((elapsed++))
    sleep 60
  done
}

main() {
  ensure_root
  apt_install_packages || warn "Some packages may not have been installed."

  enable_start_docker || warn "Could not enable/start docker automatically."

  one_time_cleanup

  create_or_start_container

  # Install chrome (best-effort)
  install_chrome_in_container

  start_cloudflared_tunnel

  start_socat_preview

  print_elapsed_loop
}

main "$@"
