# 战舰 NuttX：怎么开其他外设并编进镜像

> 前提：已能 `nsh>`，芯片为 **STM32F103ZE**，`ld.script` 为 512K/64K，板级 depends 已放宽（见 `week07-two-leds.md`）。  
> 你现在的板包名仍是 `stm32f103-minimum`——**芯片驱动在 arch 里已有**；缺的是「menuconfig 打开 + 板级引脚/初始化 +（可选）应用」。

---

## 1. 四层要改什么（通用套路）

| 层 | 在哪 | 干什么 |
|----|------|--------|
| ① 芯片外设开关 | `make menuconfig` → System Type → **STM32 Peripheral Support** | 把 `CONFIG_STM32_CAN1` / `SPI1` / `I2C1` / `ADC1`… 打开，对应 `.c` 才进 `libarch` |
| ② 引脚复用 | `boards/.../stm32f103-minimum/include/board.h`（及芯片 pinmap） | 定义 `GPIO_CANx_RX/TX`、`GPIO_SPI1_*` 等，对齐**战舰原理图** |
| ③ 板级挂载 | `boards/.../src/stm32_bringup.c`（或板级 `stm32_can.c` 等） | 上电后 `xxx_register()` / 创建 `/dev/can0` 等节点 |
| ④ 应用 | `apps/` 示例或自写 | NSH 命令、`open("/dev/...")`、ioctl |

只做 ①：驱动可能编进镜像，但**没设备节点或引脚不对**，应用仍用不了。  
LED 当时就是：① USERLED_LOWER + ② GPIO 宏 + ③ bringup 里 `userled_lower_initialize` + ④ `ledblink`。

---

## 2. menuconfig 里常见路径

```bash
cd ~/automotive-nuttx/nuttx
make menuconfig
```

| 目的 | 菜单（名称因版本略有出入，可用 `/` 搜索） |
|------|------------------------------------------|
| 开 CAN1 / SPI / I2C / ADC / 定时器 | `System Type` → `STM32 Peripheral Support` → 勾选外设 |
| 串口控制台 | 已用 USART1；别和外设脚冲突 |
| 字符驱动 / CAN 框架 | `Device Drivers` → 搜 `CAN` / `SPI` / `I2C` |
| 示例程序 | `Application Configuration` → `Examples` |
| 确认芯片仍是 ZE | `System Type` → `STM32 Chip Selection` → **STM32F103ZE** |

改完：

```bash
make olddefconfig   # 可选，补依赖
make -j$(nproc)
# 再烧录 nuttx
```

用 grep 确认真开了（示例）：

```bash
grep -E 'CONFIG_STM32_CAN|CONFIG_CAN|CONFIG_STM32_SPI|CONFIG_STM32_I2C' .config | grep '=y'
```

---

## 3. 战舰引脚速查（配 board.h 时对照）

| 外设 | 战舰常用脚 | 注意 |
|------|------------|------|
| USART1 控制台 | PA9 TX / PA10 RX | 已占用，别改作别的 |
| LED | PB5 / PE5 | USERLED 已用 |
| CAN1 | PA11 RX / PA12 TX | 跳线 **P9**；与 USB 脚复用，战舰走 CAN 时按原理图 |
| SPI1（例） | 视模块：常见 PA5/6/7 等 | 以原理图 / 正点例程为准 |
| I2C1（例） | 常见 PB6/PB7 | 同上 |

板级里改/加宏，例如 CAN（名称以你树里已有 `GPIO_CAN1_*` 为准）：

```c
/* include/board.h — 示意，以 NuttX 该板现有宏名为准 */
#define GPIO_CAN1_RX  GPIO_CAN1_RX_1   /* 通常映到 PA11 */
#define GPIO_CAN1_TX  GPIO_CAN1_TX_1   /* 通常映到 PA12 */
```

芯片头文件里往往有多组复用（`_1` / `_2`），**选战舰那一组**。

---

## 4. bringup：让 `/dev/xxx` 出现

打开外设后，在板级初始化里注册设备。常见位置：

```text
boards/arm/stm32f1/stm32f103-minimum/src/stm32_bringup.c
```

