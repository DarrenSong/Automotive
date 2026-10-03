#!/bin/bash
# Firefly-RK3399 Ubuntu 20.04 依赖安装
set -e
sudo apt update

echo "==> 基础"
sudo apt install -y python3-pip python3-dev git v4l-utils i2c-tools

echo "==> MQTT"
sudo apt install -y mosquitto mosquitto-clients
pip3 install --user paho-mqtt

echo "==> GPIO / I2C / SPI"
sudo apt install -y python3-libgpiod gpiod
pip3 install --user smbus2 spidev pillow luma.oled

echo "==> GPU"
sudo apt install -y glmark2-es2

echo "==> OpenCV 摄像头"
sudo apt install -y python3-opencv python3-numpy
# Haar 级联随 opencv 数据文件提供；YOLO 轻量版见 04-camera

echo "==> 蓝牙"
sudo apt install -y bluez bluez-tools
pip3 install --user bleak

echo "==> 完成。建议把用户加入拨号/GPIO 相关组后重新登录："
echo "    sudo usermod -aG dialout,i2c,spi,gpio,video,bluetooth \$USER"
echo "    （若某组不存在可忽略报错）"
