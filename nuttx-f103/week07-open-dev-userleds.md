# `open("/dev/...")` 怎么驱动外设——以 ledblink + `/dev/userleds` 为例

> 对照代码：`apps-overlay/examples/ledblink/ledblink_main.c`  
> 板级：`stm32_userleds.c`、`stm32_bringup.c`  
> 框架：`drivers/leds/`（upper + lower）

---

## 0. 先建立模型

应用**不直接碰寄存器**。路径是：

```text
应用 open/ioctl
  → VFS（按路径找到已注册的字符设备）
    → 驱动 upper-half（统一 ioctl 号、文件语义）
      → 驱动 lower-half（回调表）
        → 板级 board_userled_*（知道 PB5/PE5）
          → stm32_gpiowrite（写 GPIO 寄存器）
            → 灯亮灭
```

`/dev/userleds` 只是一个**字符串名字**：bringup 注册时绑定到这套驱动；`open` 成功后返回的 `fd` 就是通往这套驱动的句柄。

---

## 1. 设备节点从哪来（上电时，在 app 之前）

`stm32_bringup()`（板级）里大致有：

```c
ret = userled_lower_initialize("/dev/userleds");
```

内部会：

1. 调 **`board_userled_initialize()`**（你的 `stm32_userleds.c`）  
   - 按 `g_ledcfg[]` 对 PB5、PE5 做 `stm32_configgpio`  
   - 返回灯数量（`BOARD_NLEDS`，你为 2）
2. 把 lower-half 回调表绑到 upper-half  
3. **`register_driver("/dev/userleds", ...)`** 一类接口  
   - 在 VFS 里挂上节点，NSH `ls /dev` 能看到 `userleds`

若这一步没跑（没开 `USERLED_LOWER`、bringup 没调），后面 `open` 必失败——ledblink 会退回 printf-only。

---

## 2. ledblink 里每一行在干什么

### 2.1 头文件

```c
#include <nuttx/leds/userled.h>   /* ULEDIOC_*, userled_set_t */
#include <fcntl.h>               /* open, O_WRONLY */
#include <sys/ioctl.h>           /* ioctl */
```

POSIX 风格：`open` / `ioctl` 与 Linux 用户态类似，但底层是 NuttX VFS + 内核驱动（flat 构建下与应用同地址空间，但仍走驱动接口）。

### 2.2 `open`：拿到设备句柄

```c
fd = open("/dev/userleds", O_WRONLY);
```

| 步骤 | 含义 |
|------|------|
| 路径 `/dev/userleds` | 必须与 bringup 注册名**完全一致** |
| `O_WRONLY` | 只写；控灯用 ioctl，一般不 `read` |
| 返回 `fd >= 0` | VFS 找到驱动，打开成功 |
| `fd < 0` | 节点不存在 / 未注册 → 代码走 printf-only |

`open` **不会**自己点灯；只建立「应用 ↔ 驱动实例」的连接。

### 2.3 `ULEDIOC_SUPPORTED`：问板子有几盏灯

```c
userled_set_t supported = 0;
ioctl(fd, ULEDIOC_SUPPORTED, (unsigned long)&supported);
/* 你板上打印: supported mask=0x3  → bit0、bit1 有效 */
```

调用链：

```text
ioctl(fd, ULEDIOC_SUPPORTED, &supported)
  → userled upper-half 处理该命令
    → lower→ll_supported()
      → 最终与 BOARD_NLEDS / 板级能力对应
```

`0x3` 表示 bit0=LED0(PB5)、bit1=LED1(PE5) 可用。这是**软件能力掩码**，不是「灯已经在闪」。

### 2.4 循环里算 `ledset`，再 `ULEDIOC_SETALL`

```c
if (same)
  ledset = on ? 0x3 : 0x0;     /* 两灯同亮同灭 */
else
  ledset = on ? 0x1 : 0x2;     /* 交替：只亮 0 或只亮 1 */

ioctl(fd, ULEDIOC_SETALL, ledset);
```

注意第三个参数：这里传的是 **`ledset` 的值**（位图），不是指针（与 `SUPPORTED` 不同）。含义由驱动约定：

| bit | 战舰 |
|-----|------|
| bit0 = 1 | 逻辑「亮」LED0 → PB5 |
| bit1 = 1 | 逻辑「亮」LED1 → PE5 |

### 2.5 `usleep`：让出 CPU，不占死总线

```c
usleep(CONFIG_EXAMPLES_LEDBLINK_INTERVAL_MS * 1000);
```

