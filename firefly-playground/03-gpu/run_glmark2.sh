#!/bin/bash
# 在图形会话下跑 GPU 基准
set -e
export DISPLAY="${DISPLAY:-:0}"

if ! command -v glmark2-es2 >/dev/null; then
  echo "请先: sudo apt install -y glmark2-es2"
  exit 1
fi

echo "DISPLAY=$DISPLAY"
glmark2-es2 --benchmark build --benchmark texture --benchmark shading --size 800x600
