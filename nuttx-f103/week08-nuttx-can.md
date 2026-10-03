# 第 8 周 — NuttX + CAN（战舰 F103ZET6）

> 第 7 周已完成：ledblink / 双灯 / wqblink。  
> 本周目标：打开片上 **CAN1** → `/dev/can0` → 周期发「车速」帧。  
> 示例应用：`apps-overlay/examples/canspeed/`  
> 通用外设步骤见 `week07-enable-peripherals.md`。

---

## 0. 硬件（战舰）

| 项 | 说明 |
|----|------|
| CAN 控制器 | 片上 bxCAN → **CAN1** |
| MCU 脚 | **PA11 = RX，PA12 = TX**（默认复用） |
| 收发器 | 板载 TJA1050 |
| 跳线 | **P9** 接到 CAN（按正点原理图） |
| 波特率 | 与 P1/IAP 一致：**500 kbit/s** |
| 本周 TX ID | **0x201**（模拟车速，见学习计划） |

单板自测：优先开 **CAN 环回（loopback）**，不依赖第二节点 / USB-CAN。  
双机或 USB-CAN 抓帧：关环回，CANH/CANL + 终端电阻按物理层接法。

注意：F103 上 **USB 与 CAN 共享部分 SRAM**，开 CAN 时不要同时开 USB 设备功能。

---

## 1. 验收分两档

| 档 | 标准 |
|----|------|
| A. 驱动起来 | `ls /dev` 有 `can0`；`canspeed` 能 `open` 且 `write` 不报错 |
| B. 业务 | 环回或外部工具看到 ID `0x201`、数据为递增车速；下周再做收开关量控灯 |

先做 **A**。

---

## 2. menuconfig（芯片 + 驱动）

```bash
cd ~/automotive-nuttx/nuttx
make menuconfig
```

用 `/` 搜索并打开（名称随版本略有差异）：

| 符号 | 含义 |
|------|------|
| `STM32_CAN1` | 片上 CAN1 外设 |
| `CAN` | CAN 字符驱动框架 |
| `CAN_LOOPBACK` | **单板自测先开**（有则打开） |
| （可选）`EXAMPLES_CAN` | 官方 `can` 示例，联调备用 |

波特率相关（有则设为 500000）：

```text
Device Drivers → CAN Driver Support
  或 System Type → STM32 → CAN
  Baud / bitrate → 500000
```

核对芯片仍是 ZE、板级仍在：

```bash
grep -E 'CONFIG_ARCH_CHIP_STM32F103ZE=y|CONFIG_ARCH_BOARD_STM32F103_MINIMUM=y' .config
grep -E 'CONFIG_STM32_CAN1=y|CONFIG_CAN=y|CONFIG_CAN_LOOPBACK' .config
```

期望至少：

```text
CONFIG_STM32_CAN1=y
CONFIG_CAN=y
```

---

## 3. 板级：引脚 + `/dev/can0`

### 3.1 先看树里有没有现成文件

```bash
BOARD=boards/arm/stm32f1/stm32f103-minimum
ls $BOARD/src/stm32_can.c 2>/dev/null || echo "no stm32_can.c"
grep -n 'CAN\|can' $BOARD/src/stm32_bringup.c $BOARD/src/Make.defs $BOARD/include/board.h | head -40
```

若 NuttX 13.1 的 minimum 板**已有** `stm32_can.c` 且 bringup 会调 `stm32_can_setup()`，则主要确认 **board.h 引脚为 PA11/PA12**，跳到 §4。

### 3.2 `include/board.h` 引脚（战舰）

在芯片 pinmap 里 CAN1 默认组一般是 PA11/PA12。板级常写成：

```c
/* CAN1 — WarShip: PA11 RX, PA12 TX */
#define GPIO_CAN1_RX   GPIO_CAN1_RX_0   /* 以你树里 pinmap 实际后缀为准 */
#define GPIO_CAN1_TX   GPIO_CAN1_TX_0
```

查可用后缀：

```bash
grep -n 'GPIO_CAN1_RX\|GPIO_CAN1_TX' \
  arch/arm/src/stm32f1/hardware/*can* \
  arch/arm/include/stm32f1/*.h 2>/dev/null | head -30
```

选映到 **PA11 / PA12** 的那一组（不要选 PB8/PB9 remap，那是工控板常用脚）。

### 3.3 新增 `src/stm32_can.c`（若没有）

```c
/****************************************************************************
 * boards/.../stm32f103-minimum/src/stm32_can.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <debug.h>

#include <nuttx/can/can.h>
#include <arch/board/board.h>

#include "chip.h"
#include "arm_internal.h"
#include "stm32.h"
#include "stm32f103_minimum.h"

#ifdef CONFIG_CAN

#if defined(CONFIG_STM32_CAN1)
#  define CAN_PORT 1
#else
#  error "WarShip week8 expects CONFIG_STM32_CAN1"
#endif

int stm32_can_setup(void)
{
  struct can_dev_s *can;
  int ret;

  can = stm32_caninitialize(CAN_PORT);
  if (can == NULL)
    {
      canerr("ERROR: Failed to get CAN interface\n");
      return -ENODEV;
    }

  ret = can_register("/dev/can0", can);
  if (ret < 0)
    {
      canerr("ERROR: can_register failed: %d\n", ret);
      return ret;
    }

  return OK;
}

#endif /* CONFIG_CAN */
```

