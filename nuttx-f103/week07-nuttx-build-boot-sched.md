# NuttX：编译流程 · 启动流程 · 任务调度（战舰 / ledblink）

> 结合当前环境：`stm32f103-minimum` + NuttX 13.1 + `ledblink`（USERLED PB5/PE5）  
> 相关：`week07-rtos-task.md`、`week07-two-leds.md`、`apps-overlay/examples/ledblink/`

---

## 0. 一句话主线

**配置决定「编谁」→ 编译把板级驱动 + 内核 + 应用链成一个 `nuttx` 镜像 → 上电后内核起调度器 → NSH 按命令创建并调度你的 `ledblink` 任务。**

---

## 1. 源码树里都有什么

```text
~/automotive-nuttx/
├── nuttx/          # 内核 + 芯片驱动 + 板级
│   ├── sched/      # 调度、任务、信号量…
│   ├── arch/arm/src/stm32f1/   # F1 芯片：GPIO / UART / 时钟…
│   ├── boards/arm/stm32f1/stm32f103-minimum/  # 「这块板」：引脚、bringup、USERLED
│   └── drivers/    # 通用驱动框架（含 userled lower-half）
└── apps/           # 应用（独立仓库，与内核分开）
    ├── nshlib/     # NSH shell
    └── examples/ledblink/   # 本周 app
```

「包里包含这块板子的驱动」更准确的拆法：

| 层次 | 位置 | 职责 |
|------|------|------|
| 芯片驱动 | `arch/.../stm32f1`（及 STM32 共用层） | GPIO / USART / RCC 等寄存器级 |
| 板级适配 | `boards/.../stm32f103-minimum` | PB5/PE5、串口脚、`stm32_bringup` |
| 通用框架 | `drivers/leds` 等 | `userled_lower_initialize` → `/dev/userleds` |
| 应用 | `apps/examples/ledblink` | 业务：`open` / `ioctl` / `usleep` |

`.config`（menuconfig）只决定：**哪些源文件进编译、哪些能力打开**。

---

## 2. 编译流程（configure → 烧录）

```text
configure.sh (stm32f103-minimum:nsh)
        ↓
     .config（芯片 / 板 / 驱动 / app 开关）
        ↓
   make 编内核+驱动+板级     +     make 编 apps（ledblink / nsh / sh）
        ↓
   链接成单个 nuttx ELF（Flash 里一份镜像）
        ↓
   烧进 0x08000000
```

### 2.0 两个 Git 仓库如何编成「一个」二进制（nuttx + apps）

`apache/nuttx` 与 `apache/nuttx-apps`（你机器上的 `nuttx/`、`apps/`）**仓库分离，构建时约定并排目录**，由 nuttx 顶层 Make **调用** apps，再把产物链进同一个 ELF。

#### 目录约定

```text
~/automotive-nuttx/
├── nuttx/     # 内核仓库；在这里执行 make
└── apps/      # 应用仓库；默认相对路径 ../apps
```

`.config` 里（Kconfig 符号 `APPS_DIR`）：

```text
CONFIG_APPS_DIR="../apps"
```

`configure.sh` / `make` 会据此设置 **`APPDIR`**（apps 绝对或相对路径）。  
nuttx **不 submodule 强制绑死** apps；只要路径对、apps 顶层有合格 `Makefile`，就能联编。也可用 `./tools/configure.sh -a ../apps ...` 或换成别的应用目录。

#### 配置期：一份 `.config`，两套 Kconfig

- 主 `.config` 只在 **`nuttx/`** 下生成。
- nuttx 顶层 `Kconfig` 有类似：

  ```kconfig
  menu "Application Configuration"
  source "$APPSDIR/Kconfig"
  ```

- 因此 `make menuconfig` 能同时改芯片/驱动 **和** `EXAMPLES_LEDBLINK` 等 apps 选项；apps 并不单独再跑一套 menuconfig。