与驱动无关：任务睡眠，调度器可跑 NSH。下次循环再 `ioctl` 改灯。

---

## 3. `ioctl(ULEDIOC_SETALL)` 完整下钻

```text
ledblink 任务
  ioctl(fd, ULEDIOC_SETALL, ledset)
      │
      ▼
  VFS：用 fd 找到 file_operations → userled 的 ioctl 入口
      │
      ▼
  drivers/leds/... upper-half
      解析命令号 ULEDIOC_SETALL，取出 ledset
      │
      ▼
  lower-half：ll_setall(lower, ledset)
      │
      ▼
  board_userled_all(ledset)          ← 板级 stm32_userleds.c
      for i in 0 .. BOARD_NLEDS-1:
        on = (ledset & (1<<i)) != 0
        stm32_gpiowrite(g_ledcfg[i], !on)   ← 战舰低电平亮，故取反
      │
      ▼
  stm32_gpiowrite
      写 GPIOB ODR bit5 / GPIOE ODR bit5
      │
      ▼
  硬件 LED DS0 / DS1
```

你踩过的坑对应这一层：

- 只改 `board_userled`、不改 **`board_userled_all`** → `SETALL` 极性错  
- 芯片仍是 C8 → `stm32_configgpio(PE5)` 无效 → bit1 软件有、灯不动  

---

## 4. 为什么用 `open` + `ioctl`，而不是直接调 `board_userled_all`

| 直接调板级函数 | `open`/`ioctl` |
|----------------|----------------|
| 应用依赖板级符号，换板要改 app | 应用只认 `/dev/userleds` 和 ioctl 号 |
| 绕过权限/统一设备模型 | 与 CAN、串口同一套 POSIX 风格 |
| 适合极简 demo | **NuttX 推荐用法**（可移植） |

同一套习惯换外设时几乎不变：

```c
fd = open("/dev/can0", O_RDWR);
write(fd, frame, len);           /* 或 ioctl，视驱动而定 */
```

---

## 5. 和其他外设的类比

| 外设 | 典型节点 | 应用常用接口 | 板级/arch 最终落到 |
|------|----------|--------------|-------------------|
| USERLED | `/dev/userleds` | `ioctl(ULEDIOC_*)` | `board_userled_*` → GPIO |
| 串口 | `/dev/ttyS0` 等 | `read`/`write` | USART 寄存器 |
| CAN | `/dev/can0` 等 | `read`/`write`/`ioctl` | CAN 控制器 + 收发器 |
| SPI | `/dev/spi0` 等 | `ioctl` + 传输 | SPI 外设 |

共同模式：

1. bringup **注册** `/dev/xxx`  
2. app **`open`** 得 `fd`  
3. **`read`/`write`/`ioctl`** 进驱动  
4. 驱动调 **芯片/板级** 操作硬件  

差别只在：ioctl 命令集、数据格式（LED 位图 vs CAN 帧）。

---

## 6. 对照 ledblink 源码结构图

```text
main()
├── open("/dev/userleds")     ──► 要节点已存在（bringup）
├── ioctl(SUPPORTED)          ──► 读能力掩码 0x3
└── loop:
      ├── 计算 ledset (0x1/0x2 或 0x3/0x0)
      ├── ioctl(SETALL, ledset) ──► board_userled_all → GPIO
      ├── printf(...)
      └── usleep(500ms)         ──► 调度，不关驱动
```

---

## 7. 自己排查 `open` 失败

```bash
# NSH
ls /dev
# 应有 userleds

# 配置
grep -E 'CONFIG_USERLED|CONFIG_USERLED_LOWER|CONFIG_ARCH_LEDS' .config
```

| 现象 | 常见原因 |
|------|----------|
| `open` 失败 | bringup 未调用 / 未开 `USERLED_LOWER` / 名不一致 |
| `open` 成功但灯不对 | `board_userled_all` 极性、引脚宏、芯片无 GPIOE |
| `mask=0x1` | `BOARD_NLEDS` 仍为 1 或 `g_ledcfg` 只配了一盏 |

---

## 8. 相关文档

| 文档 | 内容 |
|------|------|
| `week07-nuttx-build-boot-sched.md` | 任务如何被调度；`Application.mk` 登记 |
| `week07-enable-peripherals.md` | 开 CAN/SPI 等外设的配置层 |
| `week07-two-leds.md` | 双灯板级与 ZE 踩坑 |
| `apps-overlay/examples/ledblink/ledblink_main.c` | 本文对照源码 |
