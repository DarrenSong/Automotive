# P3 第 7 周 — NuttX 用户任务（周期闪灯 / 调度入门）

> 第 6 周已验收：`nsh>` + `help` / `uname -a`  
> 本周目标：自己的任务在 RTOS 里周期运行；理解与裸机 `while+delay` 的差别。

示例代码（Windows 仓库）：

`D:\Automotive\nuttx-f103\apps-overlay\examples\ledblink\`

---

## 1. 本周要搞懂的点

原理（编译流程、上电启动、调度器、`ledblink` 如何被 NSH 拉起）见：

**`week07-nuttx-build-boot-sched.md`**

| 裸机（app_boot） | NuttX |
|------------------|--------|
| `main` 里 `delay_ms` 死等 | 任务里 `usleep()`，睡眠时 **CPU 可跑别的任务（如 NSH）** |
| 只有一条执行流 | 多任务：NSH + ledblink |
| 优先级靠你自己排 | 任务有 **priority / stack** |

验收：

1. NSH 里能看到 builtin：`ledblink`  
2. 运行后周期打印 `ledblink: tick=...`  
3. （加分）灯硬件闪；或 `ledblink &` 后 NSH 仍能输入 `help`

---

## 2. 把示例拷到 Ubuntu

在 **Windows**（先保证 Ubuntu 能 SSH）：

```bat
scp -r D:\Automotive\nuttx-f103\apps-overlay\examples\ledblink darren@<IP>:~/automotive-nuttx/apps/examples/
```

在 **Ubuntu** 确认：

```bash
ls ~/automotive-nuttx/apps/examples/ledblink
# 应有: Kconfig Makefile Make.defs ledblink_main.c
```

若 `apps/examples` 的 Kconfig 不会自动收录子目录，检查是否已有自动生成；NuttX 13 apps 一般用目录扫描。拷贝后执行：

```bash
cd ~/automotive-nuttx/nuttx
make distclean   # 若之前配置很乱再清；否则可跳过
./tools/configure.sh -l stm32f103-minimum:nsh
make menuconfig
```

---

## 3. menuconfig 打开示例

### 3.0 菜单里找不到？先做这几步

`apps/Kconfig` 是工具**扫描子目录后生成**的，只 `scp` 不重新生成时，menuconfig 里不会出现 ledblink。

在 Ubuntu 上：

```bash
# 1) 确认文件在（你已通过）
ls -la ~/automotive-nuttx/apps/examples/ledblink

# 2) 关键：mkkconfig 只扫「当前目录的下一级 */Kconfig」
#    在 apps/ 下跑 → 只更新 apps/Kconfig（source examples/Kconfig）
#    必须在 examples/ 下再跑一次 → 才会把 ledblink 写进 examples/Kconfig
cd ~/automotive-nuttx/apps/examples
../tools/mkkconfig.sh -m "Examples"

# 3) 确认
grep -n ledblink Kconfig

# 4) 再进 menuconfig，按 / 搜索 LEDBLINK
cd ~/automotive-nuttx/nuttx
make menuconfig
```

`grep` 应能看到类似 `source ".../examples/ledblink/Kconfig"`。

**备用：** `grep` 已有 ledblink 之后，也可：

```bash
cd ~/automotive-nuttx/nuttx
echo 'CONFIG_EXAMPLES_LEDBLINK=y' >> .config
make olddefconfig
make -j$(nproc)
```

### 3.1 正常菜单路径

```text
Application Configuration
  -> Examples
       -> [*] LED blink user-task example (week7)     (EXAMPLES_LEDBLINK)
```

同时确认：

```text
Application Configuration
  -> NSH Library
       -> [*] Built-in applications support          (NSH_BUILTIN_APPS)
```

（`stm32f103-minimum:nsh` 通常已开 builtin。）

可选（真灯，见 §5）：

```text
Board Selection / Device Drivers
  -> [*] User LED support   (USERLED)     # 名称随版本略有不同
```

保存退出后：

```bash
make -j$(nproc)
ls -lh nuttx.bin
```

---

## 4. 烧录并运行

拷回 Windows → CubeProgrammer 烧到 `0x08000000`（仍会覆盖 bootloader，与上周相同）。

串口 115200，复位后：

```text
nsh> help
```

Builtin Apps 里应有 `ledblink`。

```text
nsh> ledblink
```

应周期出现：

```text
ledblink: tick=1 led=1
ledblink: tick=2 led=0
...
```

该任务占着前台时，NSH 被挡住是正常的。若支持后台：

```text
nsh> ledblink &
nsh> help
```

还能出 `help`，就说明 **多任务调度** 在工作。

结束后台任务（若支持）：`kill %1` 或重启。

---

## 5. 战舰 LED（加分，可后做）

裸机 LED0 = **PB5**（低电平亮，视原理图）。  
`stm32f103-minimum` 默认灯脚常是 **PC13（蓝药丸）**，和战舰不同。

路线：

1. **本周先 printf 版**（已够验收调度）  
2. 打开 `CONFIG_USERLED`，改板级 `stm32_userleds.c` / `board.h` 把 LED 映到 PB5  
3. 或后续用 GPIO 字符设备 `/dev/gpio*`

改板级路径大致在（随版本）：

```text
nuttx/boards/arm/stm32f1/stm32f103-minimum/src/
```

改完 `make` 重烧；`ledblink` 会尝试 `open("/dev/userleds")` 并 ioctl 翻转。

---

## 6. 和裸机代码对照（写进笔记）

裸机：

```c
while (1) {
  LED0 = !LED0;
  delay_ms(500);   // 空转等待
}
```

NuttX：

```c
while (1) {
  printf(...);
  usleep(500000);  // 任务阻塞休眠，调度器可运行 NSH
}
```

`menuconfig` 里可调：

- `EXAMPLES_LEDBLINK_PRIORITY`（默认 100）  
- `EXAMPLES_LEDBLINK_STACKSIZE`  
- `EXAMPLES_LEDBLINK_INTERVAL_MS`（默认 500）

---

## 7. 进度

| 项 | 状态 |
|----|------|
| 示例代码进仓库 | 已就绪（apps-overlay） |
| 拷到 Ubuntu + menuconfig | 已做（examples 下 mkkconfig） |
| 烧录含 ledblink 的镜像 | 已烧 |
| NSH 跑通 ledblink tick | **通过**（interval=500ms，printf-only） |
| 战舰 PB5 真灯 | **通过** → `week07-userled-pb5.md` |
| 双灯 PB5+PE5 | **通过** → `week07-two-leds.md` |
| 编译/启动/调度说明 | 见 `week07-nuttx-build-boot-sched.md` |
| Work queue / 定时器 | **通过** → `week07-workqueue-timer.md` + `wqblink` |

---

*第 7 周通过（2026-10-03）：ledblink / 双灯 / wqblink(LPWORK)。*  
*第 8 周：`week08-nuttx-can.md` + `canspeed`。*