#### 编译期：nuttx Make 递归进 apps → `libapps.a`

在 **`nuttx/`** 里执行 `make` 时，大致顺序：

1. 编内核侧库：`libsched.a`、`libarch.a`、`libdrivers.a`、`libboard.a`、`libfs.a`…（落到 `nuttx/staging/`）
2. **`$(MAKE) -C $(APPDIR)`**（进入 `../apps`），并传入 `TOPDIR=<nuttx路径>`  
   - apps 用 nuttx 的 `.config`、工具链、头文件  
   - 各 `CONFIGURED_APPS`（含 `examples/ledblink`）编成 `.o`  
   - **打进静态库 `libapps.a`**（通常出现在 `apps/` 顶或同步到 `nuttx/staging/libapps.a`）
3. builtin 登记（`Register: ledblink`）也在 apps 构建里完成（见 §5.1）

apps 顶层 `Makefile` 必须支持 nuttx 期望的目标：`libapps.a`、`context`、`depend`、`clean`、`distclean` 等——这是两个仓库的「契约」。

#### 链接期：Flat 构建 = 一个大 blob

你当前是 **`CONFIG_BUILD_FLAT`（平坦地址空间）**：

```text
arm-none-eabi-ld ... \
  staging/libsched.a \
  staging/libarch.a \
  staging/libdrivers.a \
  staging/libboard.a \
  staging/libapps.a \    ← apps 仓库的全部所选应用/NSH
  ... \
  -o nuttx                 ← 唯一 ELF，烧这一份即可
```

- **没有**「先烧内核再烧 app」两份固件。  
- `ledblink_main`、`nsh_main` 等符号都在 **`libapps.a`** 里，与内核同一链接地址空间。  
- 若走 Protected / Kernel 构建，会变成多镜像或可加载模块——战舰 minimum/nsh **不是**这条路。

#### 一张关系图

```text
  git: apache/nuttx              git: apache/nuttx-apps
           │                              │
           ▼                              ▼
        nuttx/                         apps/
           │                              │
           │  CONFIG_APPS_DIR=../apps     │
           │  menuconfig source apps/Kconfig
           │                              │
           │  make -C ../apps TOPDIR=...  │
           │ ─────────────────────────────► 编译 → libapps.a
           │                              │
           │  staging/*.a + libapps.a     │
           └────────── ld ───────────────► nuttx（单个二进制）
```

#### 本机可核对

```bash
cd ~/automotive-nuttx/nuttx
grep CONFIG_APPS_DIR .config          # 期望 "../apps"
ls -la staging/libapps.a staging/libsched.a
arm-none-eabi-nm staging/libapps.a | grep ledblink_main
# 链接产物
ls -la nuttx
```

看到 `libapps.a` 里有 `ledblink_main`，且最终只有一个 `nuttx` ELF，就是「两仓库 → 一二进制」已接通。

### 2.1 `./tools/configure.sh -l stm32f103-minimum:nsh`

- 拷贝该板 `defconfig` → `.config`
- 建立符号链接（如 `arch/.../board`、`scripts/ld.script`）
- Make 才知道板目录、链接脚本在哪

### 2.2 `.config` 里和你相关的项（示例）

- 芯片 `STM32F103ZE`、板 `STM32F103_MINIMUM`（战舰需先改 Kconfig depends，见 `week07-two-leds.md`）
- `USERLED` + `USERLED_LOWER`
- `EXAMPLES_LEDBLINK`（及 priority / stack / interval）

打开后，对应 `.c` 才会进入 `libarch` / `libboard` / `libdrivers` / apps。

### 2.3 `make` 实际在干什么

1. 编译 **arch + sched + fs + drivers + board** → 多个 `.a`  
   （例如 `libboard.a` 含 `stm32_userleds.o`、`stm32_bringup.o`）
