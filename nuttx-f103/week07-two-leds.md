# 双灯 ledblink（战舰 LED0=PB5，LED1=PE5）

**验收（2026-10-03，darren / NuttX 13.1.0-RC0）**

| 命令 | 现象 |
|------|------|
| `nsh> ledblink` | 两灯**轮流**闪，`mask=0x3`，打印 `mode=opposite` |
| `nsh> ledblink same` | 两灯**同相**闪，打印 `mode=same` / `both=0\|1` |

硬件：战舰 STM32F103ZET6，DS0=PB5、DS1=PE5，**低电平亮**。

---

## 0. 一次做对的最短路径（已验证）

### 0.1 放宽板级芯片依赖（必做，否则选 ZE 会崩）

`boards/Kconfig` 中 `ARCH_BOARD_STM32F103_MINIMUM`（约 3097 行）：

```text
# 原：depends on ARCH_CHIP_STM32F103C8
depends on ARCH_CHIP_STM32F103C8 || ARCH_CHIP_STM32F103ZE
```

### 0.2 板级三处

| 文件 | 改动 |
|------|------|
| `include/board.h` | `BOARD_NLEDS  2` |
| `src/stm32f103_minimum.h` | `GPIO_LED1`=PB5，`GPIO_LED2`=PE5，`GPIO_OUTPUT_SET`（初始灭） |
| `src/stm32_userleds.c` | `g_ledcfg[]={GPIO_LED1,GPIO_LED2}`；`board_userled` / `board_userled_all` 均 `stm32_gpiowrite(..., !ledon)` |

删掉旧的 `#ifdef BLACKPILL` 重复 `GPIO_LED1`，只留一套战舰宏。

### 0.3 配置 + 链接脚本

```bash
./tools/configure.sh -l stm32f103-minimum:nsh
# C8→ZE、关 ARCH_LEDS、开 USERLED + USERLED_LOWER + ledblink
# ld.script: flash 512K / sram 64K
mkdir -p scripts
ln -sfn ../boards/arm/stm32f1/stm32f103-minimum/scripts/ld.script scripts/ld.script
make -j$(nproc)
```

核对：

```bash
grep -E 'CONFIG_ARCH_BOARD_STM32F103_MINIMUM=y|CONFIG_ARCH_CHIP_STM32F103ZE=y|CONFIG_USERLED_LOWER=y|CONFIG_STM32_HIGHDENSITY=y' .config
```

### 0.4 应用

```bat
scp D:\Automotive\nuttx-f103\apps-overlay\examples\ledblink\ledblink_main.c darren@<IP>:~/automotive-nuttx/apps/examples/ledblink/
```

`ledblink` = 交替（0x1 / 0x2）；`ledblink same` = 同相（0x3 / 0x0）。

---

## 1. 本次踩坑全集（务必读）

### 坑 1：C8 没有 GPIOE，PE5 配了也不亮

| | |
|--|--|
| 现象 | `supported mask=0x3`，只有 PB5 那盏动，PE5 永远不亮 |
| 原因 | `stm32f103-minimum` 默认 `CONFIG_ARCH_CHIP_STM32F103C8`（LQFP48，无 PORTE） |
| 正确 | 战舰 ZET6 → 选 **STM32F103ZE**，拿到 `CONFIG_STM32_HIGHDENSITY` 与 GPIOE |

### 坑 2：板级 Kconfig 绑死 C8，直接选 ZE 会「拆家」

| | |
|--|--|
| 现象 | `make olddefconfig` / `menuconfig` 后 `CONFIG_ARCH_BOARD_STM32F103_MINIMUM` 消失；`menuconfig` 报 `No directory at .../nuttx//src`；`ld.script` 链接丢失 |
| 原因 | `boards/Kconfig`：`depends on ARCH_CHIP_STM32F103C8` —— 选 ZE 后面板选项被 Kconfig 清掉 |
| 正确 | **先**改 `depends on ...C8 \|\| ...ZE`，**再**切芯片 |
| 识别 | NuttX 13 板名是 `CONFIG_ARCH_BOARD_STM32F103_MINIMUM=y`（不是字符串 `CONFIG_ARCH_BOARD=`） |

### 坑 3：BOARD 一丢，符号链接全断

| 坏掉的链接 | 正常应指向 |
|------------|------------|
| `arch/arm/src/board` | `boards/.../common`（再经 `board/board` 到 minimum） |
| `scripts/ld.script` | `boards/.../stm32f103-minimum/scripts/ld.script` |
| `include/arch/board` 等 | 板级 / chip 头文件 |

`make distclean` 在链接已坏时也会失败（`board/Make.defs: No such file`）。  
恢复：**不要硬 distclean**，手工：