模式与 USERLED 相同：

```c
#ifdef CONFIG_STM32_CAN1
  /* 调用板级或架构提供的 can 初始化 / 注册 */
  ret = stm32_can_initialize(...);   /* 具体函数名以板内已有代码为准 */
#endif
```

有的板已有 `stm32_can.c`、`stm32_spi.c`，被 `Make.defs` 条件编译；打开 `CONFIG_STM32_CANx` 后确认对应 `.c` 在编：

```bash
grep -E 'can|spi|i2c' boards/arm/stm32f1/stm32f103-minimum/src/Make.defs
```

烧录后在 NSH：

```text
nsh> ls /dev
```

应能看到如 `can0`、`ttyS0`、`userleds` 等（名称以驱动为准）。

---

## 5. 应用怎么用

三种由易到难：

1. **官方示例**（menuconfig → Examples）  
   - 如 CAN：搜 `CAN` / `can` 相关 example（版本不同名字不一）  
2. **NSH 命令**（若开了对应命令）  
3. **自写 apps**（仿 `ledblink`：`Makefile` + `Kconfig` + `Make.defs` + `xxx_main.c`，`open`/`read`/`write`/`ioctl`）

自写时记得在 `apps/examples` 下跑 `mkkconfig`（见 `week07-rtos-task.md`），再 menuconfig 勾选。

---

## 6. 推荐练习顺序（结合你的计划）

| 顺序 | 外设 | 建议 |
|------|------|------|
| 已做 | GPIO LED | USERLED + ledblink |
| 下一步（第 8 周） | **CAN1** | ① `CONFIG_STM32_CAN1` + CAN 驱动 ② PA11/12 ③ bringup ④ 收发例程；硬件 P9 |
| 可选 | SPI / I2C | 按模块原理图改脚，再开 `CONFIG_STM32_SPIx` / `I2Cx` |
| 可选 | ADC / PWM | 同样：Peripheral → 引脚 → bringup → 应用 |

第 8 周 CAN 细节可另开 `week08-*.md`；配置手法与本文相同。

---

## 7. 常见坑

1. **只开了 Peripheral，没改引脚 / bringup** → 链进驱动但 `/dev` 没有或通讯失败。  
2. **引脚与 USART1/LED 冲突** → 控制台丢或灯异常。  
3. **又改回 C8** → GPIOE 等又没了；保持 **ZE** + 板级 depends 补丁。  
4. **`menuconfig` 把 BOARD 弄丢** → 先确认 `CONFIG_ARCH_BOARD_STM32F103_MINIMUM=y` 仍在。  
5. **时钟** → 部分外设要在 `board.h` 里配 PLL/APB；CAN 对波特率时钟敏感。  
6. **改完没重烧** → 串口还是旧镜像。

---

## 8. 最小操作清单（复制用）

```bash
cd ~/automotive-nuttx/nuttx

# 1) 开外设 + 驱动框架 +（可选）示例
make menuconfig
# 保存后核对：
grep '=y' .config | grep -iE 'STM32_CAN|CONFIG_CAN|STM32_SPI|STM32_I2C'

# 2) 改板级引脚 / bringup（vim）
# boards/arm/stm32f1/stm32f103-minimum/include/board.h
# boards/arm/stm32f1/stm32f103-minimum/src/stm32_bringup.c

# 3) 确认芯片与板还在
grep -E 'CONFIG_ARCH_CHIP_STM32F103ZE=y|CONFIG_ARCH_BOARD_STM32F103_MINIMUM=y' .config

# 4) 编译烧录
make -j$(nproc)

# 5) NSH 检查
# nsh> ls /dev
```

---

## 9. 相关文档

| 文档 | 内容 |
|------|------|
| `week07-nuttx-build-boot-sched.md` | 两仓库如何链成一个 `nuttx` |
| `week07-two-leds.md` | ZE / USERLED 踩坑（开外设前先稳定） |
| `README.md` | 战舰引脚表、烧录 |
| `stm32-warship-troubleshooting.md` | 硬件跳线（CAN P9 等） |
| `automotive-learning-plan.md` | 第 8 周 NuttX CAN |
