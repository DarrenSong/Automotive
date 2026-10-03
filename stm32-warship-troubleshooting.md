# STM32 / 车载入门踩坑与问答整理

> 主学板：正点原子 **战舰 STM32F1 V3**（STM32F103ZET6 + TJA1050）  
> 工具：Keil MDK + ST-Link V2 + STM32CubeProgrammer  
> 战舰资料：`F:\stm32\a盘`  
> 工控板资料：`D:\BaiduNetdiskDownload\STM32F103VET6工控板（新）`  
> 时间：约 2026-10-01 起  

本文记录真实遇到的问题、原因和解决办法，方便以后复查。

---

## 0. 手头硬件总览（更新）

| 板子 | 角色 | 资料/备注 |
|------|------|-----------|
| 战舰 F103ZET6 | **主学**：裸机、CAN、Boot、NuttX | `F:\stm32\a盘`；CAN=PA11/PA12，P9 跳线 |
| STM32F103VET6 工控板（MINI） | **第二 CAN 节点** / RS485·RS232 工控接口 | 见 §11；缺线缆暂缓上电联调 |
| ESP32-S3 | Wi‑Fi/BLE、以后做 MQTT/无线桥 | **不挤占**当前 CAN/Boot 主线 |
| Firefly RK3399 | Linux 桌面、DDS、gPTP | Ubuntu 已刷好 |
| HiKey970 | SocketCAN、第二台 Linux | 有板载 CAN |

**近期策略：** 战舰继续吃透环回 + 笔记；工控板线缆（杜邦/端子 CAN 线、下载线、电源）齐了再做 **战舰↔工控板** 普通模式双节点；ESP32-S3 留到 DDS/MQTT 阶段再用。

---

## 1. 环境与资料对应