```bash
rm -f arch/arm/src/board arch/arm/src/chip scripts/ld.script .config .config.old
./tools/configure.sh -l stm32f103-minimum:nsh
```

板级源文件（`.h` / `stm32_userleds.c`）一般还在，configure 不会抹掉。

### 坑 4：`userled_lower_initialize` 未定义

| | |
|--|--|
| 现象 | 编译警告 implicit declaration；链接 `undefined reference to userled_lower_initialize` |
| 原因 | bringup 调了 lower-half，但只开了 `CONFIG_USERLED`，没开 **`CONFIG_USERLED_LOWER`** |
| 正确 | `CONFIG_USERLED=y` **且** `CONFIG_USERLED_LOWER=y`；`# CONFIG_ARCH_LEDS is not set` |

### 坑 5：`board_userled` 取反了，`board_userled_all` 没取反

| | |
|--|--|
| 现象 | 单灯 ioctl 可能正常，`ledblink`（走 `ULEDIOC_SETALL`）极性怪 / 看起来「不亮」 |
| 原因 | `ledblink` → `ULEDIOC_SETALL` → **`board_userled_all`**，不是 `board_userled` |
| 正确 | 两处都是 `stm32_gpiowrite(g_ledcfg[i], !ledon);`（战舰低电平亮） |

### 坑 6：烧的仍是旧 `ledblink`

| | |
|--|--|
| 现象 | 敲了 `ledblink same`，串口仍打印 `LED0/LED1 opposite blink`，same 无效 |
| 原因 | 板子上还是旧 ELF；新版应打印 `mode=same` / `mode=opposite` |
| 正确 | 重新 `scp` `ledblink_main.c` → `make` → 重烧后再看串口文案 |

### 坑 7：`ld.script` MEMORY 改错

| | |
|--|--|
| 现象 | `sed` 把 sram 也改成 512K（误伤） |
| 正确 | **flash = 512K，sram = 64K**（ZET6） |
| 另 | 改完板级 `ld.script` 后确认 `scripts/ld.script` 符号链接仍在 |

### 坑 8：重复的 `GPIO_LED1` 宏

旧 `#ifdef CONFIG_STM32F103MINIMUM_BLACKPILL` / `#else` 与战舰宏叠在一起，易看错脚。  
应注释/删掉旧分支，只留 PB5 + PE5 一套。

### 坑 9：`ARCH_LEDS` 与 `USERLED` 抢灯

开着 `CONFIG_ARCH_LEDS` 时，系统心跳可能占 LED，USERLED 表现异常。  
双灯实验：**关 ARCH_LEDS，开 USERLED + LOWER**。

---

## 2. 验收命令与期望输出

```text
nsh> ledblink
ledblink: mode=opposite (bits 0=PB5, 1=PE5)
ledblink: supported mask=0x3
ledblink: tick=1 led0=1 led1=0
...

nsh> ledblink same
ledblink: mode=same (bits 0=PB5, 1=PE5)
ledblink: supported mask=0x3
ledblink: tick=1 both=1
...
```

---

## 3. 配置核对清单（编译前）

```bash
grep -E 'CONFIG_ARCH_BOARD_STM32F103_MINIMUM=y|CONFIG_ARCH_CHIP_STM32F103(C8|ZE)=y|CONFIG_USERLED|CONFIG_ARCH_LEDS|CONFIG_STM32_HIGHDENSITY|CONFIG_EXAMPLES_LEDBLINK' .config
ls -la scripts/ld.script arch/arm/src/board
grep LENGTH boards/arm/stm32f1/stm32f103-minimum/scripts/ld.script
```

期望要点：

- `ARCH_BOARD_STM32F103_MINIMUM=y`
- `ARCH_CHIP_STM32F103ZE=y`（不要 C8）
- `STM32_HIGHDENSITY=y`
- `USERLED=y` + `USERLED_LOWER=y`
- `# CONFIG_ARCH_LEDS is not set`
- `ld.script` 链接存在；MEMORY 512K / 64K

---

## 4. 相关文件

| 路径 | 说明 |
|------|------|
| `apps-overlay/examples/ledblink/ledblink_main.c` | 应用（opposite / same） |
| `week07-userled-pb5.md` | 单灯 PB5 + USERLED |
| `week07-rtos-task.md` | 第 7 周任务总览 |
| `week07-nuttx-build-boot-sched.md` | 编译 / 启动 / 调度原理 |
| `README.md` | Ubuntu 构建 / 烧录 |
| 板级（Ubuntu） | `boards/arm/stm32f1/stm32f103-minimum/` |
| 板级 Kconfig 补丁 | `boards/Kconfig` → `ARCH_BOARD_STM32F103_MINIMUM` depends |
