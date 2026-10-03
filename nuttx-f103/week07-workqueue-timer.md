# 第 7 周 · Work Queue / 定时器（对比 ledblink）

> 目标：理解 **专用任务 + `usleep`** 与 **工作队列延迟回调** 的差别；会开 `LPWORK`、跑通 `wqblink`。  
> 代码：`apps-overlay/examples/wqblink/`  
> 已完成：`ledblink` 双灯（PB5/PE5）

---

## 1. 三种「延时做事」怎么选

| 方式 | 谁在跑回调/循环 | 典型用途 |
|------|-----------------|----------|
| **任务 + `usleep`**（ledblink） | 你的任务一直活着 | 用户态业务、可阻塞、逻辑长 |
| **Work queue + delay**（本周） | 内核 **worker 线程** | 驱动底半部、周期轻活、不想占专用任务 |
| **`wd_start` 看门狗定时器** | 定时器到期后常再丢进 work queue | 更底层的 tick 定时；**回调里别干重活** |

关系（简化）：

```text
work_queue(LPWORK, &work, worker, arg, MSEC2TICK(500))
        │
        ├─ delay==0 → 尽快在 worker 线程执行 worker()
        └─ delay>0  → 内部用 wd_* 定时，到期后再让 worker 线程执行
```

所以学 work queue，也就摸到了 NuttX 里最常用的「软件定时 + 延后执行」路径。

**本周主练 LPWORK**（低优先级工作队列）。  
`HPWORK` 留给驱动中断底半部，应用里周期闪灯请用 **`LPWORK`**，避免拖实时性。

---

## 2. 和 ledblink 的核心差别

```text
ledblink:
  任务 while(1) { ioctl灯; usleep(500ms); }
  → 任务栈一直在；睡着时占着一个任务控制块

wqblink:
  main: work_queue(..., delay=500ms); return;
  worker线程到点:  ioctl灯; 再 work_queue(..., delay=500ms);
  → main 可以退出；闪灯在 LPWORK worker 上继续
```

验收体感：

```text
nsh> wqblink
wqblink: started ...
wqblink: main returns; blink runs on worker thread
nsh>                          ← 立刻回到提示符（不必 &）
wqblink: worker tick=1 ...
nsh> help                     ← 同时可敲命令
nsh> wqblink stop
```

---

## 3. 拷贝示例到 Ubuntu

Windows：

```bat
scp -r D:\Automotive\nuttx-f103\apps-overlay\examples\wqblink darren@<IP>:~/automotive-nuttx/apps/examples/
```

Ubuntu：

```bash
ls ~/automotive-nuttx/apps/examples/wqblink
# Kconfig Makefile Make.defs wqblink_main.c

cd ~/automotive-nuttx/apps/examples
../tools/mkkconfig.sh -m "Examples"
grep -n wqblink Kconfig
```

---

## 4. menuconfig

```bash
cd ~/automotive-nuttx/nuttx
make menuconfig
```

### 4.1 打开工作队列（若尚未开启）

```text
RTOS Features  --->
  Work queue support / Tasks and Scheduling（名称随版本）
    [*] High priority work queue     CONFIG_SCHED_HPWORK   （驱动常用，可已开）
    [*] Low priority work queue      CONFIG_SCHED_LPWORK   ← 本 demo 需要
```

也可用搜索：`/` → `LPWORK` / `SCHED_LPWORK`。

核对：

```bash
grep -E 'CONFIG_SCHED_(HP|LP)WORK=y' .config
```

至少要有 **`CONFIG_SCHED_LPWORK=y`**（若只有 HPWORK，则 `LPWORK` 宏常重定向到 HPWORK，能编过但不推荐拿 HP 做闪灯）。

### 4.2 打开示例

```text
Application Configuration  --->
  Examples  --->
    [*] Work-queue periodic blink (wqblink)
```

保存后：

```bash
grep CONFIG_EXAMPLES_WQBLINK=y .config
make -j$(nproc)
```

烧录（与之前 NuttX 相同）。

---

## 5. NSH 怎么用

```text
nsh> wqblink
nsh> help
nsh> wqblink stop
```

期望：