头文件里若板级习惯声明在 `stm32f103_minimum.h`：

```c
#ifdef CONFIG_CAN
int stm32_can_setup(void);
#endif
```

### 3.4 `stm32_bringup.c` 里调用

在合适位置（与 `userled_lower_initialize` 同类）增加：

```c
#ifdef CONFIG_CAN
  ret = stm32_can_setup();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: stm32_can_setup failed: %d\n", ret);
    }
#endif
```

### 3.5 `src/Make.defs` 编进该文件

```make
ifeq ($(CONFIG_CAN),y)
CSRCS += stm32_can.c
endif
```

（若已有类似行则不必重复。）

---

## 4. 安装 `canspeed` 应用

Windows：

```powershell
scp -r D:\Automotive\nuttx-f103\apps-overlay\examples\canspeed darren@RedmiBook-Pro-14:~/automotive-nuttx/apps/examples/
```

Ubuntu：

```bash
cd ~/automotive-nuttx/apps/examples
../tools/mkkconfig.sh -m "Examples"
grep -n canspeed Kconfig

cd ~/automotive-nuttx/nuttx
make menuconfig
# Application Configuration → Examples → [*] canspeed
# 确认 CONFIG_CAN=y（否则 depends 选不了）

grep CONFIG_EXAMPLES_CANSPEED=y .config
make -j$(nproc)
```

烧录后：

```text
nsh> ls /dev
nsh> canspeed
nsh> ledblink          # 应立刻能跑（canspeed 已改成后台 LPWORK）
nsh> canspeed stop
```

期望类似：

```text
canspeed: baud=500000 ...
canspeed: BG TX ... on LPWORK
canspeed: main returns; NSH free ...
nsh>
canspeed: #1 id=0x201 speed=0
...
```

### 为何旧版会「卡住」跑不了 ledblink？

NSH **前台一次只跑一个命令**。旧 `canspeed` 在任务里 `for + write + usleep`，约 `count×period`（如 50×100ms≈5s）内占着前台，此时敲 `ledblink` 不会执行。

处理办法（新版已默认用 1）：

| 做法 | 说明 |
|------|------|
| **`canspeed`（默认）** | TX 丢到 **LPWORK**，main 马上返回，NSH 空闲 |
| `canspeed &` | 旧前台版也可用后台（若 NSH 支持） |
| `canspeed fg` | 仍要阻塞时用（调试） |
| `canspeed stop` | 停掉后台 TX |

若 `write` 因无 ACK 一直阻塞，前台/后台都可能卡住——单板请开 **loopback**。

### 后台 TX 与 `errno=9`（EBADF）

NuttX 里 **fd 属于打开它的那个任务**。若在 `canspeed` 的 main 里 `open`，main 一返回任务退出，内核会 **关掉该任务的全部 fd**；随后 LPWORK 再 `write` 就会：

```text
canspeed: write failed #1 nbytes=-1 errno=9
```

正确做法：在 **worker 线程里** `open`/`write`/`close`（当前 `canspeed_main.c` 已按此实现）。`canspeed stop` 也会把 `close` 投递回 LPWORK。

**已通过（驱动档）：** `open` 成功且能连续 `write`（2026-10-03）。  
若打印 `baud=257142` 一类而非 **500000**，见下方 §4.1 改比特率（联 USB-CAN / 970 前必须改对）。

若 `open failed`：bringup / `STM32_CAN1` / 引脚未就绪。  
若 `write failed`：常见是总线上无 ACK——**先开 loopback** 再测。

### 4.1 把波特率改到 500 kbit/s

你当前日志里的 `baud=257142 tseg1=6 tseg2=7` 是驱动按 **APB1≈36MHz** 与默认 TSEG 算出来的实际速率（不是 500k）。环回自测能通，但联 USB-CAN / 970 前必须改成 **500000**。

```bash
cd ~/automotive-nuttx/nuttx
grep -E 'CONFIG_STM32_CAN1_BAUD|CONFIG_STM32_CAN_TSEG' .config
```

`make menuconfig`，`/` 搜 **`STM32_CAN1_BAUD`**（或 “CAN1 baud”）：

```text
设 CONFIG_STM32_CAN1_BAUD = 500000
```

默认 `TSEG1=6`、`TSEG2=7`（共 14 tq）在 36MHz APB1 上**凑不出整数 BRP 的精确 500k**。若改完 BAUD 后打印仍不是 500000，再改：

```text
CONFIG_STM32_CAN_TSEG1=6
CONFIG_STM32_CAN_TSEG2=5
```

（12 tq × BRP=6 → 36MHz/72 = **500000**。也可用 TSEG1=10、TSEG2=1。）

也可用 sed（改完建议 `make olddefconfig` 再编）：