2. 编译 **apps**：`ledblink_main.c` 按 Makefile 的 `PROGNAME/PRIORITY/STACKSIZE` 登记为 NSH **builtin**
3. **一次链接** 成 `nuttx`（不是「驱动一个 hex、app 另一个 hex」）

### 2.4 和裸机 Keil 对比

| 裸机（app_boot） | NuttX |
|------------------|--------|
| 一个工程里几个 `.c` | 按 Kconfig 从大量文件中挑选再链接 |
| 通常一个 `main` | 内核 + 多个可调度任务入口 |
| 驱动常手写在工程里 | 芯片驱动 + 板级 + 框架分层 |

---

## 3. 上电启动流程（到 `nsh>`）

Cortex-M3 / STM32 上简化时间线：

```text
复位
  → 硬件取复位向量（启动文件 / 异常向量表）
  → C 运行时：清 BSS、拷 DATA
  → nx_start()                    ← NuttX 内核入口
       ├─ 初始化调度器、空闲任务 Idle
       ├─ 初始化内存、系统节拍（SysTick 等）
       ├─ 架构 / 板级早期初始化
       ├─ 驱动与 VFS 框架
       └─ 创建并启动「初始任务」
            → nsh 配置下进入 NSH（nsh_main / 带 NSH 的 init）
                 → stm32_bringup()
                      其中：userled_lower_initialize("/dev/userleds")
                 → 挂上 USART 控制台
                 → 打印 NuttShell，进入读命令循环 → nsh>
```

要点：

- **UART**：控制台，所以能敲命令  
- **bringup**：挂好 `/dev/userleds` 等设备节点  
- **`ledblink` 不会自动跑**——只是编进镜像并登记在 NSH 命令表，**等你输入才创建任务**

---

## 4. 任务调度怎么工作

NuttX 是多任务内核（优先级抢占；时间片视配置）。每个任务有独立栈与优先级。

### 4.1 和你 ledblink 相关的属性

| 属性 | 典型值（Kconfig） | 含义 |
|------|-------------------|------|
| 优先级 | `PRIORITY=100` | 调度优先级 |
| 栈 | `STACKSIZE=2048` | 该任务独立栈 |
| 状态 | Ready / Running / Waiting | `usleep` 时多为 Waiting |

来自 `apps/examples/ledblink/Makefile`：

```makefile
PROGNAME  = $(CONFIG_EXAMPLES_LEDBLINK_PROGNAME)   # 默认 ledblink
PRIORITY  = $(CONFIG_EXAMPLES_LEDBLINK_PRIORITY)     # 默认 100
STACKSIZE = $(CONFIG_EXAMPLES_LEDBLINK_STACKSIZE)   # 默认 2048
MAINSRC   = ledblink_main.c
```

### 4.2 调度直觉

```text
CPU 同一时刻只跑一个任务（Running）
  ├─ Idle       最低优先级；无事可跑 / 省电
  ├─ NSH        等串口输入时阻塞，让出 CPU
  └─ ledblink   亮灯 → usleep(500ms) → 再亮…
                  usleep 期间：ledblink 睡眠，NSH / Idle 可运行
```

**`usleep(500000)` ≠ 裸机 `delay_ms` 死等**：  
睡眠时任务进入等待队列，调度器切换到其它就绪任务；超时后被唤醒，继续 `while(1)`。

系统节拍（SysTick 等）周期性进入内核，处理超时唤醒、可能的抢占。

### 4.3 与裸机对比

| 裸机 | NuttX |
|------|--------|
| 一条执行流，`delay` 占死 CPU | 多任务；睡眠让出 CPU |
| 「并发」靠中断里干活 | 任务 + 中断 + 可选 work queue |
| 优先级靠自己排 | 任务 priority / 调度器 |

---

## 5. `ledblink` 是怎么被调度起来的

### 5.1 编译期：`Application.mk` 如何登记（builtin）

你的 `examples/ledblink/Makefile` 只写了变量，真正「登记」在 `include $(APPDIR)/Application.mk` 里完成。

