#!/usr/bin/env python3
"""订阅 firefly/# 并打印。"""
import paho.mqtt.client as mqtt

HOST = "localhost"
TOPIC = "firefly/#"


def on_connect(client, userdata, flags, rc):
    print("connected:", rc)
    client.subscribe(TOPIC)
    print("subscribed:", TOPIC)


def on_message(client, userdata, msg):
    print(f"[{msg.topic}] {msg.payload.decode(errors='replace')}")


def main():
    client = mqtt.Client(client_id="firefly-sub")
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(HOST, 1883, 60)
    print("Listening ... Ctrl+C to stop")
    client.loop_forever()


if __name__ == "__main__":
    main()
