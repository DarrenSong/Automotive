#!/usr/bin/env python3
"""
用 libgpiod 闪烁 GPIO。
先运行: gpiodetect && gpioinfo
把 CHIP / LINE 改成你的扩展脚。
"""
import time

try:
    import gpiod
except ImportError:
    raise SystemExit("请安装: sudo apt install python3-libgpiod")

# === 按板子修改 ===
CHIP = "gpiochip1"  # 例: gpiochip0 / gpiochip1
LINE = 17           # 该 chip 上的 line offset，不是物理丝印脚号


def main():
    chip = gpiod.Chip(CHIP)
    line = chip.get_line(LINE)
    line.request(consumer="firefly-blink", type=gpiod.LINE_REQ_DIR_OUT)
    print(f"Blinking {CHIP} line {LINE} ... Ctrl+C to stop")
    try:
        while True:
            line.set_value(1)
            time.sleep(0.5)
            line.set_value(0)
            time.sleep(0.5)
    except KeyboardInterrupt:
        line.set_value(0)
        print("\nbye")


if __name__ == "__main__":
    main()
