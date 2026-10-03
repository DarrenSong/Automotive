# P3 — NuttX on 战舰 F103（起步）

> 上一阶段 P2（Bootloader / 串口 IAP）已完成。  
> 本阶段目标：在 F103 上跑起 **Apache NuttX**，串口进 **NSH**。  
> 总计划：`automotive-learning-plan.md` → P3  

---

## 0. 会不会覆盖 bootloader？

**会（按当前首次烧录方式）。**

| 镜像 | 默认链接/烧录地址 | 结果 |
|------|-------------------|------|
| 现有 bootloader（`bootDemo`） | `0x08000000`（32KB） | — |
| 现有 app_boot | `0x08008000` | — |
| **本次 NuttX（`stm32f103-minimum:nsh`）** | **`0x08000000` 起** | **整片从开头写 → 覆盖 bootloader（也可能盖住后面的 app）** |

因此：

1. **第一次跑通 NSH**：接受覆盖；用 ST-Link 直接烧 `nuttx.bin` / `nuttx.hex` 到 `0x08000000`。  
2. **还要串口 IAP**：烧完 NuttX 测完后，再 Keil 重烧 `bootDemo`（+ 需要时重烧 `app_boot`）。  
3. **以后（第 8 周加分）**：再改 NuttX 链接到 `0x08008000`，由 bootloader 跳转，那时才不占 boot 区。

本周策略：**先占 `0x08000000` 把 NSH 跑通，分区策略往后放。**

---

## 1. 和裸机工程的关系

| 项目 | 说明 |
|------|------|
| `bootDemo` / `app_boot` | 工程保留在 Windows；被覆盖后可随时重烧 |
| NuttX | Kconfig + Make，在 Ubuntu 上编，**不用 Keil 编内核** |
| Windows | ST-Link 烧录 + 串口看 NSH |

---

## 2. 编译环境

| 方案 | 说明 |
|------|------|
| **Ubuntu 24 laptop（主用）** | RedmiBook Pro 14，用户 **`darren`**，SSH 远程编译 |
| Firefly / WSL | 备选 |

Windows 登录示例：

```bat
ssh darren@<RedmiBook的IP或主机名>
```

产物目录（Windows）：`D:\Automotive\nuttx-f103\build\`

---

## 3. 第 6 周清单

- [x] 安装工具链与依赖（若 apt 卡住见 §3.1）
- [x] 获取 `nuttx` + `apps`（已 clone）
- [ ] `configure` + `make` 出 `nuttx.bin`
- [ ] ST-Link 烧录（知会覆盖 bootloader）
- [ ] 串口进 NSH：`help` / `free` / `uname -a`

**验收：** 复位后出现 NuttX banner + `nsh>`。

---

## 3.1 apt 卡住时（WARNING 后不动）

`WARNING: apt does not have a stable CLI interface` **可忽略**。  
若一直不继续：

1. 试按 `Y` + 回车（可能在等确认）  
2. 另开 SSH：`ps aux | grep -E 'apt|dpkg' | grep -v grep`  
3. 原窗口 `Ctrl+C` 后非交互重装：

```bash
sudo dpkg --configure -a
sudo apt update
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
  git flex bison gettext texinfo libncurses-dev \
  gperf automake libtool pkg-config build-essential \
  gcc-arm-none-eabi binutils-arm-none-eabi python3
```

---

## 4. 工具链与源码（Ubuntu / darren）

```bash
sudo apt update
sudo apt install -y \
  git flex bison gettext texinfo \
  libncurses-dev libgmp-dev libmpfr-dev libmpc-dev \
  gperf automake libtool pkg-config build-essential \
  gcc-arm-none-eabi binutils-arm-none-eabi \
  python3

arm-none-eabi-gcc --version

mkdir -p ~/automotive-nuttx && cd ~/automotive-nuttx
git clone https://github.com/apache/nuttx.git nuttx
git clone https://github.com/apache/nuttx-apps.git apps
ls nuttx apps
```

目录约定：

```text
~/automotive-nuttx/
  nuttx/     # 内核
  apps/      # 与 nuttx 同级、同版本