#### 5.1.1 你这边提供什么

```makefile
# apps/examples/ledblink/Makefile
PROGNAME  = $(CONFIG_EXAMPLES_LEDBLINK_PROGNAME)  # "ledblink"
PRIORITY  = $(CONFIG_EXAMPLES_LEDBLINK_PRIORITY)    # 100
STACKSIZE = $(CONFIG_EXAMPLES_LEDBLINK_STACKSIZE)  # 2048
MODULE    = $(CONFIG_EXAMPLES_LEDBLINK)             # y
MAINSRC   = ledblink_main.c

include $(APPDIR)/Application.mk
```

`Make.defs` 在 `CONFIG_EXAMPLES_LEDBLINK=y` 时把该目录加入 `CONFIGURED_APPS`，才会编到这个 Makefile。

#### 5.1.2 Flat 构建：把 `main` 改名成 `ledblink_main`

Flat 镜像（你现在的用法，非 KERNEL 可加载模块）下，`Application.mk` 编译 `MAINSRC` 时加宏：

```text
-Dmain=ledblink_main
```

因此源码里写的 `int main(...)`，目标文件里符号是 **`ledblink_main`**，避免和别的 app 的 `main` 冲突，又能链进同一个 `nuttx`。

（命令名里的 `-` 会先变成 `_` 再拼 `_main`，例如 `my-app` → `my_app_main`。）

#### 5.1.3 `register` 目标：写出 `.bdat` / `.pdat`

`PROGNAME` 非空且非 module 构建时，`DO_REGISTRATION=y`，`Application.mk` 会：

```text
REGLIST = .../apps/builtin/registry/ledblink.bdat
register:: $(REGLIST)
```

生成规则里调用 apps 顶层的 **`REGISTER` 宏**（定义在 `apps/Make.defs`），大意是：

```make
# 伪代码，与 apps/Make.defs 中 REGISTER 等价
echo '{ "ledblink", 100, 2048, ledblink_main },' > builtin/registry/ledblink.bdat
echo 'int ledblink_main(int argc, char *argv[]);' > builtin/registry/ledblink.pdat
touch builtin/registry/.updated
```

也就是说：**登记 = 往 `apps/builtin/registry/` 写一条「命令名 + 优先级 + 栈 + 入口函数」记录**。  
编译时你常看到的 `Register: ledblink` 就是这一步。

`Application.mk` 里对应逻辑（概念摘录）：

```make
$(call REGISTER,$(PROGNAME),$(PRIORITY),$(STACKSIZE),$(PROGSYM)_main)
```

#### 5.1.4 汇总进 `builtin_list.h` / `builtin_proto.h`

`apps/builtin` 的构建会把所有 `registry/*.bdat` 拼进 **`builtin_list.h`**，所有 `*.pdat` 拼进 **`builtin_proto.h`**，得到类似：

```c
/* builtin_list.h 中的一项（示意） */
{ "ledblink", 100, 2048, ledblink_main },

/* builtin_proto.h */
int ledblink_main(int argc, char *argv[]);
```

这就是 NSH / `exec_builtin()` 查找的**内置应用表**。

#### 5.1.5 运行时谁用这张表

```text
nsh> ledblink
  → nsh_builtin()          （需 CONFIG_NSH_BUILTIN_APPS=y）
    → exec_builtin("ledblink", argv, ...)
      → 在 builtin 表里按名字匹配
      → task_create / 等价接口，入口 = ledblink_main，prio/stack 用表里的值
```

因此：`help` 能列出 `ledblink`，但**上电不会自动跑**——表只负责「能查到、能拉起」。

#### 5.1.6 和 KERNEL 模块构建的区别（了解即可）

