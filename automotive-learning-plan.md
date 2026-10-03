# 车载嵌入式学习计划（从 MCU 起）

硬件：
- 正点原子 **战舰 STM32F1 V3（F103ZET6 + TJA1050）** — MCU 主学板  
- **STM32F103VET6 工控板/MINI**（板载 TJA1050，CAN 脚 **PB8/PB9 重映射**）— 第二 CAN 节点（线缆补齐后）  
- **ESP32-S3** — Wi‑Fi/BLE/桥接辅助（非本阶段主线）  
- HiKey970 · Firefly RK3399 — Linux / DDS / gPTP  

资料：
- 战舰：`F:\stm32\a盘` · `F:\stm32\b盘` · `F:\stm32\新下载`  
- 工控板：`D:\BaiduNetdiskDownload\STM32F103VET6工控板（新）`  
- 踩坑笔记：`stm32-warship-troubleshooting.md`  

节奏：业余每天约 1–2 小时 ≈ **16 周**；全职可压缩到 8–10 周。  
原则：**先 MCU，再 Linux CAN，再 DDS，再 gPTP，最后综合**；同时最多推进一个主阶段。  
**第 1 周逐日清单：** 见 `week01-warship-f103.md`（库函数路线）。

---

## 总路线

```text
P0 准备
 → P1 STM32 裸机 + CAN
 → P2 Bootloader / IAP
 → P3 NuttX on F103
 → P4 HiKey970 SocketCAN 联调
 → P5 RK3399 DDS（+ CAN→DDS 桥）
 → P6 gPTP（RK3399 ↔ 970）
 → P7 三板毕业项目
```

| 阶段 | 周 | 主题 | 主硬件 |
|------|----|------|--------|
| P0 | 0 | 准备与地图 | 三块板 |
| P1 | 1–3 | 裸机 + CAN | F103 |
| P2 | 4–5 | Bootloader | F103 |
| P3 | 6–8 | NuttX | F103 |
| P4 | 9–10 | SocketCAN 联调 | 970 + F103 |
| P5 | 11–13 | DDS | RK3399 (+970) |
| P6 | 14–15 | gPTP | RK3399 + 970 |
| P7 | 16+ | 综合项目 | 三块板 |

---

## 必备配件

- [x] USB 转串口、ST-Link（战舰已通，Under reset）
- [ ] 杜邦线、面包板（可选）
- [ ] CAN 双绞线 / 端子线（**战舰↔工控板** 普通模式用）
- [ ] CAN 终端电阻 120Ω（工控板若已板载则战舰侧按原理图确认）
- [ ] 工控板下载线（J-Link 扁线或 SWD，以实板丝印为准）与合规电源
- [ ] （可选）USB-CAN — 无第二块 MCU 时用 PC 抓总线
- [ ] 网线 + 交换机（DDS / gPTP，Linux 阶段）
- [ ] （可选）示波器

**说明：** 战舰环回已完成，不依赖第二节点。工控板资料显示板载 TJA1050，CAN 为 **PB8/PB9 Remap**；线缆齐后再做双机。ESP32-S3 不占用 P1 主线。

确认 F103 板：**战舰与工控板均有 TJA1050**。

---

## P0 — 第 0 周：准备

### 任务
- [ ] 记录三块板型号、接口照片、电源规格
- [ ] F103：装 CubeIDE 或 Keil / arm-none-eabi-gcc + OpenOCD
- [ ] F103：成功烧录厂家 Blink / 串口例程
- [ ] 970 / RK3399：SSH 或串口登录确认
- [ ] 新建笔记仓库（Git）：每周验收截图、引脚表、问题记录
- [ ] 手绘最终目标框图（CAN + 以太网如何连）

### 验收
三块板都能进系统/烧录；笔记仓库建好；框图一页纸说得清。

### 本周阅读
- STM32F103 数据手册：引脚、CAN 外设有无
- 正点原子对应板「简介 / 原理图」CAN 部分

---

## P1 — 第 1–3 周：STM32 裸机 + CAN

### 第 1 周 — 裸机基础
**每天建议**
- Day1–2：时钟树、GPIO 输出、点灯
- Day3–4：USART 打印、SysTick / TIM 周期任务
- Day5–6：中断优先级、启动文件与向量表（只读懂）
- Day7：整理工程模板（可复用的最小工程）