```

可选：两个仓库 checkout **同一** release tag（如 `nuttx-12.x.x`）。

---

## 5. 配置与编译（当前进度：clone 已完成 → 做这里）

```bash
cd ~/automotive-nuttx/nuttx

# 查看有哪些 F103 配置
find boards -path '*stm32f103*' -name defconfig | head -40

# 先用官方最小板 NSH（控制台 USART1 = 战舰 PA9/PA10）
./tools/configure.sh -l stm32f103-minimum:nsh

# 编译
make -j$(nproc)

# 产物
ls -lh nuttx.bin nuttx.hex 2>/dev/null
ls -lh nuttx
```

说明：

- `stm32f103-minimum` 默认按 **C8（较小 Flash/RAM）**；战舰是 **ZET6 512K/64K**。  
- **第一次目标只是进 `nsh>`**；编通后再用 `make menuconfig` 改高密度芯片与内存。  
- 若 `configure.sh` / `make` 报错，把完整日志留下。

可选改配置：

```bash
make menuconfig
# System Type -> 芯片密度 / Flash·RAM（按菜单）
# Serial -> USART1 console
# Application -> NSH
```

改完再 `make -j$(nproc)`。

---

## 6. 拷回 Windows

在 **Windows** PowerShell / CMD：

```bat
scp darren@<IP>:~/automotive-nuttx/nuttx/nuttx.bin D:\Automotive\nuttx-f103\build\
scp darren@<IP>:~/automotive-nuttx/nuttx/nuttx.hex D:\Automotive\nuttx-f103\build\
```

（有 `.hex` 再拷 hex；有的配置主要生成 `nuttx.bin`。）

---

## 7. 烧录与串口（会覆盖 bootloader）

### 7.1 先确认 Ubuntu 已编出文件

```bash
cd ~/automotive-nuttx/nuttx
ls -lh nuttx.bin nuttx.hex nuttx
```

有 `nuttx.bin` 或 `nuttx.hex` 再往下。

### 7.2 拷到 Windows

```bat
scp darren@<IP>:~/automotive-nuttx/nuttx/nuttx.bin D:\Automotive\nuttx-f103\build\
scp darren@<IP>:~/automotive-nuttx/nuttx/nuttx.hex D:\Automotive\nuttx-f103\build\
```

### 7.3 用 STM32CubeProgrammer 烧（推荐）

1. 战舰供电；ST-Link 接 SWD（建议带 **NRST**）  
2. 关掉 Keil / 其它占用 ST-Link 的软件  
3. 打开 **STM32CubeProgrammer**  
4. 右上角选 **ST-LINK** → 设置：  
   - Mode：**Under reset**  
   - Reset mode：**Hardware reset**  
5. **Connect**  
6. **Erasing & Programming**（或 Open file）：  
   - 选 `D:\Automotive\nuttx-f103\build\nuttx.bin`  
   - 若是 **.bin**：Start address 填 **`0x08000000`**  
   - 若是 **.hex**：地址已在文件里，一般不用改  
7. 勾选 Download / Verify（可选 Run after programming）  
8. **Start Programming**  
9. Disconnect  

**结果：bootloader 已被覆盖**（见 §0）。

### 7.4 串口看 NSH

- 助手：对应 COM，**115200 8N1**  
- 复位战舰，应出现类似：

```text
NuttShell (NSH) ...
nsh>
```

```text
help
free
uname -a
```

### 7.4.1 进了 nsh 但有「乱码」如 `^[K` / `←[K`

**不是烧坏了。** 那是 **VT100/ANSI 清行转义**（ESC `[` `K`）。  
普通串口助手不解析 ANSI，就会把控制符显示成乱码；提示符 `nsh>` 仍可用来打命令。

处理（二选一）：

1. **换终端（推荐）**：Tera Term / PuTTY / MobaXterm 等支持 ANSI 的工具，同样 115200。  
2. **关花哨行编辑后重编**：Ubuntu 上

```bash
cd ~/automotive-nuttx/nuttx
make menuconfig
# Application Configuration -> NSH Library
#   关掉 CONFIG_NSH_CLE（EMACS-like editor）若已打开
#   使用较简单的 readline 即可
make -j$(nproc)
```

再烧新的 `nuttx.bin`。多数情况只换终端就够。

试命令（直接输入回车）：

```text
help
uname -a
ls /
```

`free` 在 **minimum/nsh** 默认配置里常未编进 NSH（会报 `command not found`），属正常。若需要：

```bash
cd ~/automotive-nuttx/nuttx
make menuconfig
# Application Configuration -> NSH Library -> Disable Individual Commands
#   确保 free 未被 disable；或打开 Memory Management / procfs 相关选项
# 也可启用 procfs 后: mount -t procfs /proc 再 cat /proc/...
make -j$(nproc)
```

已验证示例：`help` 有命令表、`uname -a` 显示 `NuttX 13.1.0-RC0 ... stm32f103-minimum` → **第 6 周 NSH 验收通过**。

### 7.5 恢复 P2 Boot + IAP

```text
Keil 打开 bootDemo → Download bootloader
再 Download app_boot（若还要 APP）
```

Flash 算法仍用整片：`Start=0x08000000`，`Size=0x80000`。

---

## 8. 战舰引脚对照（后续 menuconfig）

| 外设 | 战舰 |
|------|------|
| 串口控制台 | USART1，PA9/PA10，115200 |
| LED | PB5 / PE5（USERLED 双灯已验，见 `week07-two-leds.md`） |
| CAN | PA11/PA12，P9 跳线（第 8 周） |
| Flash / SRAM | 512KB / 64KB（ZET6） |

---

## 9. 本周不要做的事

- 不要同时大改 boot/APP 分区（除非已决定长期用 NuttX 占满 Flash）  
- 不要一上来开以太网/USB 复杂组件  
- ESP32 / DDS 不挤占本阶段  

---

## 10. 进度

| 项 | 状态 |
|----|------|
| 工具链（Ubuntu24 / darren） | 已推进 |
| 源码 nuttx+apps | **已 clone** |
| configure + make | 已推进 |
| 烧录进 NSH | **已进 nsh>**（NuttX-13.1.0-RC0；串口可见 ANSI 转义属正常） |
| 双灯 USERLED（PB5+PE5） | **已验收**（`ledblink` 轮流 / `ledblink same` 同闪；ZE+Kconfig 坑见 `week07-two-leds.md`） |
| Work queue `wqblink` | **已验收**（LPWORK 周期 tick，main 返回后 NSH 仍可用） |
| NuttX CAN TX | **已验收**（`canspeed` 0x201 @ 500k） |
| NuttX CAN RX 控灯 | **已验收**（`canlight` 0x202 + loopback；见 `week08-nuttx-can.md`） |

---

## 11. 相关文档

- 总计划：`automotive-learning-plan.md` → P3  
- **编译 / 启动 / 调度**：`week07-nuttx-build-boot-sched.md`（configure→镜像、上电到 nsh、ledblink 如何被调度）  
- **开其他外设**：`week07-enable-peripherals.md`（menuconfig → 引脚 → bringup → app）  
- **`open("/dev/...")` 详解**：`week07-open-dev-userleds.md`（以 ledblink + userleds 为例）  
- **调用链到 GPIO**：`week07-callchain-open-to-gpio.md`（注册 → open → ioctl → 寄存器）  
- **Work queue / 定时器**：`week07-workqueue-timer.md` + `apps-overlay/examples/wqblink/`  
- **新增 App 要哪些文件**：`how-to-add-nuttx-app.md`  
- **第 8 周 CAN**：`week08-nuttx-can.md` + `canspeed` / `canlight`  
- **任务状态 / CPU**：`how-to-task-status-cpuload.md`（`ps` / `top` / procfs）  
- **P4 SocketCAN**：`../hikey970-socketcan/`（970 + 战舰联调）  
- 双灯踩坑：`week07-two-leds.md`（C8/ZE、Kconfig depends、USERLED_LOWER、链接断裂等）  
- 裸机/IAP：`bootloader\iap-demo-readme.md`  
- Flash 算法：`bootloader\flash-algorithm-fix.md`  
- 战舰踩坑：`stm32-warship-troubleshooting.md`  

---

*编译主机：Ubuntu 24 / darren @ RedmiBook Pro 14。*  
*第 6 周 NSH 已通。第 7 周：`week07-rtos-task.md` + `apps-overlay/examples/ledblink/`。*
