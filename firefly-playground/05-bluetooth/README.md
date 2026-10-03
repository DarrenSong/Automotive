# 蓝牙 / BLE

板载 AP6356S 含 Bluetooth 4.1。

## 安装

```bash
sudo apt install -y bluez bluez-tools
pip3 install --user bleak
sudo systemctl enable --now bluetooth
sudo usermod -aG bluetooth $USER   # 重新登录
```

## 经典蓝牙（手机配对）

```bash
bluetoothctl
# 进入交互后：
power on
agent on
default-agent
scan on
# 找到手机后:
pair XX:XX:XX:XX:XX:XX
trust XX:XX:XX:XX:XX:XX
connect XX:XX:XX:XX:XX:XX
```

手机需处于可被发现模式。也可：

```bash
./bt_scan.sh
```

## BLE 扫描传感器

```bash
python3 ble_scan.py
```

会列出附近 BLE 设备名称与地址；温度计/手环等多为 BLE。拿到地址后可用 `bleak` 进一步读特征值（不同厂商 UUID 不同）。

## 注意

- 天线要接好（与 Wi‑Fi 同模块）  
- Wi‑Fi 与 BT 共存一般可用，干扰大时可暂时关 Wi‑Fi 测 BT  
- 部分桌面还会占 adapter，命令行优先用 `bluetoothctl`
