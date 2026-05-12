# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "paho-mqtt",
#   "python-dotenv",
# ]
# ///
"""Fake audio stream → MQTT. Run: uv run fake_stream.py"""
import math
import os
import random
import time

import paho.mqtt.client as mqtt
from dotenv import load_dotenv

THRESHOLD = 0.7

load_dotenv()
host = os.environ["MQTT_HOST"]
port = int(os.environ["MQTT_PORT"])
user = os.environ["MQTT_USER"]
password = os.environ["MQTT_PASS"]
topic = os.environ["MQTT_TOPIC"]

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="fake_stream")
client.username_pw_set(user, password)
client.connect(host, port, keepalive=60)
client.loop_start()

try:
    t = 0
    while True:
        rms = 0.5 + 0.3 * math.sin(t / 5)
        if random.random() < 0.05:
            rms += random.uniform(0.5, 1.5)
        above = "true" if rms > THRESHOLD else "false"
        line = f"audio,source=fake rms={rms:.4f},above_threshold={above}"
        client.publish(topic, line, qos=0)
        print(f"t={t:4d} rms={rms:.3f} above={above}")
        t += 1
        time.sleep(1)
finally:
    client.loop_stop()
    client.disconnect()
