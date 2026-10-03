#!/usr/bin/env python3
"""
SSD1306 128x64 I2C OLED 示例（需 luma.oled）。
接线: VCC=3.3V GND SDA SCL；地址常见 0x3c。
先用 i2c_scan.py / i2cdetect 确认总线号与地址。
"""
import time

try:
    from luma.core.interface.serial import i2c
    from luma.core.render import canvas
    from luma.oled.device import ssd1306
except ImportError:
    raise SystemExit("pip3 install --user luma.oled pillow")

I2C_PORT = 1      # 改成实际总线
I2C_ADDR = 0x3C


def main():
    serial = i2c(port=I2C_PORT, address=I2C_ADDR)
    device = ssd1306(serial)
    print("OLED OK, drawing ...")
    with canvas(device) as draw:
        draw.rectangle(device.bounding_box, outline="white")
        draw.text((10, 20), "Firefly RK3399", fill="white")
        draw.text((10, 36), "OLED hello", fill="white")
    time.sleep(10)
    print("done")


if __name__ == "__main__":
    main()
