#!/usr/bin/env python3
"""扫描 I2C 总线上的设备地址。"""
import glob
import sys

try:
    from smbus2 import SMBus
except ImportError:
    raise SystemExit("pip3 install --user smbus2")


def scan(bus_id: int):
    print(f"--- /dev/i2c-{bus_id} ---")
    found = []
    with SMBus(bus_id) as bus:
        for addr in range(0x03, 0x78):
            try:
                bus.read_byte(addr)
                found.append(addr)
            except OSError:
                pass
    if found:
        print(" ".join(f"0x{a:02x}" for a in found))
    else:
        print("(no devices)")


def main():
    buses = sorted(int(p.split("-")[-1]) for p in glob.glob("/dev/i2c-*"))
    if not buses:
        print("没有 /dev/i2c-* ，检查设备树/驱动是否启用 I2C")
        sys.exit(1)
    targets = [int(sys.argv[1])] if len(sys.argv) > 1 else buses
    for b in targets:
        try:
            scan(b)
        except PermissionError:
            print(f"i2c-{b}: 权限不足，试 sudo 或加入 i2c 组")
        except FileNotFoundError:
            print(f"i2c-{b}: 不存在")


if __name__ == "__main__":
    main()