```bash
# 若已有该项则改数值；没有则追加
grep -q '^CONFIG_STM32_CAN1_BAUD=' .config \
  && sed -i 's/^CONFIG_STM32_CAN1_BAUD=.*/CONFIG_STM32_CAN1_BAUD=500000/' .config \
  || echo 'CONFIG_STM32_CAN1_BAUD=500000' >> .config
sed -i 's/^CONFIG_STM32_CAN_TSEG1=.*/CONFIG_STM32_CAN_TSEG1=6/' .config
sed -i 's/^CONFIG_STM32_CAN_TSEG2=.*/CONFIG_STM32_CAN_TSEG2=5/' .config
# 若没有 TSEG 行，menuconfig 里改更稳妥

make -j$(nproc)
```

重烧后期望：

```text
canspeed: baud=500000 tseg1=6 tseg2=5 ...
```

---

## 5. 和 `open("/dev/userleds")` 对照

```text
canspeed
  open("/dev/can0")
  write(fd, &can_msg_s, CAN_MSGLEN(2))
    → CAN 字符驱动 upper
      → stm32 can lower
        → bxCAN 邮箱 → PA12/PA11 → TJA1050 → 总线
```

帧内容在应用里填 `struct can_msg_s`（见 `canspeed_main.c`），不必直接碰 CAN 寄存器。

---

## 6. 官方 `can` 示例（可选）

menuconfig → Examples → CAN example：可做 read/write/loopback 更完整的自检。  
本周业务演示用 **`canspeed`（0x201）** 即可，和后续 SocketCAN / DDS 车速话题对齐。

---

## 7. 常见坑

| 现象 | 处理 |
|------|------|
| 无 `/dev/can0` | 缺 `stm32_can_setup` / 未开 `CONFIG_CAN` |
| open 失败 | 同上；确认已重烧 |
| write 超时 / 失败 | 无第二节点 ACK → 开 **loopback**；或接 USB-CAN |
| 脚不对 | 误用 PB8/PB9；战舰用 **PA11/PA12** + P9 |
| 与 USB 冲突 | 关掉 USB device 相关配置 |
| BOARD 又丢了 | 保持 ZE + `ARCH_BOARD_STM32F103_MINIMUM`（见 week07-two-leds） |

---

## 8. 本周完成后

- [x] `/dev/can0` 存在  
- [x] `canspeed` 周期 TX `0x201`  
- [x] `baud=500000`（TSEG1=6 / TSEG2=5）  
- [ ] （加分）USB-CAN / 第二节点抓到帧（需关 loopback）  
- [x] 收开关量控灯：`canlight` loopback 自测通过（2026-10-03）

下一阶段：`hikey970-socketcan/week09-hikey970-can.md`（先关战舰 loopback 再接线）。

---

## 10. 收帧控灯：`canlight`（ID 0x202）

代码：`apps-overlay/examples/canlight/`

### 协议

| ID | 方向 | 数据 |
|----|------|------|
| 0x201 | 车速 TX（`canspeed`） | 2 字节速度 |
| **0x202** | **灯控** | `data[0]`：**bit0=LED0(PB5)，bit1=LED1(PE5)**，1=亮 |
| 0x100 | CAN IAP（Boot） | 勿占用 |

### 安装

```powershell
scp -r D:\Automotive\nuttx-f103\apps-overlay\examples\canlight darren@RedmiBook-Pro-14:~/automotive-nuttx/apps/examples/
```

```bash
cd ~/automotive-nuttx/apps/examples && ../tools/mkkconfig.sh -m "Examples"
cd ~/automotive-nuttx/nuttx
make menuconfig   # Examples → canlight（需 CAN + USERLED）
make -j$(nproc)
```

单板自测请保持 **`CONFIG_CAN_LOOPBACK=y`**。

### 用法（顺序很重要）

```text
nsh> canlight          # ① 要测 RX 时先启动守护
nsh> canlight 3        # ② 本地立刻控灯 + CAN TX
nsh> canlight 1
nsh> canlight 0
nsh> canlight stop
```

**灯不亮：** 旧版只 TX、靠守护 RX 才控灯；若没先 `canlight` 起守护，灯不会变。  
**新版：** `canlight <mask>` **先本地 USERLED，再 TX**。

**串口很久才回应：** `write` 在等总线 ACK。无第二节点时请：

```bash
grep CONFIG_CAN_LOOPBACK .config   # 须 =y
```

期望（新版）：

```text
canlight: local LEDs mask=0x3 ...   ← 灯应马上变
canlight: TX ...
canlight: TX ok
canlight: RX ...                    ← 守护已开且 loopback 时才有
```

---

## 9. 相关文档

| 文档 | 内容 |
|------|------|
| `how-to-add-nuttx-app.md` | 新增 app 四文件 |
| `week07-enable-peripherals.md` | 外设四层 |
| `week07-open-dev-userleds.md` | open/ioctl 模型 |
| `bootloader/iap-demo-readme.md` | CAN IAP ID（0x100，勿与 0x201 业务帧混淆） |
| `automotive-learning-plan.md` | P3 第 8 周 |
