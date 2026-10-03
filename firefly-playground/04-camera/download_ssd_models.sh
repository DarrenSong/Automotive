#!/bin/bash
# 下载 MobileNet-SSD 模型到 models/
set -e
cd "$(dirname "$0")"
mkdir -p models
cd models

if [ ! -f MobileNetSSD_deploy.prototxt ]; then
  wget -O MobileNetSSD_deploy.prototxt \
    https://raw.githubusercontent.com/chuanqi305/MobileNet-SSD/master/deploy.prototxt
fi

if [ ! -f MobileNetSSD_deploy.caffemodel ]; then
  wget -O MobileNetSSD_deploy.caffemodel \
    https://github.com/chuanqi305/MobileNet-SSD/raw/master/mobilenet_iter_73000.caffemodel
fi

ls -lh
echo "OK"
