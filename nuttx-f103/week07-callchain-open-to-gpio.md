# 从 `open("/dev/userleds")` 到 GPIO 寄存器——完整调用链

> 对照：`ledblink_main.c`、NuttX `drivers/leds/userled_*.c`、板级 `stm32_userleds.c`、`stm32_gpio.c`  
> 概念补充：`week07-open-dev-userleds.md`（upper/lower 含义）

本文按时间分两段：

1. **上电注册**（还没有 ledblink）  
2. **运行时** `open` → `ioctl(SETALL)` → 改 ODR  

---

## A. 上电：设备如何挂到 `/dev/userleds`

```text
复位 → nx_start() → … → 板级 stm32_bringup()
                              │
                              ▼
              userled_lower_initialize("/dev/userleds")
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
   board_userled_initialize()      userled_register(devname, &g_userled_lower)
   （stm32_userleds.c）            （userled_upper.c）
              │                               │
              │  for each g_ledcfg[]:         │  分配 userled_upperhalf_s
              │    stm32_configgpio()         │  priv->lu_lower = &g_userled_lower
              │  return BOARD_NLEDS (2)       │  lu_supported = lower->ll_supported()
              │                               │       → (1<<2)-1 = 0x3
              │                               │  lower->ll_setall(0) 初值全灭
              │                               │  register_driver(
              │                               │      "/dev/userleds",
              │                               │      &g_userled_fops,  /* open/ioctl… */
              │                               │      0660, priv)
              │                               ▼
              │                         VFS inode：路径 → fops + priv
              ▼
         PB5/PE5 已配成推挽输出
```

此时 NSH `ls /dev` 能看到 `userleds`。  
**还没有任何 app 的 `open`。**

关键数据结构：

```text
g_userled_fops = {
  open:  userled_open,
  close: userled_close,
  write: userled_write,
  ioctl: userled_ioctl,
  …
}

g_userled_lower = {
  ll_supported: userled_supported,   // → 返回 0x3
  ll_setled:    userled_setled,      // → board_userled()
  ll_setall:    userled_setall,      // → board_userled_all()
}
```

upper 的 `priv->lu_lower` 指向这张表；之后所有硬件动作都经函数指针下去。

---

## B. 运行时 1：`open("/dev/userleds", O_WRONLY)`

### B.1 应用层

```c
fd = open("/dev/userleds", O_WRONLY);
```

（NuttX libc / syscall 层进入内核 VFS，flat 构建下仍走同一套 `file`/`inode` 逻辑。）

### B.2 VFS（示意）

```text
open()
  → 路径解析：找到 inode("/dev/userleds")
  → inode 上绑着：
        i_ops / file_operations = g_userled_fops
        i_private             = userled_upperhalf_s *priv
  → 分配 struct file，挂到进程/任务的 fd 表 → 得到 fd（如 3）
  → 调用 g_userled_fops.open = userled_open(filep)
```

### B.3 `userled_open`（upper）

```text
userled_open(filep)
  → priv = filep->f_inode->i_private
  → 加锁
  → 分配本次打开的 userled_open_s，链到 priv->lu_open
  → filep->f_priv = 该打开实例
  → 解锁，返回 OK
```

**这里不访问 GPIO。**  
只是：fd ↔ file ↔ inode ↔ upper priv（内含 lower 指针）。

```text
任务 ledblink
  fd ──► struct file
            f_inode ──► inode("/dev/userleds")
                          i_private ──► userled_upperhalf_s
                                          lu_lower ──► g_userled_lower
                                          lu_supported = 0x3
```

---

## C. 运行时 2：`ioctl(fd, ULEDIOC_SUPPORTED, &supported)`

```text
ioctl(fd, ULEDIOC_SUPPORTED, &supported)
  → VFS：filep = fd 对应 file
  → filep->f_inode->i_ops->ioctl
       = userled_ioctl(filep, ULEDIOC_SUPPORTED, arg)
          │
          ▼
       case ULEDIOC_SUPPORTED:
         *supported = priv->lu_supported;   // 注册时已算好 = 0x3
         一般不再下钻 lower（已缓存）
```

ledblink 打印 `supported mask=0x3`。

---

## D. 运行时 3：`ioctl(fd, ULEDIOC_SETALL, ledset)`（控灯主路径）

以 `ledset = 0x1`（只亮 LED0）为例。

### D.1 总览（从上到下）

```text
ledblink_main
  ioctl(fd, ULEDIOC_SETALL, 0x1)
        │
        ▼
[VFS]  file_ioctl / 等价路径
        │  根据 fd → filep → inode → g_userled_fops.ioctl
        ▼
[Upper] userled_ioctl(filep, ULEDIOC_SETALL, 0x1)
        │  校验 (ledset & ~lu_supported) == 0
        │  priv->lu_ledset = 0x1
        │  lower = priv->lu_lower
        │  lower->ll_setall(lower, 0x1)
        ▼
[Lower] userled_setall(lower, 0x1)     // userled_lower.c
        │  board_userled_all(0x1)
        ▼
[Board] board_userled_all(ledset)      // stm32_userleds.c
        │  i=0: on=true  → stm32_gpiowrite(GPIO_LED1/PB5, !true) → 写 0 → 灯亮
        │  i=1: on=false → stm32_gpiowrite(GPIO_LED2/PE5, !false)→ 写 1 → 灯灭
        ▼
[Arch]  stm32_gpiowrite(pinset, value)
        │  解析 port=B/E, pin=5
        │  写 GPIOx->BSRR 或 ODR（F1 实现以树内代码为准）
        ▼
[HW]    引脚电平变化 → LED 阴极/阳极电路 → DS0/DS1
```