若 `CONFIG_BUILD_KERNEL` 或 `MODULE=m`，`Application.mk` 走 **可加载 ELF** 路径（`BUILD_MODULE=y`），此时 **`DO_REGISTRATION=n`**，不写上述 `.bdat`，而是单独产出 bin；你当前 minimum/nsh **flat 构建不走这条路**。

#### 5.1.7 本机可核对

```bash
# Ubuntu，编过一次之后
ls ~/automotive-nuttx/apps/builtin/registry/ledblink.*
cat ~/automotive-nuttx/apps/builtin/registry/ledblink.bdat
# 期望类似: { "ledblink", 100, 2048, ledblink_main },

grep -n ledblink ~/automotive-nuttx/apps/builtin/builtin_list.h
```

### 5.2 运行期：敲命令 → 新任务

```text
串口输入: ledblink
        ↓
NSH 解析，在 builtin 表找到 ledblink
        ↓
创建任务（task_create / posix_spawn 一类）
  入口 = ledblink_main，prio / stack 来自配置
        ↓
任务就绪 → 调度器选中运行
        ↓
ledblink_main:
  open("/dev/userleds")        → 驱动 → board_userled_*
  ioctl(ULEDIOC_SETALL, ...)   → board_userled_all() → GPIO PB5/PE5
  usleep(500ms)                → 让出 CPU
  循环…
```

若支持后台：`ledblink &` → NSH 立刻回提示符，两个任务并存（可继续敲 `help`，灯仍闪）。

### 5.3 谁创建、何时创建

| 任务 | 谁创建 | 何时 |
|------|--------|------|
| Idle | 内核 | 启动必有 |
| NSH / init | 启动配置 | 上电后自动 |
| **ledblink** | **NSH（你的命令）** | **输入 `ledblink` 之后** |

结论：**驱动在 bringup 时挂好设备；app 在你敲命令时才变成可调度任务。**

### 5.4 调用链（灯）

```text
ledblink 任务
  → open/ioctl(/dev/userleds)
    → userled 驱动
      → board_userled / board_userled_all
        → stm32_gpiowrite(PB5 / PE5)   # 战舰低电平亮，写 !ledon
```

---

## 6. 总览图

```text
Flash 中的 nuttx 镜像
├── 内核调度器
├── STM32F1：GPIO / USART / …
├── 板级：PB5、PE5、bringup → /dev/userleds
├── NSH
└── ledblink 代码 + NSH 命令表项
         │
上电 ──► 内核启动 ──► bringup 挂设备 ──► nsh>
         │
输入 ledblink ──► 创建任务 ──► open/ioctl 灯 ──► usleep 睡眠
         │                              ▲
         └──────── 调度器在多任务间切换 ─┘
```

---

## 7. 和你已改内容的对应

| 你改过的东西 | 落在哪一段 |
|--------------|------------|
| `boards/Kconfig` 允许 ZE | 配置：芯片与板能同时选中 |
| `GPIO_LED*` / `stm32_userleds.c` | 板级驱动；bringup + ioctl |
| `USERLED_LOWER` | 板级 LED ↔ `/dev/userleds` |
| `ledblink_main.c` | 用户任务逻辑；NSH 创建后被调度 |
| `usleep(500ms)` | 主动让出 CPU（体现调度，而非死循环占满） |

---

## 8. 下一步（调度相关）

当前写法：**独立任务 + `usleep` 周期循环**。  

第 7 周后半可选另一种常见路径：

- **定时器 / work queue**：回调挂在内核工作队列或定时器上，不一定长期占一个用户任务  

两者对比可在做 work queue 时再补一节。

---

## 9. 相关文档

| 文档 | 内容 |
|------|------|
| `week07-rtos-task.md` | 第 7 周操作与验收 |
| `week07-userled-pb5.md` | 单灯 USERLED |
| `week07-two-leds.md` | 双灯 + C8/ZE 等踩坑 |
| `README.md` | Ubuntu 构建 / 烧录 |
| `automotive-learning-plan.md` | P3 总计划 |
