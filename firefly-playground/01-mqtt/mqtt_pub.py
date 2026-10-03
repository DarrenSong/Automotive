#!/usr/bin/env python3
"""向本机 Mosquitto 周期性发布模拟传感器数据。"""
import json
import random
import time

import paho.mqtt.client as mqtt

HOST = "localhost"
TOPIC = "firefly/sensors/demo"


def main():
    client = mqtt.Client(client_id="firefly-pub")
    client.connect(HOST, 1883, 60)
    client.loop_start()
    print(f"Publishing to {TOPIC} ... Ctrl+C to stop")
    try:
        while True:
            payload = {
                "temp_c": round(20 + random.random() * 10, 2),
                "humidity": round(40 + random.random() * 30, 1),
                "ts": time.time(),
            }
            client.publish(TOPIC, json.dumps(payload), qos=0)
            print("->", payload)
            time.sleep(2)
    except KeyboardInterrupt:
        print("\nbye")
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