### D.2 Upper：`userled_ioctl` 里 SETALL 分支（逻辑摘录）

```c
case ULEDIOC_SETALL:
  ledset = (userled_set_t)arg;
  if ((ledset & ~priv->lu_supported) == 0)
    {
      priv->lu_ledset = ledset;
      lower = priv->lu_lower;
      lower->ll_setall(lower, ledset);   /* 函数指针！ */
    }
```

要点：upper **不知道** PB5/PE5；只认位图和 `ll_setall`。

### D.3 Lower：薄封装

```c
static void userled_setall(..., userled_set_t ledset)
{
  board_userled_all(ledset);
}
```

Lower 文件是通用的；真正板相关在 `board_*`。

### D.4 Board：战舰极性

```c
void board_userled_all(uint32_t ledset)
{
  for (i = 0; i < BOARD_NLEDS; i++)
    {
      bool ledon = (ledset & (1 << i)) != 0;
      stm32_gpiowrite(g_ledcfg[i], !ledon);  /* 低电平亮 */
    }
}

/* g_ledcfg[] = { GPIO_LED1(/*PB5*/), GPIO_LED2(/*PE5*/) } */
```

### D.5 Arch：到寄存器

`stm32_gpiowrite(cfgset, value)` 大致：

1. 从 `cfgset` 取出 port、pin（NuttX 把端口/引脚编码在宏里）  
2. 得到 `GPIO_TypeDef *`（如 `GPIOB`）  
3. `value==1` → 输出高；`0` → 输出低（F1 常用 BSRR 原子置位/复位）  

战舰 LED：**驱动脚为低 → 灯亮**，故板级传 `!ledon`。

---

## E. 一张「谁调用谁」表

| 层级 | 函数 / 对象 | 文件（典型） |
|------|-------------|--------------|
| App | `open` / `ioctl` | `ledblink_main.c` |
| libc/VFS | 路径查找、`file`/`inode`、调 `fops` | `fs/`、`syscall` |
| Upper fops | `userled_open` / `userled_ioctl` | `drivers/leds/userled_upper.c` |
| Upper 状态 | `userled_upperhalf_s`（含 `lu_lower`） | 同上 |
| Lower 表 | `g_userled_lower.ll_setall` | `drivers/leds/userled_lower.c` |
| Lower 实现 | `userled_setall` → `board_userled_all` | 同上 |
| Board | `board_userled_initialize` / `_all` | `boards/.../stm32_userleds.c` |
| Arch | `stm32_configgpio` / `stm32_gpiowrite` | `arch/.../stm32_gpio.c` |
| HW | GPIOx ODR/BSRR、LED 电路 | 芯片 + 原理图 |

---

## F. `open` 与 `ioctl` 各自停在哪一层

| API | 最深通常到哪 | 是否动灯 |
|-----|--------------|----------|
| `open` | upper `userled_open`（建打开实例） | 否 |
| `ioctl(SUPPORTED)` | upper 读 `lu_supported` | 否 |
| `ioctl(SETALL)` | 一直到 `stm32_gpiowrite` → 寄存器 | **是** |
| `close` | upper `userled_close` | 否 |

---

## G. 和 CAN 等外设对照（同一骨架）

```text
open("/dev/can0")
  → VFS → can_upper 的 fops.open
write/ioctl(CAN 帧)
  → can_upper
    → can_lower / stm32_can
      → 写 CAN 邮箱寄存器 → 收发器 → 总线
```

换的是 **fops / 命令 / 寄存器**，不是另一套 `open` 哲学。

---

## H. 本机对照阅读（Ubuntu）

```bash
# 注册
grep -n userled_lower_initialize \
  boards/arm/stm32f1/stm32f103-minimum/src/stm32_bringup.c

# Upper / Lower
less drivers/leds/userled_upper.c    # g_userled_fops, userled_ioctl, userled_register
less drivers/leds/userled_lower.c    # ll_setall → board_userled_all

# 板级
less boards/arm/stm32f1/stm32f103-minimum/src/stm32_userleds.c

# GPIO
less arch/arm/src/stm32f1/stm32_gpio.c   # 或 common stm32 路径下的 gpio
```

---

## I. 相关文档

| 文档 | 内容 |
|------|------|
| `week07-open-dev-userleds.md` | open/ioctl 用法与 upper/lower 概念 |
| `week07-nuttx-build-boot-sched.md` | 任务如何被调度、两仓库链接 |
| `week07-enable-peripherals.md` | 开其它外设的配置层 |
