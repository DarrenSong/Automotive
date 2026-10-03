#!/usr/bin/env python3
"""扫描附近 BLE 设备（需 bleak，且用户有蓝牙权限）。"""
import asyncio
import sys

try:
    from bleak import BleakScanner
except ImportError:
    raise SystemExit("pip3 install --user bleak")


async def main():
    timeout = float(sys.argv[1]) if len(sys.argv) > 1 else 8.0
    print(f"BLE scanning {timeout}s ...")
    devices = await BleakScanner.discover(timeout=timeout)
    if not devices:
        print("未发现设备（检查蓝牙是否开启、天线、权限）")
        return
    for d in sorted(devices, key=lambda x: x.rssi or -999, reverse=True):
        name = d.name or "(unknown)"
        print(f"{d.address}  RSSI={d.rssi!s:>4}  {name}")


if __name__ == "__main__":
    asyncio.run(main())
