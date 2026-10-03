#!/usr/bin/env python3
"""MobileNet-SSD (Caffe) 物体检测，OpenCV DNN，适合 RK3399 轻量试用。"""
import os
import sys
import time
import urllib.request

import cv2
import numpy as np

HEADLESS = os.environ.get("HEADLESS", "1") != "0"
HERE = os.path.dirname(os.path.abspath(__file__))
MODEL_DIR = os.path.join(HERE, "models")
PROTOTXT = os.path.join(MODEL_DIR, "MobileNetSSD_deploy.prototxt")
CAFFEMODEL = os.path.join(MODEL_DIR, "MobileNetSSD_deploy.caffemodel")

CLASSES = [
    "background", "aeroplane", "bicycle", "bird", "boat", "bottle", "bus",
    "car", "cat", "chair", "cow", "diningtable", "dog", "horse", "motorbike",
    "person", "pottedplant", "sheep", "sofa", "train", "tvmonitor",
]

URLS = {
    PROTOTXT: "https://raw.githubusercontent.com/chuanqi305/MobileNet-SSD/master/deploy.prototxt",
    # 备用镜像若失效，请手动下载 MobileNetSSD_deploy.caffemodel 放到 models/
    CAFFEMODEL: "https://github.com/chuanqi305/MobileNet-SSD/raw/master/mobilenet_iter_73000.caffemodel",
}


def ensure_models():
    os.makedirs(MODEL_DIR, exist_ok=True)
    for path, url in URLS.items():
        if os.path.isfile(path) and os.path.getsize(path) > 1000:
            continue
        print("Downloading", os.path.basename(path), "...")
        try:
            urllib.request.urlretrieve(url, path)
        except Exception as e:
            raise SystemExit(
                f"下载失败: {e}\n请手动把模型放到 {MODEL_DIR}\n"
                "或运行 ./download_ssd_models.sh"
            )


def main():
    ensure_models()
    net = cv2.dnn.readNetFromCaffe(PROTOTXT, CAFFEMODEL)
    src = sys.argv[1] if len(sys.argv) > 1 else "0"
    out = sys.argv[2] if len(sys.argv) > 2 else "ssd_out.jpg"
    cap = cv2.VideoCapture(int(src) if src.isdigit() else src)
    if not cap.isOpened():
        raise SystemExit(f"无法打开: {src}")

    print("SSD running ... Ctrl+C / q")
    last_save = 0
    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                break
            h, w = frame.shape[:2]
            blob = cv2.dnn.blobFromImage(
                cv2.resize(frame, (300, 300)), 0.007843, (300, 300), 127.5
            )
            net.setInput(blob)
            detections = net.forward()
            count = 0
            for i in range(detections.shape[2]):
                conf = float(detections[0, 0, i, 2])
                if conf < 0.4:
                    continue
                idx = int(detections[0, 0, i, 1])
                box = detections[0, 0, i, 3:7] * np.array([w, h, w, h])
                x1, y1, x2, y2 = box.astype(int)
                label = f"{CLASSES[idx]}:{conf:.2f}"
                cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
                cv2.putText(
                    frame, label, (x1, max(20, y1 - 8)),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1,
                )
                count += 1

            if HEADLESS:
                now = time.time()
                if now - last_save > 1.0:
                    cv2.imwrite(out, frame)
                    last_save = now
                    print(f"saved {out} objects={count}")
            else:
                cv2.imshow("ssd", frame)
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break
    except KeyboardInterrupt:
        pass
    finally:
        cap.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