| 项目 | 内容 |
|------|------|
| 开发板 | ALIENTEK 战舰 V3，板载 TJA1050（CAN） |
| LED | DS0 → **PB5**（低电平亮）；DS1 → **PE5** |
| 串口 | USART1，PA9/PA10 → CH340，**115200 8N1** |
| CAN | PA11=RX，PA12=TX；跳线 **P9** 接到 CAN |
| 主跟例程 | `a盘\4，程序源码\2，标准例程-库函数版本\` |
| 学习计划 | `D:\Automotive\automotive-learning-plan.md` |
| 第 1 周清单 | `D:\Automotive\week01-warship-f103.md` |

**工具选型结论**

- 跟正点原子库函数例程：先用 **Keil**  
- 烧录排查 / ST-Link 固件：用 **STM32CubeProgrammer**  
- **STM32CubeIDE**：写代码+编译+调试的 IDE；和 Programmer 不是同一个东西  
- **ST-Link Utility**：旧工具（NRND），优先用 CubeProgrammer  

官方下载：

- CubeProgrammer：https://www.st.com/en/development-tools/stm32cubeprog.html  
- CubeIDE：https://www.st.com/en/development-tools/stm32cubeide.html  
- ST-Link 驱动：https://www.st.com/en/development-tools/stsw-link009.html  
- ST-Link Utility（旧）：https://www.st.com/en/development-tools/stsw-link004.html  

本地已有安装包目录：`F:\stm32\`（含 cubeprg / cubeide / stsw-link004 等 zip）。

---

## 2. ST-Link 连不上（重点）

### 2.1 现象分层

要把两件事分开：

| 层级 | 含义 | 当时状态 |
|------|------|----------|
| PC ↔ ST-Link | 电脑认不认调试器 | 设备管理器有 `STM32 STLink`；后来能升级固件 |
| ST-Link ↔ MCU | 调试器能不能访问芯片 | 长期失败：`Unable to get core ID` / `No STM32 target found` |

**只升级 ST-Link 固件成功 ≠ 已经连上战舰芯片。**

### 2.2 CubeProgrammer 日志要点（`D:\Automotive\stlinkerror.log`）

```text
ST-LINK SN  : 57FF6E067266535113420167
ST-LINK FW  : V2J45S7
Voltage     : 3.25V
Error: Unable to get core ID
Error: No STM32 target found!
```

解读：

- SN / FW 有了 → **ST-Link 本身 OK**  
- Voltage ≈ 3.3V → 目标板大致有电  
- Unable to get core ID → **SWD 没通到 CPU**（接线/复位方式/BOOT 等）

### 2.3 其它表象

- 老版 **ST-Link Upgrade** 报：`No ST-Link device detected`（工具太旧或占用），改用 **CubeProgrammer** 升级固件即可。  
- Keil Debug Settings 里出现：`ST-LINK USB communication error`，SW Device 空白；在 **目标芯片都连不上** 时改 Keil 没用，先保证 CubeProgrammer 能 Connect。

### 2.4 最终能连上的关键设置

在 **STM32CubeProgrammer**（以及之后 Keil 对齐）中：

1. 接口：**SWD**  
2. Mode：**Under reset**（在复位下连接）  
3. Reset mode：**Hardware reset**（硬件复位）  
4. 频率可先降到 **100 kHz** 再试  
5. **NRST 线建议接到战舰 NRST**（Under reset / Hardware reset 很依赖这根线）

Keil 对应位置：

`Options for Target` → **Debug** → ST-Link Debugger → **Settings**：

- Port: **SW**  
- Connect: 优先 **Under Reset**（不要只用 Normal）  
- Reset: **Hardware Reset** / Autodetect（以能识别 SW Device 为准）  
- Max Clock: 可先 **1.5MHz** 或 **100kHz**  
- Utilities：勾选用 Target Driver 下载，驱动也选 ST-Link  

用完 CubeProgrammer 后先 **Disconnect 并退出**，再开 Keil，避免占用 ST-Link。

### 2.5 接线检查清单（ST-Link V2 ↔ 战舰 SWD）

| ST-Link | 战舰 |
|---------|------|
| SWDIO | SWDIO |
| SWCLK | SWCLK |
| GND | GND（必须共地） |
| 3.3V | 3.3V（板子已 USB 供电时可先不接 3.3V，只共地） |
| NRST | NRST（强烈建议） |

其它：

- BOOT0 = 0（GND），正常启动，不要停在 ISP  
- 数据线要能传数据；少用劣质线/扩展坞  
- SWDIO/SWCLK 不能接反，不要接到 USART/CAN 口上  

### 2.6 问答摘要（ST-Link）

**Q：设备管理器有 STM32 STLink，为何 Upgrade 工具说检测不到？**  
A：旧 Upgrade 工具兼容性差；用 CubeProgrammer 做固件升级。

**Q：CubeProgrammer 能升级 ST-Link，Keil 仍 communication error？**  
A：先确认 CubeProgrammer 对**芯片**能否 Connect。若不能，是 SWD/复位问题；若能，再关占用、对齐 Keil 的 Under Reset / Hardware Reset。

**Q：Under reset 和 Hardware reset 是什么？**  
A：连接过程中用 **NRST 把 MCU 按在复位里再挂 SWD**，避免应用代码占用/改掉 SWD 脚或程序跑飞导致 Attach 失败。需要 **NRST 硬件线** 才可靠。

---

## 3. Keil 编译：error #268

### 现象

```text
main.c(24): error: #268: declaration may not appear after executable statement in block
```

### 原因

Keil 默认按 **C90**：变量声明必须在代码块最前面，不能写在可执行语句后面。

### 错误写法

```c
delay_init();
u32 cnt = 0;    // 前面已有语句 → #268
```

### 正确写法

```c
int main(void)
{
    u32 cnt = 0;     // 声明放最前

    delay_init();
    LED_Init();
    uart_init(115200);
    ...
}
```

---

## 4. Day3：串口每秒打印

### 做法

- `uart_init(115200)` + 例程自带的 `printf` 重定向（USART1）  
- 主循环里约每秒 `printf("... cnt=%lu\r\n", cnt);`  
- 串口助手：对应 COM，115200 8N1  

### 常见问题

| 现象 | 处理 |
|------|------|
| 无 COM | 装 CH340，换数据线 |
| 乱码 | 波特率不一致 |
| 无打印 | 是否初始化串口；是否用了 `\r\n` |

---

## 5. Day4：定时器闪灯却“灯不亮”（重点）

### 现象

串口正常每秒打印；LED 看起来不亮/不闪。

### 错误组合

`main.c`：

```c
TIM3_Int_Init(4999, 7199);  // 约 10kHz，计到 5000 → 约 500ms 进一次中断
```

`timer.c` 中断里又：

```c
led_tick++;
if (led_tick >= 500) {   // 又等 500 次中断
    led_tick = 0;
    LED0 = !LED0;
}
```

### 原因

中断周期已经是 **500ms**，再数 500 次才翻转 → \(500 \times 500\text{ms} = 250\text{s}\) 闪一次。  
`LED_Init()` 默认拉高 PB5（灯灭），所以长时间像“灯坏了”。

### 正确理解正点原子这组参数

```c
TIM3_Int_Init(arr, psc);
// 72MHz / (psc+1) ≈ 计数时钟
// (arr+1) 个计数产生一次更新中断
// (4999,7199) → 10kHz、5000 次 → 约 500ms/中断
```

### 改法 A（推荐，配合 500ms 中断）

每次中断直接翻转 → 500ms 翻转一次 → 周期 1s → **1Hz**：

```c
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
        LED0 = !LED0;
        LED1 = !LED1;
    }
}
```

### 改法 B（要 1ms 节拍）

```c
TIM3_Int_Init(9, 7199);  // 约 1ms 中断
// 中断里再 led_tick >= 500 时翻转
```

### 极性提醒

战舰 DS0：**低电平亮**。`LED0=0` 亮，`LED0=1` 灭；`LED0=!LED0` 翻转即可。

---

## 6. Day5：启动文件问答

### Q1：复位后第一条指令从哪来？

**不是直接进 `main`。**

Cortex-M3 复位后：

1. 从 `0x08000000` 取 **MSP（栈顶）**  
2. 从 `0x08000004` 取 **Reset_Handler 地址** 并跳转  

向量表在 `startup_stm32f10x_hd.s`（战舰 ZET6 用 HD 启动文件）。

### Q2：`SystemInit` 和 `main` 谁先？

**`SystemInit` 先于 `main`。**

典型顺序：

```text
Reset_Handler → SystemInit() → __main()（拷贝.data、清.bss等） → main()
```

曾误以为 `main` 先执行，不正确。

### Q3：定时器中断为何进到自己的函数？

NVIC 查向量表中 **TIM3** 项，跳到同名函数 `TIM3_IRQHandler`。  
启动文件里的名字必须和 C 里写的 ISR **完全一致**。  
这是向量表跳转（ISR），口语可说回调，但不是随意注册的那种回调。

---

## 7. 其它零散问答

**Q：官方除了 CubeMX 还有什么可视化配置+编译？**  
A：配置主要是 **CubeMX**（或 CubeIDE 内嵌配置）；编译调试用 **CubeIDE**。跟例程目前用 Keil 更省事。

**Q：CubeProgrammer 和 CubeIDE 区别？**  
A：IDE = 写代码/编译/调试；Programmer = 烧录/读 Flash/改选项字节/升级 ST-Link。  

**Q：脚本 `bash^M` bad interpreter？**（Firefly 侧）  
A：Windows CRLF 换行；板上 `sed -i 's/\r$//' *.sh` 或 `dos2unix`。

**Q：学习路线从哪开始？**  
A：MCU（F103 CAN/Boot/NuttX）→ 970 SocketCAN → RK3399 DDS → gPTP。详见 `automotive-learning-plan.md`。

---

## 8. CAN 阶段（实验 26）

### 8.1 工程与操作

| 项 | 内容 |
|----|------|
| 工程 | `a盘\...\实验26 CAN收发实验\USER\CAN.uvprojx` |
| 波特率 | **500 kbps**（见 §8.3） |
| 发送 ID | 标准帧 **StdId = 0x12**（`Can_Send_Msg`） |
| 默认模式 | **LoopBack 环回**（单板即可） |
| 按键 | KEY0 发送；WK_UP 切换环回/普通 |
| 显示 | 已改为 **串口 printf**（可不依赖 LCD） |

### 8.2 已验证现象（环回）

```text
TX OK
RX LEN=8 DATA: 74 75 76 77 78 79 7A 7B
...
```

数据来自 `canbuf[i] = cnt + i`，故为连续 8 字节；`cnt` 随主循环递增，起始值会变。  
结论：**片内 CAN 控制器 + 初始化 + 过滤器 + 环回收发 OK。**

### 8.3 500 kbps 怎么算

调用：

```c
CAN_Mode_Init(CAN_SJW_1tq, CAN_BS2_8tq, CAN_BS1_9tq, 4, mode);
```

- \(f_{\mathrm{CAN}} = 36\,\mathrm{MHz}\)（APB1，SYSCLK=72MHz 时）  
- BRP = 4，BS1 = 9 tq，BS2 = 8 tq，Sync = 1 tq  
- 一位 = \(1+9+8=18\) tq  

\[
\text{波特率} = \frac{36\times10^6}{4\times 18} = 500\,\mathrm{kbps}
\]

SJW 只影响重同步，不参与波特率公式。

### 8.4 普通模式 vs 环回——要不要“改接线”？

**Demo 没有改 P9 / PA11/PA12。** 两种模式软件切换即可。

| 模式 | 软件 | P9（CAN 脚） | 板外 CAN 线 |
|------|------|--------------|-------------|
| 环回 | `CAN_Mode_LoopBack` | 仍接到 CAN | **不必**接 CANH/CANL |
| 普通 | `CAN_Mode_Normal` | **同样**接到 CAN | **需要**另一节点：CANH–CANH、CANL–CANL，两端常要 120Ω |

因此：不是“普通模式要改引脚定义”，而是“普通模式要有总线对端”。只有一块板时普通模式常发失败/无接收。

### 8.5 接收打印 ID

`Can_Receive_Msg` 原先只返回 Data/DLC。可在 `can.c` 接收里增加：

```c
printf("RX ID=0x%03X IDE=%d RTR=%d DLC=%d DATA:",
       RxMessage.StdId, RxMessage.IDE, RxMessage.RTR, RxMessage.DLC);