**验收**
- [ ] 独立工程：LED 1Hz + 串口每秒打印
- [ ] 能说明：复位后从哪开始执行

### 第 2 周 — CAN 收发
**每天建议**
- Day1：CAN 物理层（显性/隐性、终端电阻、双绞线）
- Day2：CAN 初始化（建议 500 kbit/s）、环回模式
- Day3：标准数据帧发送；固定 ID（如 0x100）
- Day4：接收中断 / 轮询；邮箱与 FIFO
- Day5：过滤器（只收关心的 ID）
- Day6：周期报文（模拟车速 0x201）
- Day7：笔记：帧格式图 + 自己的 ID 分配表

**验收**
- [ ] 自发自收或双节点互通
- [ ] 串口打印出 ID、DLC、数据

### 第 3 周 — 总线调试与“车载味”
**每天建议**
- Day1：整理环回验收笔记；接收打印 StdId（若未做完）
- Day2：读工控板必读 PDF + 原理图 CAN 页；列出缺的线缆采购清单
- Day3～4：线缆齐后 — 战舰 Normal ↔ 工控板例程，500k 互通
- Day5：USB-CAN 或第二节点抓帧；错波特率 / Bus-Off 观察
- Day6：ID 分配表 + 简单超时逻辑
- Day7：复习本阶段，补漏

**验收**
- [x] 战舰环回自发自收（串口可见）
- [ ] 双节点或 USB-CAN 稳定抓到帧（线缆齐后）
- [ ] 有一份 ID 分配表（即使只有 2～3 个 ID）

---

## P2 — 第 4–5 周：Bootloader / IAP

### 第 4 周 — 启动与跳转
- [x] Flash 分区：命名 **bootloader** + **app_boot**；现为 bootloader **32KB** @ `0x08000000`，app_boot @ `0x08008000`
- [x] 修改 app_boot 链接（IROM / scatter）与向量表偏移（`VTOR`）
- [x] bootloader：检查栈顶/复位向量合法性 → 跳转 app_boot
- [x] 按键：KEY0 / WK_UP 强制进 bootloader（2s 窗口）
- [x] 串口区分 bootloader / app_boot

**验收**
- [x] 16KB 分区跳转曾通过
- [x] **32KB**：上电跳 app_boot（`Jump to APP` / `enter app`）；按住 KEY0 → `Force BOOTLOADER` / `in BOOTLOADER`

详解文档：`bootloader/bootloader-jump-notes.md`

### 第 5 周 — 升级协议
- [x] 定义包格式：`A5 5A` + CMD/SEQ/LEN/PAYLOAD/CRC16（`bootloader/iap-demo-readme.md`）
- [x] bootloader：擦除 APP 区 → 写 Flash → 回读校验 → JUMP
- [x] PC 侧 Python：`bootloader/iap_uart_host.py`
- [x] 失败不跳（JUMP 前校验向量表）；KEY0 强制进 IAP
- [x] 同一协议改到 **CAN IAP**（板端 `iap_can.c` + `iap_can_host.py`；**待 USB-CAN 实测**）

**验收**
- [x] 不插调试器，仅用串口升级 app_boot 成功（INFO/ERASE/WRITE/JUMP）
- [x] 升级步骤：`bootloader/iap-demo-readme.md`
- [ ] CAN IAP 真机升级成功（有工具后再勾）

---

## P3 — 第 6–8 周：NuttX on F103

起步文档：`nuttx-f103/README.md`（交叉编译：**Ubuntu 24 laptop / 用户 darren**；Windows 负责烧录与串口）

### 第 6 周 — 编译与启动
- [x] 获取 Apache NuttX + nuttx-apps（Ubuntu24 / darren）
- [x] 选 `stm32f103-minimum:nsh`（USART1；后续再改 ZET6 容量）
- [x] 交叉编译、烧录（覆盖 bootloader，预期行为）
- [x] 串口进入 **NSH**（NuttX-13.1.0-RC0）

