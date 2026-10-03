# 第 7 周 · 步骤 1：战舰 PB5 真灯（USERLED）

> 前提：`ledblink` printf 版已通过。  
> 目标：打开 `/dev/userleds`，灯脚改为战舰 **LED0 = PB5（低电平亮）**。

官方 `stm32f103-minimum` 默认灯是 **PC13**（蓝药丸）或 **PB12**（黑药丸），和战舰不同，必须改板级。

---

## 1. 找到板级文件（Ubuntu）

```bash
cd ~/automotive-nuttx/nuttx
ls boards/arm/stm32f1/stm32f103-minimum/src/stm32f103_minimum.h
ls boards/arm/stm32f1/stm32f103-minimum/src/stm32_userleds.c
```

若路径是 `boards/arm/stm32/...`（少一层 `f1`），以你机器上 `find` 为准：

```bash
find boards -name 'stm32f103_minimum.h'
find boards -name 'stm32_userleds.c'
```

下面用：

```text
BOARD=boards/arm/stm32f1/stm32f103-minimum
```

---

## 2. 改 LED 到 PB5

编辑 `$BOARD/src/stm32f103_minimum.h`，找到 `GPIO_LED1`（大约在 `#ifdef CONFIG_STM32F103MINIMUM_BLACKPILL` 那段），**整段改成**（战舰专用，简单粗暴）：

```c
/* WarShip F103: LED0 = PB5, active-low */
#define GPIO_LED1 (GPIO_OUTPUT|GPIO_CNF_OUTPP|GPIO_MODE_50MHz|\
                   GPIO_OUTPUT_SET|GPIO_PORTB|GPIO_PIN5)
```

（可删掉原来的 BLACKPILL / PC13 分支，或只改 `#else` 分支为上面宏。）

---

## 3. 低电平亮：翻转极性

编辑 `$BOARD/src/stm32_userleds.c`，把两处写 GPIO 改成取反：

`board_userled` 里：

```c
stm32_gpiowrite(g_ledcfg[led], !ledon);   /* was: ledon */
```

`board_userled_all` 里：

```c
stm32_gpiowrite(g_ledcfg[i], ((ledset & (1 << i)) != 0) ? false : true);
/* 即：bit=1 表示要点亮 → 输出 0 */
```

更简单的写法：

```c
bool on = (ledset & (1 << i)) != 0;
stm32_gpiowrite(g_ledcfg[i], !on);
```

---

## 4. menuconfig 打开 USERLED（你上次很可能没开成功）

若串口仍打印 `CONFIG_USERLED off -> printf-only`，说明 **编进去的固件里没有 `CONFIG_USERLED`**，灯不会被 `ledblink` 驱动。

```bash
cd ~/automotive-nuttx/nuttx

# 先看当前配置（把输出保存/对照）
grep -E 'CONFIG_USERLED|CONFIG_ARCH_LEDS|CONFIG_EXAMPLES_LEDBLINK' .config
```

然后：

```bash
make menuconfig
```

必做三项（按 `/` 搜索名字）：

| 搜索关键字 | 设置 |
|------------|------|
| `ARCH_LEDS` | **关掉**（Board Selection → Board LED Status support） |
| `USERLED` | **打开**（Device Drivers → LED Support → LED driver） |
| `USERLED_LOWER` | **打开**（Generic Lower Half LED Driver） |

保存退出后再查一次：

```bash
grep -E 'CONFIG_USERLED|CONFIG_ARCH_LEDS|CONFIG_EXAMPLES_LEDBLINK' .config
```

**必须**类似：

```text
CONFIG_USERLED=y
CONFIG_USERLED_LOWER=y
# CONFIG_ARCH_LEDS is not set
CONFIG_EXAMPLES_LEDBLINK=y
```

若 `CONFIG_USERLED` 仍不是 `=y`，不要 `make`，先把 `grep` 结果发出来。

强制让 apps 重编：

```bash
make -C ../apps distclean 2>/dev/null
make -j$(nproc)
# 编译日志里应出现 Register / 编到 userled；也可：
grep CONFIG_USERLED include/nuttx/config.h
```

---

## 5. 编译与烧录

```bash
make -j$(nproc)
ls -lh nuttx.bin
```

拷回 Windows → CubeProgrammer → `0x08000000`（仍覆盖 bootloader）。

---

## 6. 验收

```text
nsh> ledblink
```

期望：

```text
ledblink: start ...
ledblink: usleep() ...
# 不应再出现: CONFIG_USERLED off
ledblink: tick=1 led=1
...
```

同时战舰 **LED0（靠近标号的那个，PB5）** 约 2Hz 闪烁。

若 tick 有、灯不闪：

1. 确认改的是正在编译的那份 `stm32f103_minimum.h`  
2. 极性：若常亮/反了，把 `!ledon` 改回 `ledon` 试一次  
3. `ls /dev` 看是否有 `userleds`

---

## 7. 进度

| 项 | 状态 |
|----|------|
| 1. USERLED + 战舰 PB5 | **通过**（灯闪，无 USERLED off） |
| 2. work queue / 定时器 | 下一步 |
| 3. NuttX CAN | 再下一步 |

*验收：2026-10-03，`ledblink` + PB5 闪烁。*
