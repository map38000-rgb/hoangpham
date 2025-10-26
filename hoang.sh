#!/bin/bash
set -euo pipefail

while true; do
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] ping 9.9.9.9 (4 packets)"
  ping -c 4 9.9.9.9
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] done. Sleeping 120s..."
  sleep 120
done
