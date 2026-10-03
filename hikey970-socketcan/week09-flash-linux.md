# HiKey970：从 Android 刷成 Linux（进 P4 前置）

> 你当前是 **安卓**，SocketCAN / `ip` / `apt` 需要 **Linux 用户态**。  
> 刷机会 **清空现有系统**，先确认不需要保留安卓数据。  
> **本地镜像已有：** `F:\Hikey970\`（见下方 §2.0，不必再上网找）。  
> 官方参考：[96Boards HiKey970 · Linux Host Installation](https://www.96boards.org/documentation/consumer/hikey/hikey970/installation/linux-fastboot.md.html)

刷完再做：`week09-hikey970-can.md`（can0 / can-utils）。

---

## 0. 你要准备什么

| 项 | 说明 |
|----|------|
| 主机 | **Linux 宿主机**（推荐你那台 Ubuntu 笔记本 / 虚拟机；WSL2 对 USB/fastboot 常不稳，优先原生 Linux 或带 USB 直通的虚拟机） |
| 线 | USB **Type-C** 数据线（能传数据，别只用充电线） |
| 电源 | 96Boards 规范电源；HiKey970 建议 **12V 以上（8–18V）**，电压偏低时 fastboot 易中途重启失败 |
| 镜像 | Debian/Lebian 官方包 **或** 社区 Ubuntu 镜像（见 §2） |
| 工具 | `fastboot` / `adb`（`android-tools-fastboot`） |

主机上：

```bash
sudo apt update
sudo apt install -y android-tools-fastboot android-tools-adb
fastboot --version
```

---

## 1. 拨码开关（进 fastboot）

板子上有 **4 位拨码**（Auto Power / Recovery / …，丝印以板为准）。

刷机常用（与多数中文教程 / 96Boards 一致）：

| 模式 | 拨码（1→4，ON=拨向 ON） | 用途 |
|------|-------------------------|------|
| **Fastboot** | **ON / OFF / ON / OFF** | 正常刷机（Type-C 连主机后上电） |
| **Recovery 烧录** | **ON / ON / ON / OFF** | fastboot 识别不到时用串口 recovery 脚本 |
| **正常启动** | **ON / OFF / OFF / OFF** | 刷完后改回，上电进系统 |

不同批次丝印可能略有差异，以板子印刷 / 96Boards 文档为准。拿不准时：先试 Fastboot 组合，主机执行 `sudo fastboot devices`。

---

## 2. 选哪个 Linux 镜像

### 2.0 你本机已有（`F:\Hikey970`）— 直接用这个

| 文件 | 说明 | 建议 |
|------|------|------|
| **`hikey970-lebian-9.tar.gz`**（约 1.2GB） | **完整刷机包**：含 `flash-all-binaries.sh`、`flash-minimum-binaries.sh`、`binaries/recovery-flash.sh`、rootfs 等 | **优先用这个** |
| `Hikey 970 Lebuntu.rar` | Ubuntu（Lebuntu）镜像压缩包 | 备选 |
| `Hikey 970 Lebian.rar` | Lebian 另一份打包 | 与上面 tar.gz 同类 |
| `Hikey970image.rar` | 多半是安卓/其它整包 | 刷 Linux 先别用 |
| `Hikey970 Update Image Guide.pdf` 等 | 官方 PDF 说明 | 拨码/刷机细节可对照 |
| `Hikey970 Switch Mode Guide.pdf` | 拨码模式 | 必看 |

**推荐路径：拷 `hikey970-lebian-9.tar.gz` → Ubuntu 主机解压 → 按包内 `lebian-rfs_flashing_guide.txt` 刷。**

```powershell
# Windows：拷到 U 盘或直接 scp 到笔记本
# 例：scp 到 darren 的家目录
scp "F:\Hikey970\hikey970-lebian-9.tar.gz" darren@RedmiBook-Pro-14:~/
```

```bash
# Ubuntu 主机
cd ~
tar -xvzf hikey970-lebian-9.tar.gz
cd hikey970-lebian-9
ls flash-*.sh binaries/recovery-flash.sh
cat lebian-rfs_flashing_guide.txt   # 包内英文说明
```

包内登录信息：

```text
Username: shunya
Password: shunya
```

系统概要（Release Notes）：Lebian-9，内核约 4.9.x，LXDE。刷完首次登录后常需：

```bash
sudo resize2fs /dev/sdd15    # 分区号以实际为准，见包内说明
```

#### 板子已能进过系统 / UEFI 已有时（多数二手板）

拨码 **ON OFF ON OFF**（1=ON, 2=OFF, 3=ON, 4=OFF）→ Type-C + 电源 → 主机：

```bash
sudo fastboot devices
cd ~/hikey970-lebian-9
sudo ./flash-minimum-binaries.sh
# 或 sudo ./flash-all-binaries.sh（以脚本内容/文档为准）
```

#### 新板或 fastboot 一直 `waiting for device`

拨码 **ON ON ON OFF** → 上电后看 `ls /dev/ttyUSB*`，然后：

```bash
cd ~/hikey970-lebian-9
sudo ./binaries/recovery-flash.sh /dev/ttyUSB0
```

刷完：**断电** → 拨码 **ON OFF OFF OFF** → **拔掉 Type-C** → 接好 HDMI 再上电（文档强调先插 HDMI）。

### 方案 A / B（网上现下）— 仅当本地包损坏时

96Boards 官方页或社区 `hikey970-ubuntu-image` Release；步骤同前版文档。你已有 `F:\Hikey970` 时**不必再找**。

---

## 3. 刷机步骤（Fastboot 主路径）

```text
1) 主机装好 fastboot
2) 970 断电（拔电源 + 拔 Type-C）
3) 拨码 → Fastboot：ON OFF ON OFF
4) Type-C 连主机，再接板载电源上电
5) 主机：
```

```bash
sudo fastboot devices
# 应出现一串设备 ID，例如：
# ABCDEFG    fastboot
```

若 **没有设备**：

- 换线 / 换口；虚拟机检查 USB 是否勾选给该 VM  
- 试 Recovery 模式（§4）  
- 确认已上电且拨码正确  

有设备后：

```bash
cd <镜像目录>
sudo ./flash-all-binaries.sh
# 或按该镜像文档逐条 fastboot flash ...
```

刷完：

```text
6) 断电
7) 拨码改回正常启动：ON OFF OFF OFF
8) 接 HDMI 键盘（可选）或串口，上电
9) 登录 Linux，执行 uname -a
```

---

## 4. Fastboot 识别不到时（Recovery）

```text
1) 断电
2) 拨码 → ON ON ON OFF
3) 上电，主机看串口设备：
```

```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

