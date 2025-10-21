#!/bin/bash

CONTAINER_NAME="ubuntu-novnc"
NOVNC_PORT=6080

echo "🔍 Kiểm tra container Docker..."
if ! docker ps | grep -q "$CONTAINER_NAME"; then
  echo "⚠️ Container chưa chạy! Đang kiểm tra xem có tồn tại không..."
  if docker ps -a | grep -q "$CONTAINER_NAME"; then
    echo "➡️ Container tồn tại nhưng đang dừng. Khởi động lại..."
    docker start "$CONTAINER_NAME"
  else
    echo "❌ Không tìm thấy container có tên: $CONTAINER_NAME"
    exit 1
  fi
fi

echo "✅ Container đang chạy."

echo "🔍 Kiểm tra noVNC trong container..."
docker exec "$CONTAINER_NAME" bash -c "ps aux | grep -E 'websockify|novnc' | grep -v grep" > /tmp/novnc_process.txt

if [ -s /tmp/novnc_process.txt ]; then
  echo "✅ noVNC/websockify đang chạy:"
  cat /tmp/novnc_process.txt
else
  echo "❌ noVNC/websockify KHÔNG chạy. Đang thử khởi động thủ công..."
  docker exec -d "$CONTAINER_NAME" bash -c "/opt/novnc/utils/launch.sh --vnc localhost:5900 >/tmp/novnc_start.log 2>&1"
  sleep 3
  docker exec "$CONTAINER_NAME" bash -c "ps aux | grep websockify | grep -v grep" || echo "❌ Khởi động noVNC không thành công!"
fi

echo "🔍 Kiểm tra port $NOVNC_PORT có mở bên ngoài không..."
if ss -tulpn | grep -q ":$NOVNC_PORT"; then
  echo "✅ Port $NOVNC_PORT đang mở."
else
  echo "❌ Port $NOVNC_PORT không mở! Kiểm tra docker run -p 6080:80 hoặc -p 6080:10000"
fi

echo "🔍 Kiểm tra Cloudflared đang chạy không..."
if pgrep -x "cloudflared" > /dev/null; then
  echo "✅ Cloudflared đang chạy."
else
  echo "⚠️ Cloudflared không chạy. Bạn cần chạy lệnh:"
  echo "   cloudflared tunnel --url http://localhost:$NOVNC_PORT"
fi

echo "✅ Hoàn thành kiểm tra!"
