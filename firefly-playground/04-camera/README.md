# USB 摄像头：Haar 人脸 / 轻量检测

## 安装

```bash
sudo apt install -y python3-opencv python3-numpy v4l-utils
v4l2-ctl --list-devices
ls /dev/video*
```

无显示器时脚本默认保存结果图到当前目录，也可开窗口（有桌面/XRDP 时）。

## Haar 人脸（零模型下载，立刻可跑）

```bash
python3 face_haar.py              # 摄像头 /dev/video0
python3 face_haar.py 0 out.jpg    # 保存截图
python3 face_haar.py video.mp4    # 本地视频
```

## 轻量物体检测（MobileNet-SSD，Caffe，OpenCV DNN）

首次运行会提示下载模型（约几 MB～几十 MB），或手动放到 `models/`：

```bash
mkdir -p models
# 见 download_ssd_models.sh
./download_ssd_models.sh
python3 detect_ssd.py
```

YOLO 全尺寸偏重；这块板更推荐 **Haar** 或 **MobileNet-SSD**。若要 YOLO，可用 ultra-light 的 yolov5n ONNX（需自行转模型，见文末）。

## 无头注意

SSH 下 OpenCV 弹窗可能失败，脚本已默认 `HEADLESS=1` 写文件；有桌面时：

```bash
HEADLESS=0 DISPLAY=:0 python3 face_haar.py
```
