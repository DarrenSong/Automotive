# GPU：glmark2-es2

官方 Ubuntu 镜像通常带 Mali / 显示栈。无显示器时可直接测；SSH 无头时需要虚拟显示或接 HDMI。

## 安装

```bash
sudo apt install -y glmark2-es2
```

## 有显示器（HDMI）

在板子本地桌面终端或 XRDP 会话里：

```bash
glmark2-es2
# 或较短：
glmark2-es2 --run-forever --annotate
glmark2-es2 --benchmark build --size 800x600
```

结束时会打印 **Score**，越高越好。RK3399 Mali-T860 一般能跑起来；分数受分辨率/驱动影响。

## 无头 SSH（可选）

若只有 SSH、没有图形会话，可能报错找不到 display。可选：

1. 接 HDMI 显示器再测（推荐）  
2. 装 `xrdp`，远程桌面进 LXQt 再跑 glmark2  
3. 部分环境可用 `export DISPLAY=:0`（需已有本地 X 在跑）

```bash
# 若桌面已在 :0 登录：
export DISPLAY=:0
glmark2-es2 --offline   # 部分版本支持；没有该参数就省略
```

## 脚本

```bash
./run_glmark2.sh
```
