# MQTT：Mosquitto + 发布/订阅

## 安装

```bash
sudo apt install -y mosquitto mosquitto-clients
pip3 install --user paho-mqtt
sudo systemctl enable --now mosquitto
```

允许本机匿名（仅局域网测试，勿对公网开放）：

```bash
sudo tee /etc/mosquitto/conf.d/local.conf << 'EOF'
listener 1883
allow_anonymous true
EOF
sudo systemctl restart mosquitto
```

## 运行

终端 1（订阅）：

```bash
python3 mqtt_sub.py
# 或: mosquitto_sub -h localhost -t 'firefly/#' -v
```

终端 2（发布）：

```bash
python3 mqtt_pub.py
# 或: mosquitto_pub -h localhost -t firefly/demo -m hello
```
