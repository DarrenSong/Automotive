# HiKey970 · SocketCAN（P4）

| 文档 | 内容 |
|------|------|
| [week09-flash-linux.md](week09-flash-linux.md) | **安卓 → Linux 刷机（当前先做这个）** |
| [week09-hikey970-can.md](week09-hikey970-can.md) | Linux 上启用 can0、can-utils、500k |
| [week10-f103-hikey-can.md](week10-f103-hikey-can.md) | 战舰 NuttX ↔ 970 双向 + 日志 |
| [log_can.sh](log_can.sh) | `candump -t a` 存文件 |

协议：`0x201` 车速，`0x202` 灯控，**500 kbit/s**。  
板子仍是安卓时先刷 Linux（本地镜像在 **`F:\Hikey970\hikey970-lebian-9.tar.gz`**）；联调战舰前关闭 NuttX `CONFIG_CAN_LOOPBACK`。
