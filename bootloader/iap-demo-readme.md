# 战舰 F103 串口 IAP（已验收）

> 状态：**仅串口升级 app_boot 已通过**（INFO → ERASE → WRITE → JUMP）  
> 板型：正点原子战舰 STM32F103ZET6  
> 验收命令见 §5  

---

## 1. 当前分区与命名

| 名称 | 工程目录（勿随意改名） | Flash | 大小 |
|------|------------------------|-------|------|
| bootloader | `bootDemo\` | `0x08000000` | 32KB (`0x8000`) |
| app_boot | `app_boot\` | `0x08008000` | `0x78000` |

```text
0x08000000  ┌─ bootloader (32KB) ─┐
0x08008000  ├─ app_boot           ┤
0x08080000  └─────────────────────┘
```

IAP **只擦写** `0x08008000`～`0x08080000`，不会动 bootloader。

---

## 2. 代码位置

| 模块 | 路径 |
|------|------|
| Flash 擦写 | `bootDemo\HARDWARE\IAP\iap_flash.c` |
| 协议 + RX 环形缓冲 | `bootDemo\HARDWARE\IAP\iap_proto.c` |
| 配置（地址/页大小/payload） | `bootDemo\HARDWARE\IAP\iap_config.h` |
| 启动逻辑 | `bootDemo\USER\main.c` |
| 串口 IRQ 原始接收开关 | `bootDemo\SYSTEM\usart\usart.c`（`IAP_UseRawRx`） |
| PC 上位机 | `bootloader\iap_uart_host.py` |
| 跳转详解（前期） | `bootloader\bootloader-jump-notes.md` |
| Flash 算法界面说明 | `bootloader\flash-algorithm-fix.md` |

---

## 3. 上电行为

1. 打印 `=== BOOTLOADER ... + IAP ===`
2. **约 2s** 采样 KEY0（低有效）/ WK_UP（高有效）
3. 未按键且 app 向量表合法 → `Jump to APP...`
4. 按过键，或无合法 app → 进入 IAP：`IAP ready: A5 5A frames @115200`

---

## 4. 协议（简要）

主机→板：`A5 5A | CMD | SEQ | LEN_H LEN_L | PAYLOAD | CRC16_H CRC16_L`  
板→主机：`5A A5 | CMD | SEQ | STATUS | LEN_H LEN_L | DATA | CRC16`  

CRC16：Modbus（初值 `0xFFFF`，多项式 `0xA001`），覆盖 CMD 起至 PAYLOAD/DATA 止。

| CMD | 含义 | PAYLOAD |
|-----|------|---------|
| 01 INFO | 查分区 | 无；回复 APP/SIZE/PAGE/VER |
| 02 ERASE | 擦 APP 区 | addr(4)+size(4)，大端 |
| 03 WRITE | 编程 | addr(4)+data；**data ≤252**（整帧 payload ≤260） |
| 04 JUMP | 跳 APP | 无；先校验向量表 |

STATUS：`0=OK`，`2=参数错`（常见：WRITE 包过大），`3=Flash 失败`，`4=地址越界`。

实现要点：

- IAP 模式用 **IRQ + 环形缓冲** 收二进制（避免 1 字节 DR 丢数）
- Flash：页擦 2KB、半字写入并回读
- 地址校验：禁止写 bootloader 区

---

## 5. 验收步骤（已通过）

```bat
pip install pyserial
```

1. 关掉串口助手（释放 COM）
2. **按住 KEY0**，再运行（USB 转串口打开时常 DTR 复位）：

```bat
python D:\Automotive\bootloader\iap_uart_host.py COM3 D:\Automotive\app_boot\OBJ\app_boot.hex
```

3. 看到 `Board: APP=0x08008000 ...` 后可松手  
4. 预期输出：

```text
FW base=0x08008000 size=3472
Board: APP=0x08008000 SIZE=0x78000 PAGE=0x800 VER=0x1
ERASE 0x08008000 len=0x1000 ...
ERASE OK
WRITE 3472/3472
WRITE OK
JUMP sent
```

5. 再开串口助手应看到 `enter app, cnt=...`  
6. 不按键复位：应自动 `Jump to APP...` → `enter app`

可选：`--no-jump` 只烧不跳；`--addr 0x08008000` 覆盖基址。

---

## 6. Keil 烧录（界面自己改，勿靠改文件夹名）

**Flash Download 算法（两工程相同）：**

- Options → Debug → ST-Link → Settings → **Flash Download**
- Add：`STM32F10x_512kB Flash`
- **Start = `0x08000000`，Size = `0x00080000`（整片）**
- Utilities 勾选 Use Debug Driver

**IROM（链接，和算法分开）：**

| 工程 | IROM Start | IROM Size |
|------|------------|-----------|
| bootloader | `0x08000000` | `0x8000` |
| app_boot | `0x08008000` | `0x78000` |

改 IROM/输出名后算法常被清空 → 再按上面加一次即可。详见 `flash-algorithm-fix.md`。

首次用 ST-Link 仍建议先烧好 bootloader；之后升级 app **可只靠串口 IAP**。

---

## 7. 踩坑记录（本阶段）

| 现象 | 原因 | 处理 |
|------|------|------|
| `No Algorithm found for 0800xxxxH` | Flash 算法空/Size 太小 | 界面设整片 `0x80000` |
| INFO Timeout | 开 COM 复位；或 RX 丢字节 | 按住 KEY0 再跑脚本；IRQ 环形缓冲 |
| WRITE `status=2` | 数据块 256+地址4 > payload 上限 | 脚本 CHUNK=252 |
| 松 KEY0 后仍可 IAP | 正常：进 IAP 后不必一直按键 | — |

---

## 8. CAN IAP（代码已加，待 USB-CAN 实测）

与 UART **同一套逻辑帧**（`A5 5A` + CMD…），经 CAN 分包传输。

| 项 | 值 |
|----|-----|
| 引脚 | 战舰 PA11/PA12，跳线 **P9** |
| 波特率 | 500 kbit/s |
| Host→MCU ID | `0x100` |
| MCU→Host ID | `0x101` |
| 板端 | `bootDemo\HARDWARE\IAP\iap_can.c`（`IAP_CAN_ENABLE=1`） |
| PC | `bootloader\iap_can_host.py` |

分包：`FIRST(0x01)+len+最多5字节`，后续 `CONT(0x02)+序号+最多6字节`。

无工具时：

- 串口 IAP **照常可用**（CAN 请求才走 CAN 应答，空总线不会拖死 UART）
- 可先 dry-run 看分包：

```bat
python D:\Automotive\bootloader\iap_can_host.py --dry-run D:\Automotive\app_boot\OBJ\app_boot.hex
```

有 USB-CAN 后（示例 PCAN）：

```bat
pip install python-can
python D:\Automotive\bootloader\iap_can_host.py --interface pcan --channel PCAN_USBBUS1 D:\Automotive\app_boot\OBJ\app_boot.hex
```

进 IAP 仍要按 KEY0；总线上需有应答节点（USB-CAN）。

自测无总线：把 `iap_config.h` 里 `IAP_CAN_LOOPBACK` 改为 `1` 后重编（仅环回，不能代替真工具升级）。

### 8.1 厂商 `HARDWARE\CAN\can.c` 有用吗？

正点原子样例 `can.c` / `can.h`（`CAN_Mode_Init` / `Can_Send_Msg`）**对当前 CAN IAP 不是必需的**。

| | 厂商 `HARDWARE\CAN\` | 现用 `iap_can.c` |
|--|---------------------|------------------|
| 作用 | 通用收发封装 | IAP：固定 ID、分包、拼帧 |
| 过滤器 | 通常全收 | 只收 `0x100` |
| bootloader | **不要编进 bootDemo**（会和 `iap_can` 双重 init / 过滤器冲突） | 已在用 |

建议：

- **bootloader**：只保留 `iap_can.c` + SPL `stm32f10x_can.c`
- **厂商 can.c**：可留在磁盘，供以后 **app_boot** 普通 CAN 通信或对照波特率公式学习
- 若精简 init，可用 `CAN_Mode_Init(SJW_1tq, BS2_8tq, BS1_9tq, 4, Normal)` 替换 init 段，但 **分包收发仍必须在 `iap_can.c`**

## 9. P2 收尾 / 下一阶段

- [ ] CAN IAP 真机验收（有 USB-CAN 后）
- [x] 进入 **P3：NuttX on F103** → 见 `nuttx-f103\README.md`

---

*串口 IAP 验收通过：2026-10-03（COM3，app_boot.hex ≈3472 字节）。*  
*CAN IAP：代码与脚本已就绪，待 CAN 工具包实测；厂商 can.c 说明见 §8.1。*
