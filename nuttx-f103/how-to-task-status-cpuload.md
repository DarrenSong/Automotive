# 查看任务状态与耗时（NuttX NSH）

> 你当前 `help` 里没有 `ps`/`top`，是配置精简关掉了。按下面打开后可看：  
> **状态**（Ready / Waiting / Running）、**优先级**、**CPU 占用%**、（可选）**栈用量**。

---

## 1. 要开哪些配置

```bash
cd ~/automotive-nuttx/nuttx
make menuconfig
```

用 `/` 搜索并打开：

| 目的 | 符号（搜这些） | 菜单大致位置 |
|------|----------------|--------------|
| proc 文件系统 | `FS_PROCFS` = y | File Systems → Pseudo file systems → **procfs** |
| 不要排除进程信息 | `# CONFIG_FS_PROCFS_EXCLUDE_PROCESS is not set`（若有 EXCLUDE 相关，关掉） | 同上 |
| NSH 的 `ps` | 不要 `NSH_DISABLE_PS` | Application → NSH Library → Disable individual commands |
| CPU 负载采样 | `SCHED_CPULOAD` = y | RTOS Features → Tasks / Instrumentation |
| NSH 的 `top` | 依赖 procfs + CPULOAD；不要 `NSH_DISABLE_TOP` | 同上 NSH |
| 栈染色（看栈用了多少） | `STACK_COLORATION` = y | RTOS Features → Stack… |
| 自动挂载 /proc（可选） | `NSH_PROC_MOUNTPOINT` 或启动脚本里 mount | 见下 |

也可用命令核对：

```bash
grep -E 'CONFIG_FS_PROCFS|CONFIG_SCHED_CPULOAD|CONFIG_STACK_COLORATION|CONFIG_NSH_DISABLE_PS|CONFIG_NSH_DISABLE_TOP|EXCLUDE_PROCESS' .config
```

期望大致：

```text
CONFIG_FS_PROCFS=y
CONFIG_SCHED_CPULOAD=y
# CONFIG_NSH_DISABLE_PS is not set
# CONFIG_NSH_DISABLE_TOP is not set   # 若要用 top
CONFIG_STACK_COLORATION=y             # 可选但推荐
```

然后：

```bash
make -j$(nproc)
# 烧录
```

---

## 2. 挂载 procfs（每次上电或写进启动）

```text
nsh> mount -t procfs /proc
nsh> ls /proc
```

若希望开机自动挂，可在 board bringup / NSH 初始化里加同样一句，或开 NSH 相关 automount 选项（随版本搜 `PROCFS` / `MOUNTPOINT`）。

---

## 3. 看任务状态：`ps`

```text
nsh> ps
```

典型列（版本略有差异）：

| 列 | 含义 |
|----|------|
| PID | 任务/线程 ID |
| PRI | 优先级 |
| POLICY | FIFO / RR |
| TYPE | Task / Kthread / pthread |
| STATE | **Running / Ready / Waiting**… |
| EVENT | Waiting 时等什么（Semaphore、Signal…） |
| COMMAND | 名字（如 `nsh`、`Idle`、`lpwork`） |

你会看到类似：

- `Idle Task` — 空闲  
- `nsh` — shell（Running 时你正在敲命令）  
- `lpwork` / `hpwork` — 工作队列线程（`canspeed`/`wqblink` 的回调跑在这里）  
- 若某 builtin **前台阻塞**，会多一个该命令名的任务  

**注意：** `canspeed`/`wqblink` 默认 BG 模式时，**没有**长期占用的 `canspeed` 任务；周期逻辑在 **`lpwork`** 里，看 `lpwork` 的状态/CPU 即可。

---

## 4. 看耗时 / CPU：`top`

```text
nsh> top
```

依赖 `CONFIG_SCHED_CPULOAD`。一般会周期性刷新各任务 **CPU%**（采样估算，不是纳秒级 trace）。

退出：按提示（常见 `q`）或 Ctrl-C（若启用）。

没有 `top` 时也可：

```text
nsh> cat /proc/<pid>/loadavg
nsh> ls /proc
nsh> cat /proc/<pid>/status
```

（具体文件名随 NuttX 版本，以 `ls /proc` 为准。）

---

## 5. 「耗时」还能怎么理解

| 需求 | 做法 |
|------|------|
| 谁在跑、是否阻塞 | `ps` 看 STATE / EVENT |
| 大概占多少 CPU | `top` / CPULOAD |
| 某次回调花了多久 | 在代码里用 `clock_systime_ticks()` 前后相减，或 `perf`/`dumpme`（更重） |
| 栈有没有溢出风险 | `STACK_COLORATION` + `ps`/`top` 的栈列，或 `cat /proc/<pid>/stack` |

应用里简单打点示例：

```c
clock_t t0 = clock_systime_ticks();
/* ... 要测的代码 ... */
clock_t dt = clock_systime_ticks() - t0;
printf("dt=%lu ticks\n", (unsigned long)dt);
/* 毫秒约：dt * 1000 / CLOCKS_PER_SEC 或 CONFIG_USEC_PER_TICK */
```

---

## 6. 和你现有 demo 的对应

| 现象 | 在 ps/top 里看什么 |
|------|-------------------|
| 只跑 NSH | Idle + nsh + 可能的 lpwork/hpwork |
| `canspeed` BG | **没有**长期 canspeed 任务；`lpwork` CPU 略升、常 Ready/Waiting |
| `canspeed fg` | 多一个 `canspeed` 任务，STATE 多为 Running/Waiting(usleep) |
| `ledblink` 前台 | 有 `ledblink` 任务，NSH Waiting（等子进程/或被挡住） |
| `wqblink` | 同 canspeed BG，看 `lpwork` |

---

## 7. 内存（顺带）

```text
nsh> free          # 若已启用
```

未出现则在 menuconfig 打开 NSH `free` 命令 / 相关 MM 统计。

---

## 8. 相关文档

| 文档 | 内容 |
|------|------|
| `week07-nuttx-build-boot-sched.md` | 调度与任务谁创建 |
| `week07-workqueue-timer.md` | LPWORK 与专用任务差别 |
| 官方 | NSH Commands：`ps` / `top`；CPU Load measurement |