- 启动后 **马上** 回到 `nsh>`  
- 约每 500ms 一行 `wqblink: worker tick=...`  
- 双灯已配好时，两灯同相闪（与 `ledblink same` 类似）  
- `stop` 后不再打印，灯灭  

对比：

```text
nsh> ledblink          # 占着前台，直到 Ctrl-C / 杀任务
nsh> ledblink &        # 后台才腾出 NSH
```

---

## 6. 代码要点（读 `wqblink_main.c`）

### 6.1 静态 `work_s`

```c
static struct work_s g_work;   /* 调用方提供节点，由 wqueue 管理 */
```

规则：

- 同一 `work_s` **未完成/未 cancel 前不要再次 queue**（周期做法是在 **worker 末尾**再 queue）  
- 停止用 `work_cancel(LPWORK, &g_work)`  

### 6.2 投递与周期

```c
work_queue(LPWORK, &g_work, wqblink_worker, NULL,
           MSEC2TICK(CONFIG_EXAMPLES_WQBLINK_INTERVAL_MS));
```

| 参数 | 含义 |
|------|------|
| `LPWORK` | 低优先级队列 ID |
| `&g_work` | 队列节点 |
| `wqblink_worker` | 到点后在 **worker 线程**执行 |
| `delay` | 系统 tick；`MSEC2TICK(500)` ≈ 500ms |

Worker 末尾再次 `work_queue(...)` → 形成周期。

### 6.3 控灯

与 ledblink 相同：`open("/dev/userleds")` + `ULEDIOC_SETALL`（同相 `0x3`/`0x0`）。  
fd 可在 start 时打开，worker 里 ioctl（flat 构建下可行）。

### 6.4 和 `wd_start` 的关系（了解）

延迟 work 在内核里会走到看门狗/定时逻辑（`wd_start_*` 一类），到期后唤醒 worker。  
应用课优先 **`work_queue(..., delay)`**；只有写驱动/极底层时才直接玩 `wd_start`，且到期回调里通常再 `work_queue(HPWORK/LPWORK, delay=0)`，避免在定时器上下文干重活。

---

## 7. 调用关系（简图）

```text
nsh> wqblink
  → wqblink_main
       open(/dev/userleds)           // 可选
       work_queue(LPWORK, delay)
       return                        // 任务结束
              │
              │  ~500ms 后
              ▼
         LPWORK worker 线程
              wqblink_worker()
                 ioctl SETALL
                 printf
                 work_queue(... delay)  // 再约下一次
```

对比 ledblink 见 `week07-callchain-open-to-gpio.md`（ioctl 以下相同）。

---

## 8. 常见问题

| 现象 | 处理 |
|------|------|
| menuconfig 无 wqblink | 在 `apps/examples` 下再跑 `mkkconfig.sh` |
| `depends on SCHED_LPWORK` 选不了 | 先开 `CONFIG_SCHED_LPWORK` |
| `work_queue failed` | 查 `.config` 是否真有 HP/LP work |
| 只有打印灯不闪 | USERLED 未开或 `/dev/userleds` 失败（会提示 printf-only） |
| `stop` 无效 | 确认烧的是新镜像；命令 `wqblink stop` |

---

## 9. 验收清单

- [x] `.config`：`CONFIG_SCHED_LPWORK=y`，`CONFIG_EXAMPLES_WQBLINK=y`
- [x] `nsh> wqblink` 后立刻可输入其它命令（2026-10-03）
- [x] 周期出现 `worker tick=`（on LPWORK）
- [x] （加分）双灯同相闪（`both=0/1`）
- [ ] `wqblink stop` 停止（请本地敲一下确认）
- [x] 能对照 ledblink：专用任务+usleep vs LPWORK 回调

---

## 10. 相关文档

| 文档 | 内容 |
|------|------|
| `week07-rtos-task.md` | 任务 + usleep |
| `week07-open-dev-userleds.md` | open/ioctl 控灯 |
| `week07-nuttx-build-boot-sched.md` | 调度与联编 |
| `week07-enable-peripherals.md` | 下周 CAN 配置入口 |
| 官方 | [Work Queues](https://nuttx.apache.org/docs/latest/reference/os/wqueue.html) |
