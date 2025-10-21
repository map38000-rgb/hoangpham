#!/bin/bash

echo "🔧 Bắt đầu quá trình kiểm tra & sửa lỗi noVNC + Cloudflare..."

# 1. Kiểm tra Docker
echo "🔍 Kiểm tra Docker..."
if ! command -v docker &> /dev/null; then
    echo "⚠ Docker chưa cài đặt. Đang cài đặt..."
    apt update && apt install -y docker.io
fi

# 2. Kiểm tra container noVNC
echo "🔍 Kiểm tra container noVNC..."
if ! docker ps | grep -q "ubuntu-novnc"; then
    echo "⚠ Container không chạy. Đang khởi động lại..."
    docker run -d --name ubuntu-novnc -p 6080:6080 -p 5900:5900 \
        --restart always thuonghai2711/ubuntu-novnc-pulseaudio:22.04
else
    echo "✅ Container đang chạy."
fi

# 3. Kiểm tra noVNC có chạy đúng port 6080 không?
echo "🔍 Kiểm tra dịch vụ noVNC..."
if ! ss -ltnp | grep -q ":6080"; then
    echo "⚠ Port 6080 chưa hoạt động, đang khởi động noVNC..."
    /usr/share/novnc/utils/novnc_proxy --vnc localhost:5900 --listen 6080 &
else
    echo "✅ Port 6080 đang mở."
fi

# 4. Cài Cloudflared nếu chưa có
echo "🔍 Kiểm tra Cloudflared..."
if ! command -v cloudflared &> /dev/null; then
    echo "⚠ Cloudflared chưa cài. Đang cài..."
    wget https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb
    dpkg -i cloudflared-linux-amd64.deb || apt install -f -y
fi

# 5. Restart Cloudflare Tunnel
echo "🔁 Đang khởi động lại Cloudflare Tunnel..."
pkill cloudflared 2>/dev/null
nohup cloudflared tunnel --url http://localhost:6080 --no-autoupdate > cloudflare.log 2>&1 &

sleep 3
echo "✅ Cloudflare Tunnel đã khởi động lại!"

# 6. Lấy URL public
echo "🌐 Đang lấy Public URL từ Cloudflare:"
PUBLIC_URL=$(grep -o 'https://[a-zA-Z0-9.-]*trycloudflare.com' cloudflare.log | head -n 1)

if [ -n "$PUBLIC_URL" ]; then
    echo "✅ Truy cập noVNC tại:"
    echo "$PUBLIC_URL/vnc.html?autoconnect=true"
else
    echo "⚠ Không lấy được URL! Kiểm tra bằng: tail -f cloudflare.log"
fi

echo "✅ Hoàn tất! noVNC + Cloudflare đã sẵn sàng."
