#!/usr/bin/env python3
"""USB 摄像头 / 视频 Haar 人脸检测。"""
import os
import sys
import time

import cv2

HEADLESS = os.environ.get("HEADLESS", "1") != "0"
CASCADE = cv2.data.haarcascades + "haarcascade_frontalface_default.xml"


def open_source(src):
    if src.isdigit():
        return cv2.VideoCapture(int(src))
    return cv2.VideoCapture(src)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "0"
    out = sys.argv[2] if len(sys.argv) > 2 else "face_out.jpg"
    face = cv2.CascadeClassifier(CASCADE)
    if face.empty():
        raise SystemExit("无法加载 Haar 级联")

    cap = open_source(src)
    if not cap.isOpened():
        raise SystemExit(f"无法打开: {src}（检查摄像头与 /dev/video*）")

    print("Running ... Ctrl+C / q to quit")
    last_save = 0
    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                break
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            faces = face.detectMultiScale(gray, 1.1, 5, minSize=(60, 60))
            for (x, y, w, h) in faces:
                cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
            cv2.putText(
                frame,
                f"faces={len(faces)}",
                (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX,
                1,
                (0, 255, 0),
                2,
            )

            if HEADLESS:
                now = time.time()
                if now - last_save > 1.0:
                    cv2.imwrite(out, frame)
                    last_save = now
                    print(f"saved {out} faces={len(faces)}")
            else:
                cv2.imshow("face", frame)
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break
    except KeyboardInterrupt:
        pass
    finally:
        cap.release()
        cv2.destroyAllWindows()
        print("bye")


if __name__ == "__main__":
    main()
