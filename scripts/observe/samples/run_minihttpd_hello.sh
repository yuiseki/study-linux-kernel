#!/usr/bin/bash
set -euo pipefail

# minihttpd を起動して 1 リクエスト送って終了させる
env -i PATH=/usr/bin:/bin \
  /usr/bin/bash -c '
    ./minihttpd/minihttpd &
    pid=$!
    sleep 0.1
    /usr/bin/curl -s http://localhost:8080/ > /tmp/minihttpd_out
    kill "$pid"
    wait "$pid" 2>/dev/null || true
  '
