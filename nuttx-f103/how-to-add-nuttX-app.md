# 新增一个 NuttX App 要加哪些文件

> 以本仓库 `apps-overlay/examples/ledblink`、`wqblink` 为模板。  
> 目标树：Ubuntu `~/automotive-nuttx/apps/examples/<你的名字>/`  
> 联编原理见 `week07-nuttx-build-boot-sched.md`（`Application.mk` → builtin → `libapps.a`）。

---

## 1. 最少四个文件（examples 下）

在 `apps/examples/myapp/` 新建：

| 文件 | 作用 |
|------|------|
| **`Kconfig`** | menuconfig 开关、程序名/优先级/栈等配置项 |
| **`Make.defs`** | 打开配置时，把本目录加入 `CONFIGURED_APPS` |
| **`Makefile`** | `PROGNAME` / `PRIORITY` / `STACKSIZE` / `MAINSRC`，并 `include Application.mk` |
| **`myapp_main.c`** | 业务代码；入口写 `main()`（flat 构建会被改成 `myapp_main`） |

没有这四个，要么 menuconfig 看不见，要么编不进 `libapps.a`，要么 NSH 没有命令。

---

## 2. 各文件写什么（可抄模板）

假设应用名 **`myapp`**，配置前缀 **`EXAMPLES_MYAPP`**。

### 2.1 `Kconfig`

```kconfig
config EXAMPLES_MYAPP
	tristate "My app description"
	default n
	---help---
		Short help text.

if EXAMPLES_MYAPP

config EXAMPLES_MYAPP_PROGNAME
	string "Program name"
	default "myapp"

config EXAMPLES_MYAPP_PRIORITY
	int "myapp task priority"
	default 100

config EXAMPLES_MYAPP_STACKSIZE
	int "myapp stack size"
	default DEFAULT_TASK_STACKSIZE

endif
```

有依赖时加，例如：

```kconfig
	depends on SCHED_LPWORK || SCHED_HPWORK
	depends on USERLED
```

### 2.2 `Make.defs`

```make
ifneq ($(CONFIG_EXAMPLES_MYAPP),)
CONFIGURED_APPS += $(APPDIR)/examples/myapp
endif
```

### 2.3 `Makefile`

```make
include $(APPDIR)/Make.defs

PROGNAME  = $(CONFIG_EXAMPLES_MYAPP_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_MYAPP_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_MYAPP_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_MYAPP)

MAINSRC = myapp_main.c

include $(APPDIR)/Application.mk
```

`Application.mk` 会：编译、`-Dmain=myapp_main`、**Register** 写 `builtin/registry/myapp.bdat`。

### 2.4 `myapp_main.c`

```c
#include <nuttx/config.h>
#include <stdio.h>

int main(int argc, FAR char *argv[])
{
  printf("myapp: hello\n");
  return 0;
}
```

多个 `.c` 时：继续用 `MAINSRC` 表示带 `main` 的那个；其它源文件用 `CSRCS = foo.c bar.c`（与同目录其它 example 一致即可）。

---

## 3. 目录放好之后还要做的三步

### 步骤 A：让 `examples/Kconfig` 扫到你

`apps/Kconfig` / `examples/Kconfig` 常由工具生成，**只拷目录不够**。

```bash
cd ~/automotive-nuttx/apps/examples
../tools/mkkconfig.sh -m "Examples"
grep -n myapp Kconfig
```

应出现 `source ".../myapp/Kconfig"`。

### 步骤 B：menuconfig 打开

```bash
cd ~/automotive-nuttx/nuttx
make menuconfig
# Application Configuration → Examples → [*] My app ...
# 或 / 搜索 MYAPP
```

确认：

```bash
grep CONFIG_EXAMPLES_MYAPP=y .config
```

### 步骤 C：编译烧录

```bash
make -j$(nproc)
# 烧录 nuttx 后：
# nsh> help          # Builtin Apps 里应有 myapp
# nsh> myapp
```

编译日志里常有：`Register: myapp`。

---

## 4. 从 Windows 仓库同步到 Ubuntu

本仓库把示例放在：

```text
D:\Automotive\nuttx-f103\apps-overlay\examples\<app名>\
```

拷贝（**一个**冒号）：

```powershell
scp -r D:\Automotive\nuttx-f103\apps-overlay\examples\myapp darren@RedmiBook-Pro-14:~/automotive-nuttx/apps/examples/
```

然后做上面的 **mkkconfig → menuconfig → make**。

---

## 5. 文件关系总览

```text
apps/examples/myapp/
├── Kconfig          ← menuconfig 选项
├── Make.defs        ← 加入 CONFIGURED_APPS
├── Makefile         ← PROGNAME + Application.mk → Register
└── myapp_main.c     ← main() → 镜像里 myapp_main

mkkconfig 之后：
apps/examples/Kconfig  source 了 myapp/Kconfig

make 之后：
apps/builtin/registry/myapp.bdat
apps/builtin/registry/myapp.pdat
→ 汇总进 builtin_list.h
→ 链进 libapps.a → nuttx ELF
→ nsh> myapp
```

---

## 6. 可选 / 易漏

| 项 | 说明 |
|----|------|
| `depends on xxx` | 依赖未开时菜单里选不了或发灰 |
| 再跑 `mkkconfig` | 改名、新建目录后必须再扫 |
| `PROGNAME` 与命令名 | NSH 敲的就是 PROGNAME 字符串 |
| 不要手写全局 `main` 符号冲突 | 交给 `-Dmain=xxx_main` |
| 非 examples | 也可放 `apps/system/`、`apps/industry/` 等，同样四件套 + 该层 mkkconfig |
| 自定义整个 apps 树 | 改 `CONFIG_APPS_DIR`，见官方 custom apps 文档 |

---

## 7. 对照本仓库已有示例

| App | 目录 | 额外依赖 |
|-----|------|----------|
| ledblink | `apps-overlay/examples/ledblink/` | 可选 USERLED |
| wqblink | `apps-overlay/examples/wqblink/` | `SCHED_LPWORK`（或 HPWORK） |
| canspeed | `apps-overlay/examples/canspeed/` | `CAN` + LPWORK |
| canlight | `apps-overlay/examples/canlight/` | `CAN` + `USERLED` |

新建时直接复制其中一个目录改名，改 `Kconfig`/`Makefile`/`Make.defs` 里的前缀和文件名最快。

---

## 8. 检查清单（复制用）

- [ ] 四文件：`Kconfig` `Make.defs` `Makefile` `*_main.c`
- [ ] `examples` 下执行 `mkkconfig.sh`，`grep` 得到 source
- [ ] menuconfig 打开，`.config` 有 `=y`
- [ ] `make` 出现 `Register: <progname>`
- [ ] `nsh> help` 的 Builtin Apps 含该命令
- [ ] `nsh> <progname>` 能跑
