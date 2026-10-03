# P4 第 9 周 — HiKey970 SocketCAN

> 上一阶段：战舰 NuttX 已能 `canspeed`(0x201 @ 500k) + `canlight`(0x202 控灯，loopback 自测)。  
> 本周：在 **HiKey970** 上把 Linux **SocketCAN** 拉起来；为第 10 周与 F103 真总线互通做准备。  
> **若 970 仍是安卓：** 先完成 [`week09-flash-linux.md`](week09-flash-linux.md)，再继续本文。

协议约定（与 NuttX 一致）：

| ID | 含义 | 数据 |
|----|------|------|
| **0x201** | 车速（F103 → 总线） | 2 字节 BE，km/h 计数 |
| **0x202** | 灯控（970 → F103） | 1 字节：bit0=LED0，bit1=LED1 |

波特率：**500000**。终端电阻、共地见 §3。

---

## 0. 本周验收

- [x] 970：已是 Linux；`ip link` 有 **`can0`**（2026-10-03，Lebian-9）  
- [x] 有线网：`enp6s0` → 例 `192.168.31.59`（可用 `ssh shunya@192.168.31.59`）  
- [x] `can-utils`；`can0` @ **500000**（驱动 **mcp251x**）  
- [x] loopback 自测：`cansend 202#03` → `candump` 收到（2026-10-03）  
- [ ] （第 10 周）关双方 loopback；CANH/CANL/GND 接战舰；终端电阻  

---

## 1. 登录 970

串口或 SSH（按你现有环境）：

```bash
# 例
ssh firefly@<970-IP>
# 或串口 minicom / screen
uname -a
```

确认是 Linux 用户态，后面命令默认在 **970** 上执行（需 `sudo` 的已写出）。

---

## 2. 找 CAN 网卡

```bash
ip link show
ip -details link show type can
ls /sys/class/net/
dmesg | grep -i can | tail -30
```

常见结果：

| 现象 | 含义 |
|------|------|
| 有 `can0` / `can1` | 内核已枚举 SocketCAN |
| 只有 `eth0`/`wlan0` | 驱动未开、设备树未启用、或 CAN 在扩展板上未接 |
| `vcan0` | 虚拟 CAN，**不能**直接连战舰物理总线 |

把实际接口名记下来，下文用 **`can0`**；若是 `can1` 请自行替换。

### 2.1 没有 can 口时

1. 查 Firefly / HiKey970 文档：CAN 引脚、是否要跳线/转接板、收发器型号  
2. 查设备树 / 内核配置是否启用对应 `m_can` / `flexcan` / 厂商 CAN  
3. 过渡方案：**PC + USB-CAN** 先与战舰互通，970 本周先跑通 `vcan` 熟悉 `candump`/`cansend` 命令（不算物理联调验收）

把 `ip link` / `dmesg` 关键贴出便于继续排查。

---

## 3. 装 can-utils

Lebian-9 = **Debian Stretch（已 EOL）**，`deb.debian.org` 会 404。先改归档源：

```bash
# 备份
sudo cp /etc/apt/sources.list /etc/apt/sources.list.bak

# 写成 stretch 归档（覆盖即可）
sudo tee /etc/apt/sources.list <<'EOF'
deb http://archive.debian.org/debian stretch main contrib non-free
deb http://archive.debian.org/debian-security stretch/updates main contrib non-free
EOF

# 忽略过期 Release 校验
echo 'Acquire::Check-Valid-Until "false";' | sudo tee /etc/apt/apt.conf.d/99no-check-valid-until

sudo apt update
sudo apt install -y can-utils
candump -h | head
cansend -h | head
```

### 3.1 970 仍下不了时：笔记本下载再 scp

在 **Ubuntu 笔记本**上：

```bash
wget http://archive.debian.org/debian/pool/main/c/can-utils/can-utils_0.0+git20161220-1_arm64.deb
scp can-utils_0.0+git20161220-1_arm64.deb shunya@192.168.31.59:~/
```

在 **970** 上：

```bash
sudo dpkg -i ~/can-utils_0.0+git20161220-1_arm64.deb
```

---

## 4. 拉起 CAN（500 kbit/s）

```bash
IF=can0   # 改成你的接口名

sudo ip link set "$IF" down
sudo ip link set "$IF" up type can bitrate 500000
ip -details link show "$IF"
```

