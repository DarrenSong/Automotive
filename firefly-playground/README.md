# Firefly-RK3399 Playground

面向 Firefly-RK3399（Ubuntu 20.04）的折腾示例：MQTT、GPIO/I2C/SPI、GPU、摄像头、蓝牙。

## 一键装依赖（在板子上执行）

```bash
cd ~
# 把本目录 scp 到板子后：
cd ~/firefly-playground
chmod +x install.sh
./install.sh
```

或手动按模块安装，见各子目录 README。

## 目录

| 目录 | 内容 |
|------|------|
| `01-mqtt/` | Mosquitto + Python 发布/订阅 |
| `02-gpio-i2c-spi/` | LED、I2C 传感器、SPI OLED |
| `03-gpu/` | glmark2-es2 测 GPU |
| `04-camera/` | OpenCV Haar 人脸 / 轻量 YOLO |
| `05-bluetooth/` | 蓝牙配对与 BLE 扫描 |

## 拷到板子

在 Windows PowerShell（IP 按实际修改）：

```powershell
scp -r D:\Automotive\firefly-playground firefly@192.168.31.231:~/
```

Wi‑Fi IP 示例：`192.168.31.231`；有线：`192.168.31.27`。
