# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "httpx",
#   "python-dotenv",
# ]
# ///
"""Fake audio stream → InfluxDB. Run: uv run fake_stream.py"""
import math
import os
import random
import time

import httpx
from dotenv import load_dotenv

URL = "http://influx.localhost:8234/api/v3/write_lp"
DB = "iot"
THRESHOLD = 0.7

load_dotenv()
token = os.environ["TOKEN"]

with httpx.Client(headers={"Authorization": f"Bearer {token}"}) as client:
    t = 0
    while True:
        rms = 0.5 + 0.3 * math.sin(t / 5)
        if random.random() < 0.05:
            rms += random.uniform(0.5, 1.5)
        above = "true" if rms > THRESHOLD else "false"
        line = f"audio,source=fake rms={rms:.4f},above_threshold={above}"
        r = client.post(URL, params={"db": DB}, content=line)
        r.raise_for_status()
        print(f"t={t:4d} rms={rms:.3f} above={above}")
        t += 1
        time.sleep(1)