```

环回应看到 **ID=0x012**（与发送一致）。

---

## 9. 当前进度

| 项 | 状态 |
|----|------|
| Day1～5（灯/串口/定时器/启动） | 完成 |
| Day6 最小模板整理 | 可顺带，非阻塞 |
| CAN 环回 + 串口观察 | **完成** |
| CAN 500kbps 计算理解 | **完成** |
| 接收打印 StdId | **完成**（串口可见 ID=0x012） |
| 普通模式双节点 | **等工控板下载线/电源齐**（缺 USB-RS232 见 §11） |
| Bootloader 跳转 | **完成**（**32KB** @`0x08000000` → app_boot @`0x08008000`） |
| Bootloader 串口 IAP | **完成**（Python 升级 hex → JUMP）；见 `bootloader\iap-demo-readme.md` |
| Bootloader CAN IAP | **代码已加**（`iap_can.c` / `iap_can_host.py`）；待 USB-CAN 实测 |
| 厂商 can.c 与 IAP | **bootloader 勿混编**；说明见 `bootloader\iap-demo-readme.md` §8.1 |
| P3 NuttX | **已开始**；见 `nuttx-f103\README.md` |

---

## 10. 建议收藏的路径

```text
D:\Automotive\stm32-warship-troubleshooting.md   ← 本文
D:\Automotive\automotive-learning-plan.md
D:\Automotive\week01-warship-f103.md
D:\Automotive\stlinkerror.log