**验收**
- [x] 出现 `nsh>`（串口助手见 ANSI `^[K` 属正常，见 `nuttx-f103/README.md`）
- [x] NSH：`help`、`uname -a` 通过（`free` 在 minimum 默认未启用，属正常）

### 第 7 周 — RTOS 编程
文档/示例：`nuttx-f103/week07-rtos-task.md`，work queue：`week07-workqueue-timer.md`，原理 `week07-nuttx-build-boot-sched.md`  
代码：`apps-overlay/examples/ledblink/`、`apps-overlay/examples/wqblink/`

- [x] 创建用户任务：周期打印 `ledblink`（USERLED 未开，printf-only）
- [x] 理解任务优先级、栈、调度与裸机差异（`usleep` vs `delay_ms`）
- [x] 定时器 / work queue：`wqblink`（LPWORK 周期回调，见 `week07-workqueue-timer.md`）
- [x] 战舰 PB5 + `CONFIG_USERLED`（灯闪已验）
- [x] 双灯 PB5+PE5：`ledblink` 轮流 / `ledblink same` 同闪（踩坑见 `nuttx-f103/week07-two-leds.md`）

**验收**
- [x] NSH 下 `ledblink` 周期输出 tick（2026-10-03）
- [x] 双灯验收（2026-10-03）：ZE + USERLED_LOWER；opposite / same 均通过
- [x] `wqblink`：main 返回后仍周期 tick；NSH 可并行（2026-10-03）；`wqblink stop` 自测一下即可

### 第 8 周 — NuttX + CAN 应用
文档/示例：`nuttx-f103/week08-nuttx-can.md`，代码 `nuttx-f103/apps-overlay/examples/canspeed/`

- [x] 打开 CAN 驱动配置（`STM32_CAN1` + `CAN`；`canspeed` 已能 TX）
- [x] 板级 `/dev/can0`；`canspeed` 周期发 **0x201**（2026-10-03）
- [x] 波特率 **500000**（`tseg1=6 tseg2=5`，2026-10-03）
- [x] 后台 LPWORK TX（不堵 NSH；open 在 worker 内，避免 EBADF）
- [x] 收帧控灯：`canlight`（ID **0x202** → USERLED；loopback 自测通过，2026-10-03）
- [ ] （加分）与 Boot 分区策略协调（NuttX 镜像放 APP 区）

**验收**
- [x] NSH：`canspeed` open/write 成功（2026-10-03）
- [x] `canspeed` 打印 `baud=500000`（2026-10-03）
- [x] `canlight`：TX→RX→灯 mask 0x3/0x1/0x0（需 `CONFIG_CAN_LOOPBACK=y`）
- [ ] （加分）关 loopback + 外部节点发 0x202 / 抓 0x201

---

## P4 — 第 9–10 周：Linux SocketCAN 联调

文档：`hikey970-socketcan/week09-hikey970-can.md`、`week10-f103-hikey-can.md`

### 第 9 周 — HiKey970 CAN
文档：`hikey970-socketcan/week09-flash-linux.md`（安卓→Linux）、`week09-hikey970-can.md`

- [x] **刷成 Linux**（Lebian-9 / 4.9.78，2026-10-03）
- [x] 确认有 **`can0`**（当前 DOWN；以太网 `192.168.31.59`）
- [x] `can-utils`；`can0` @ 500k（mcp251x）；loopback 自测 `202#03` 通过（2026-10-03）

**验收**
- [x] 970：loopback 下 `cansend`/`candump` 互通
- [ ] 关 loopback 后与战舰真总线互通（第 10 周）

### 第 10 周 — F103 ↔ 970
- [ ] 战舰关闭 `CONFIG_CAN_LOOPBACK` 并重烧
- [ ] 统一波特率 500k、终端电阻、共地
- [ ] NuttX `canspeed`↔970 `candump`；970 `cansend 202#`↔`canlight`
- [ ] `log_can.sh` / `candump -t a` 存日志；故障注入记录现象

**验收**
- [ ] 双向稳定 ≥10 分钟无 Bus-Off
- [ ] 有原始日志文件一份

---

## P5 — 第 11–13 周：DDS

