# Keil：No Algorithm found（bootloader / app_boot 通用）

编译 `0 Error` 只说明链接成功。  
`No Algorithm found for: 0800xxxxH` + `Erase skipped!` = **Flash Programming Algorithm 列表为空，或 Size/路径不对**。

改 IROM / scatter / 输出名后，Keil **经常把算法弄丢**——所以你会觉得「每次重编都难下」。

## 两个工程都要设成一样（烧录算法 ≠ IROM）

| 项 | 值 |
|----|-----|
| Algorithm | `STM32F10x_512kB Flash` |
| **Start** | `0x08000000` |
| **Size** | `0x00080000`（整片 512KB） |

| 工程 | IROM（链接，别和算法搞混） |
|------|---------------------------|
| bootloader | Start `0x08000000` Size `0x8000` |
| app_boot | Start `0x08008000` Size `0x78000` |

## 操作（关掉工程再开一次更稳）

1. 关闭 Keil → 再打开 `bootDemo.uvprojx`
2. **Options for Target** → **Debug**
3. 选中 **ST-Link Debugger** → **Settings**
4. **Flash Download** 页：
   - 若 Programming Algorithm **是空的**：点 **Add** → 选 `STM32F10x_512kB Flash`
   - 选中该行：**Start=`0x08000000`，Size=`0x00080000`**
   - 勾选 Erase Sectors / Program / Verify（可选 Reset and Run）
5. **Utilities** 页勾选 **Use Debug Driver**
6. OK → Download

`app_boot` 用同样步骤再设一遍（算法仍是整片，不要改成 `0x78000`）。

## 已在工程文件里加固

- FLM 改为绝对路径：  
  `C:\Keil_v5\ARM\Pack\Keil\STM32F1xx_DFP\1.0.5\Flash\STM32F10x_512.FLM`
- 涉及：`bootDemo\USER\LED.uvoptx`、`bootDemo.uvprojx`（及 app_boot 同类项）

若你本机 Keil 不在 `C:\Keil_v5`，在 Flash Download 里 **Add** 一次即可自动改路径。

## 仍失败时

1. 看 Flash Download 列表是否真有一行算法（空 = 必失败）
2. ST-Link：Connect Under Reset / Hardware Reset（和以前一样）
3. 备用：STM32CubeProgrammer 直接下 `OBJ\bootloader.hex` 到 `0x08000000`
