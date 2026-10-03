# P4 第 10 周 — 战舰 NuttX ↔ HiKey970 SocketCAN

> 前置：第 9 周 970 上 `can0` @ 500k 可用；战舰 `canspeed` / `canlight` 已会。  
> 目标：真总线双向通信 ≥10 分钟稳定，并留下一份原始日志。

---

## 0. 验收

- [ ] 战舰 **已关** `CONFIG_CAN_LOOPBACK`  
- [ ] 970：`can0` up，bitrate **500000**  
- [ ] 970 `candump` 能收到战舰 `0x201`（`canspeed`）  
- [ ] 970 `cansend 202#..` 能改战舰灯（`canlight` 守护已开）  
- [ ] 连续跑 ≥10 分钟无 Bus-Off（或记录一次 Bus-Off 原因）  
- [ ] 保存一份 `candump` 日志文件  

---

## 1. 接线检查清单

| 项 | 确认 |
|----|------|
| CANH ↔ CANH | |
| CANL ↔ CANL | |
| GND 共地 | |
| 终端电阻（两端约 120Ω，避免重复并联过多） | |
| 战舰 P9 → CAN | |
| 波特率双方 500k | |

---

## 2. 战舰侧（关环回后重烧）

```bash
# 编 NuttX 的 Ubuntu
cd ~/automotive-nuttx/nuttx
grep CONFIG_CAN_LOOPBACK .config
# 必须：# CONFIG_CAN_LOOPBACK is not set
```

NSH：

```text
nsh> canlight          # 收 0x202 控灯
nsh> canspeed          # 后台发 0x201（LPWORK）
```

可先只开一个方向测，再一起开。

---

## 3. 970 侧

```bash
IF=can0
sudo ip link set "$IF" down
sudo ip link set "$IF" up type can bitrate 500000
ip -details link show "$IF"
```

### 3.1 收车速

```bash
candump -t a "$IF",201:7FF
```

应对应 `canspeed` 递增 payload。

### 3.2 控灯

```bash
cansend "$IF" 202#03
cansend "$IF" 202#01
cansend "$IF" 202#00
```

战舰串口应有 `canlight: RX ...`，灯变化。

### 3.3 双向 + 记日志

```bash
cd ~/   # 或把 log_can.sh 拷到 970
# Windows 仓库: D:\Automotive\hikey970-socketcan\log_can.sh
./log_can.sh can0
```

另开一窗：

```bash
# 周期性控灯（示例）
while true; do
  cansend can0 202#03; sleep 1
  cansend can0 202#00; sleep 1
done
```

---

## 4. 故障注入（记录现象）

| 动作 | 预期/记录 |
|------|-----------|
| 拔掉 CANH 或 CANL | dump 中断；可能 Error/Bus-Off |
| 970 改成 bitrate 250000 | 战舰 TX 异常或 Bus-Off |
| 恢复 500k + 复位 can 口 | `ip link set down/up`；战舰可重启 canspeed |

Bus-Off 后 970 示例恢复：

```bash
sudo ip link set can0 down
sudo ip link set can0 up type can bitrate 500000 restart-ms 100
```

---

## 5. 日志里应有什么

至少包含：

- 时间戳  
- 接口名  
- ID `201` / `202`  
- payload  

例（`candump -t a`）：

```text
(123.456789) can0 201 [2] 00 0A
(123.557001) can0 202 [1] 03
```

文件建议：`can_log_YYYYMMDD_HHMMSS.txt`，拷回 Windows 仓库 `hikey970-socketcan/logs/`（可自建目录）。

---

## 6. 相关

| 文档 | 内容 |
|------|------|
| `week09-hikey970-can.md` | 970 单机 SocketCAN |
| `../nuttx-f103/week08-nuttx-can.md` | 战舰 CAN 应用 |
| `../automotive-learning-plan.md` | P4 |
