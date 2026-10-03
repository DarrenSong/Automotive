# Bootloader / app_boot 跳转详解（32KB 分区，已按此配置改代码）

> 命名约定（固定）：**bootloader** + **app_boot**  
> 板型：正点原子战舰 STM32F103ZET6  
> bootloader 源码：`bootDemo\USER\main.c`（参考副本 `bootloader\boot_main.c`）  
> app_boot：`app_boot\USER\main.c`  

---

## 0. 命名与分区（当前）

| 名称 | 工程路径 | Flash | 大小 |
|------|----------|-------|------|
| **bootloader** | `bootDemo\`（输出名 `bootloader`） | `0x08000000` | **32KB** (`0x8000`) |
| **app_boot** | `app_boot\`（输出名 `app_boot`） | `0x08008000` | `0x78000`（512KB−32KB） |

```text
0x08000000  ┌─────────────────┐
            │  bootloader     │  32KB = 0x8000
0x08008000  ├─────────────────┤
            │  app_boot       │  Size = 0x78000
0x08080000  └─────────────────┘
```

Keil 配置：

| 工程 | IROM Start | IROM Size | Scatter | OutputName |
|------|------------|-----------|---------|------------|
| bootloader | `0x08000000` | `0x8000` | `bootloader.sct` | `bootloader` |
| app_boot | `0x08008000` | `0x78000` | `app_boot_app.sct` | `app_boot` |

Flash Download Algorithm **两边都仍用整片**：`0x08000000` / `0x80000`。  
若报 `No Algorithm found for: 08008000H`，见 `flash-algorithm-fix.md`（算法 Size 被误设为 `0x8000`）。

---

## 1. 从 16KB 改到 32KB 要改什么

| 位置 | 旧（16KB） | 新（32KB） |
|------|------------|------------|
| bootloader IROM Size | `0x4000` | `0x8000` |
| `APP_ADDR` | `0x08004000` | `0x08008000` |
| app_boot IROM Start | `0x08004000` | `0x08008000` |
| app_boot IROM Size | `0x7C000` | `0x78000` |
| `SCB->VTOR` | `0x08004000` | `0x08008000` |

改完后必须：**先重新下载 app_boot，再下载 bootloader**（旧 APP 在 `0x08004000` 已失效）。

---

## 2. bootloader 代码逐段讲解

对应：`bootDemo\USER\main.c` / `bootloader\boot_main.c`。

### 2.1 宏

```c
#define APP_ADDR          0x08008000u   /* app_boot 向量表基址 */
#define APP_STACK_MASK    0x2FFE0000u
#define APP_STACK_EXPECT  0x20000000u
typedef void (*pFunction)(void);
```

### 2.2 `App_IsValid`

读 `APP_ADDR+0` 栈顶、`+4` 复位向量；检查 SRAM 特征、Flash 高字节 `0x08`、Thumb 位为 1。

### 2.3 `Jump_To_App`

校验 → 打印 `Jump to APP...` → `delay_ms(80)` → `__disable_irq` → `USART_DeInit` → `__set_MSP(sp)` → `jump()`。

### 2.4 `main` 策略

约 2s 窗口采样 KEY0（低有效）/ WK_UP（高有效）：

1. 按过键 → `Force BOOTLOADER` → 循环 `in BOOTLOADER`  
2. 合法 app_boot → 跳转  
3. 否则 `No valid APP` + dump `APP[0]`/`APP[1]`  

---

## 3. app_boot 必改项

```c
u32 cnt = 0;                 /* C90：声明在前 */
SCB->VTOR = 0x08008000u;     /* 向量表 = app_boot 基址 */
__enable_irq();              /* bootloader 跳转前关过中断 */
```

循环里 `printf("enter app, cnt=...")` 用于验收。

---

## 4. 下载与验收

1. Rebuild **app_boot** → Download（确认 map：`RESET 0x08008000`）  
2. Rebuild **bootloader** → Download（Erase Sectors，勿 Full Chip 误擦 APP）  
3. 不按键复位 → `=== BOOTLOADER ... ===` → `Jump to APP...` → `enter app`  
4. 按住 KEY0 复位 → `Force BOOTLOADER` → `in BOOTLOADER`  

---

## 5. 踩坑（与分区相关）

| 现象 | 原因 |
|------|------|
| `No valid APP`，全 `FF` | 未烧 app_boot，或仍烧在旧地址 `0x08004000` |
| 乱值 SP/RV | app_boot 链接地址未改到 `0x08008000` |
| `No Algorithm for 08008000H` | 烧录算法 Size 太小，应整片 `0x80000` |
| bootloader 链接超 32KB | 代码过大；精简或再加大分区 |

---

## 6. 下一步

串口 IAP：擦写从 **`0x08008000`** 起，不要再写 `0x08004000`。

---

*32KB 分区验收通过（2026-10）：自动跳 app_boot + KEY0 Force BOOTLOADER。*  
*串口 IAP 已验收：见同目录 `iap-demo-readme.md`。*