F:\stm32\a盘\...\实验1 跑马灯实验\
F:\stm32\a盘\...\实验26 CAN收发实验\

D:\BaiduNetdiskDownload\STM32F103VET6工控板（新）\
```

---

## 11. STM32F103VET6 工控板（MINI）资料摘要

**根目录：** `D:\BaiduNetdiskDownload\STM32F103VET6工控板（新）`

| 项目 | 说明 |
|------|------|
| MCU | STM32F103**VE**T6（比战舰 ZET6 封装/资源略有差别，同属 F103 增强型） |
| CAN | **有**板载 TJA1050；端子 CANH/CANL；板上常带 120Ω |
| CAN 引脚 | 原理图：**PB8=CAN_RX，PB9=CAN_TX（Remap）** — **与战舰 PA11/PA12 不同** |
| 其它接口 | RS485（MAX485）、RS232/DB9、USB、JTAG 排针 |
| 以太网 | **无** |
| 原理图 | `工控板原理图 (1).PDF` |
| 必读 | `工控板使用前必读.pdf`、`板子使用前必看注意事项.pdf` |
| CAN 例程 | `MINI板实验代码\高级例程-基于两个MINI板的CAN通讯` |

**下载方式（必读 PDF 结论）：**

| 方式 | 条件 | 需要的线 |
|------|------|----------|
| **J-Link / JTAG（资料主推）** | BOOT0=0, BOOT1=0；20 针 J9 | **J-Link + 20 针扁线**（不是 USB-232） |
| **串口 ISP** | BOOT0=1, BOOT1=0；USART1→DB9 | **USB-RS232（USB-232）** 接 DB9；UV1 接 3V3/5V |
| ST-Link SWD | 可用 J9 上 SWDIO/SWCLK（约 7/9 脚）+ GND/NRST | 现有 ST-Link + 杜邦线飞线（非官方首选，但可行） |

当前缺口记录：**无 J-Link**；有 **ST-Link**；所谓「FT232 资料」实际是 **CH341A 土豪金编程器**（`D:\Download\新杰影音土豪金2425编程器资料包`，P/S 打开可当 USB-TTL）。  
**推荐下载：ST-Link 飞线 SWD → J9。**  
CH341 TTL 可走 USART1 **TTL 点**做 ISP，**不要**把 TTL 直接插 DB9（DB9 是 RS232 电平）。

**缺线缆时优先补齐（按联调价值）：**

1. **下载**：J-Link 扁线 **或** USB-RS232（走 ISP）**或** ST-Link 飞线 SWD  
2. 电源：USB **或** 外部，勿同时；按注意事项限压  
3. **CAN 双绞线**（CANH/CANL）→ 与战舰普通模式互通  
4. （可选）USB-RS485  

**和战舰双节点时注意：** 两边都是 500kbps、共地、终端电阻；软件上工控板例程多半已 Remap CAN 到 PB8/PB9，不要照搬战舰的 PA11/PA12 初始化。

---

## 12. ESP32-S3 怎么放进计划

| 适合 | 暂不适合抢主线 |
|------|----------------|
| Wi‑Fi / BLE、MQTT、以后做「无线仪表」 | 替代 F103 学 CAN/Bootloader/NuttX |
| 以后：F103 CAN → 串口/SPI → S3 → Wi‑Fi → RK3399 | 一上来就多开一个 IDE 生态分心 |

建议：**P1～P3 仍以战舰为主**；MQTT/无线桥接到 P5 前后再启用 S3。

---

## 13. 一页纸速查

1. **ST-Link 连芯片**：SWD + **NRST** + **Under reset + Hardware reset**  
2. **Keil #268**：变量声明放块首  
3. **定时器闪灯**：分清中断周期 vs 软件计数  
4. **启动**：向量表 → `Reset_Handler` → `SystemInit` → `main`  
5. **CAN 500k**：\(36\mathrm{M}/(4\times18)=500\mathrm{k}\)；环回不必外接总线；普通模式要第二节点  
6. **第二块 F103 工控板**：有 TJA1050，CAN 在 **PB8/PB9**；补线后再双机  

---

*文档持续追加；P2 Boot/IAP 见 `bootloader\iap-demo-readme.md`；P3 NuttX 见 `nuttx-f103\README.md`。*