```bash
cd <镜像目录>
sudo ./binaries/recovery-flash.sh /dev/ttyUSB0
# 把 ttyUSB0 换成你机器上实际名字
```

完成后同样改回 **ON OFF OFF OFF** 再启动。

---

## 5. 刷完后最小确认

```bash
uname -a
cat /etc/os-release
ip link
ping -c 2 8.8.8.8   # 或先连 WiFi/网线
```

然后进入 SocketCAN：

→ **`week09-hikey970-can.md`**

---

## 6. 注意

1. **电压**：尽量用足够功率的 12V+ 电源，避免 flash 中途复位。  
2. **虚拟机 USB**：整段刷机过程保持设备直通，勿休眠。  
3. **与 Firefly RK3399 不是同一套刷法**——RK3399 用 Firefly 工具/镜像；本文仅针对 **HiKey970**。  
4. 刷 Linux 后安卓分区会被覆盖；需要安卓请另备镜像再刷回。  
5. 社区镜像较旧（16.04/18.04 常见），够跑 `can-utils` / SocketCAN 学习即可；内核若无 can 驱动，再查设备树/模块（见 week09 §2.1）。

---

## 7. 本周你先做的事

1. 在 Ubuntu 主机装 `fastboot`  
2. 下载一套 HiKey970 **Linux** 镜像包  
3. 拨码进 fastboot → `sudo fastboot devices` 有输出  
4. 执行官方/包内刷机脚本  
5. 正常启动拨码 → 登录 → 把 `uname -a` 贴回来  

卡住时贴：`fastboot devices` 输出、拨码位置、用的镜像名称/链接。
