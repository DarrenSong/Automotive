#!/bin/bash
# 简单扫描附近经典蓝牙设备
set -e
sudo systemctl start bluetooth
bluetoothctl power on
bluetoothctl scan on &
SCAN_PID=$!
echo "Scanning 15s ..."
sleep 15
bluetoothctl scan off
kill $SCAN_PID 2>/dev/null || true
bluetoothctl devices
