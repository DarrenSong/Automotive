# GPIO / I2C / SPI

Firefly-RK3399 **没有板载用户 LED 脚位文档统一编号**，扩展口 GPIO 编号因内核/设备树而异。先探测，再接线。

## 探测

```bash
# GPIO 芯片
gpiodetect
gpioinfo

# I2C 总线（OLED / 温湿度常用）
ls /dev/i2c-*
sudo i2cdetect -y 1   # 总线号按实际改：0/1/2/4/7...

# SPI
ls /dev/spidev*
```

把用户加入组（组名因系统而异，不存在可跳过）：

```bash
sudo usermod -aG gpio,i2c,spi $USER
# 重新登录生效
```

## 示例脚本

| 文件 | 说明 |
|------|------|
| `blink_gpio.py` | 用 libgpiod 翻转一个 GPIO（需改 chip/line） |
| `i2c_scan.py` | 扫描 I2C 地址 |
| `oled_ssd1306.py` | SSD1306 0.96" I2C OLED 显示文字（需硬件） |

## 接线提示（常见）

- **LED**：GPIO → 限流电阻 220Ω → LED → GND  
- **SSD1306**：VCC 3.3V，GND，SDA/SCL 接到对应 I2C  
- **SPI OLED / 传感器**：确认 3.3V 逻辑，不要直接接 5V 设备到 GPIO  

芯片与 line 号用 `gpioinfo` 查到后再改脚本顶部常量。