### 第 11 周 — RK3399 HelloWorld
- [ ] 在 RK3399 安装 Fast DDS 或 Cyclone DDS（或 ROS2 自带 DDS）
- [ ] 跑官方 Publisher / Subscriber
- [ ] 理解：Domain、Topic、Type、Discovery

**验收**
- [ ] 本机两个进程 topic 互通

### 第 12 周 — QoS 与双机
- [ ] 试验 Reliability、History、Deadline（各改一次看现象）
- [ ] 970 或第二终端作第二节点（同局域网）
- [ ] 记录：防火墙 / 组播不通时的排查步骤

**验收**
- [ ] 两台机器 DDS 互通

### 第 13 周 — CAN → DDS 桥
- [ ] RK3399：读 SocketCAN（USB-CAN）或收 970 转发的 UDP/串口
- [ ] 解析车速/开关 → 发布 `VehicleSpeed` / `BodyLight` topic
- [ ] （可选）简易订阅打印或网页显示

**验收**
- [ ] 拧 F103「模拟油门」或改数据，DDS 订阅端数值跟着变

---

## P6 — 第 14–15 周：gPTP

### 第 14 周 — 概念 + 单机
- [ ] 阅读：802.1AS / gPTP 角色（GM、Slave）、与 NTP 差别
- [ ] `sudo apt install linuxptp`
- [ ] `ptp4l -i eth0 -m` 看日志字段含义
- [ ] 查网卡是否支持硬件时间戳（`ethtool -T eth0`）

**验收**
- [ ] 能解释 ptp4l vs phc2sys；知道自己是硬件还是软件打戳

### 第 15 周 — 双机对时
- [ ] RK3399 作 Master，970 作 Slave（或反之）
- [ ] 直连或经交换机；记录 `offset` / `freq` 收敛曲线
- [ ] 对比：无 PTP 时应用层时间差 vs 有 PTP
- [ ] 笔记：汽车里为何需要时间同步（相机、雷达、控制）

**验收**
- [ ] 双机 offset 进入「可解释的稳定范围」（软件打戳可能是数十微秒～毫秒级，如实记录）

---

## P7 — 第 16 周+：毕业项目

### 目标架构
```text
[F103 NuttX] --CAN--> [HiKey970 SocketCAN]
                          |
                     以太网/桥接
                          v
                   [RK3399 DDS + UI]
                   [RK3399 gPTP GM] <--gPTP--> [970 Slave]
[F103 Boot] <--CAN IAP--> 升级灯控 APP
```

### 验收清单
- [ ] F103：NuttX 应用 + 可 CAN/UART 升级
- [ ] F103↔970：CAN 车速/灯控互通
- [ ] RK3399：DDS 订阅可见
- [ ] RK3399↔970：gPTP 日志
- [ ] 文档：接线照片、波特率、ID 表、topic 名、启动命令
- [ ] 5 分钟演示可复现（自己按文档再走一遍）

---

## 每周节奏模板（可复制）

```text
周一：读文档 / 看例程（理论）
周二–周四：动手（主任务）
周五：联调与修 bug
周六：写笔记 + 验收勾选
周日：缓冲 / 补漏（不新开大坑）
```

---

## 明确不做（避免分心）

- F103 上硬啃完整 DDS / gPTP  
- 一上来 AUTOSAR 全栈商用工具链  
- 同时大改 970 内核 / 重刷系统（联调阶段优先稳）  
- 无 USB-CAN、无第二节点就宣称「CAN 学会了」

---

## 阶段完成后的能力画像

| 阶段完成后你应能… |
|-------------------|
| P1 | 讲清 CAN 帧，并用 MCU 稳定收发 |
| P2 | 讲清启动链，并做最小 IAP |
| P3 | 在 RTOS 里做 CAN 应用任务 |
| P4 | Linux 与 MCU 真总线联调 |
| P5 | 用 DDS 传车载信号并懂 QoS |
| P6 | 用 linuxptp 做主从对时并解释结果 |
| P7 | 端到端演示一条「 ECU → 域控」链路 |

---

**下一步行动（本周就做）：** 勾选 P0，确认 F103 有无 CAN 收发器，并把结果发我——有收发器则直接给 **第 1 周逐日命令/工程清单**，没有则先列模块购买与接线。