期望看到 `state UP`（或 ERROR-ACTIVE）、`bitrate 500000`。

常用：

```bash
sudo ip link set "$IF" down          # 改参数前先 down
sudo ip link set "$IF" up type can bitrate 500000 restart-ms 100
```

---

## 5. 970 本机自测

### 5.1 虚拟 CAN（只练命令，不连板子）

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# 终端 A
candump vcan0

# 终端 B
cansend vcan0 202#03
cansend vcan0 201#000A
```

`candump` 应看到 `202 [1] 03` 一类输出。

### 5.2 物理 can0 环回（若驱动支持）

部分控制器支持：

```bash
sudo ip link set can0 down
sudo ip link set can0 up type can bitrate 500000 loopback on
```

再 `candump` + `cansend`。**不是所有芯片都支持**；不支持就用 USB-CAN 或直接进第 10 周双机。

### 5.3 与 USB-CAN（推荐本周物理验收）

1. PC 或 970 插 USB-CAN，按厂商说明出现 `can0`（或 `slcan0`）  
2. 双方 **500k**，CANH/CANL 对接，**共地**，总线两端各约 **120Ω**（板载已有则勿重复）  
3. 一端 `candump`，一端 `cansend 123#DEADBEEF`  

---

## 6. 与战舰联调前：关掉 NuttX 环回

战舰（Ubuntu 编 NuttX 的机器）上：

```bash
cd ~/automotive-nuttx/nuttx
grep CONFIG_CAN_LOOPBACK .config
# 若是 =y，改为关闭：
sed -i 's/^CONFIG_CAN_LOOPBACK=y/# CONFIG_CAN_LOOPBACK is not set/' .config
make -j$(nproc)
# 重烧战舰
```

环回开着时，帧常不上物理总线，970 **收不到**。

硬件：

| 线 | 接法 |
|----|------|
| CANH | 战舰 CANH ↔ 970 CANH |
| CANL | 战舰 CANL ↔ 970 CANL |
| GND | 共地 |
| 终端 | 总线两端 120Ω（战舰/模块板载则按原理图） |
| 战舰 | P9 跳线到 CAN；PA11/PA12 |

---

## 7. 命令速查（对齐 NuttX 协议）

### 970 收车速（F103 跑 `canspeed`）

```bash
# 970
candump can0,201:7FF

# 战舰 NSH（已关 loopback）
nsh> canspeed
```

`cansend`/`candump` 格式：`ID#PAYLOAD`（hex）。  
例：速度 10 → `201#000A`。

### 970 发灯控（F103 跑 `canlight`）

```bash
# 战舰
nsh> canlight

# 970
cansend can0 202#03    # 两灯亮
cansend can0 202#01    # 只 LED0
cansend can0 202#00    # 灭
```

### 记日志（第 10 周用）

```bash
candump -t a can0 | tee ~/can_log_$(date +%Y%m%d_%H%M%S).txt
```

或用本目录脚本：`./log_can.sh can0`。

---

## 8. 常见坑

| 现象 | 处理 |
|------|------|
| 无 can0 | 设备树/驱动/接线；见 §2.1 |
| UP 后 Bus-Off | 线序、波特率、终端、共地 |
| 970 收不到战舰 | 战舰仍开着 **LOOPBACK**；或 H/L 反了 |
| 战舰 TX 卡住 | 无第二节点 ACK——联调时应有 970 在线；单板测再开 loopback |
| 权限 | `sudo`，或把用户加入有权访问 net 的组 |
| 接口名不对 | `ip link` 为准，不要死记 can0 |

---

## 9. 本周结束时请贴回

```bash
uname -a
ip -details link show type can
# 以及一次成功的：
# candump 几行 或 cansend 自测说明
```

有 `can0` + 500k + dump/send 任一物理路径打通，即可进 **第 10 周 F103↔970**（见 `week10-f103-hikey-can.md`）。

---

## 10. 相关

| 路径 | 内容 |
|------|------|
| `week10-f103-hikey-can.md` | 双机联调与日志 |
| `log_can.sh` | candump 存文件 |
| `../nuttx-f103/week08-nuttx-can.md` | 战舰 CAN / canspeed / canlight |
| `../automotive-learning-plan.md` | P4 总计划 |
