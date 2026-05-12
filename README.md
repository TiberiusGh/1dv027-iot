# IoT — room noise monitor

A 1DV027 course project. Measures noise level in a room and visualises it over time in a Grafana dashboard. An ESP32 with a microphone reads audio, computes a per-second loudness value, and ships it to a TIG stack running locally in Docker.

## Stack

The TIG stack — Telegraf (ingest), InfluxDB 3 Core (storage), Grafana (dashboards) — with Mosquitto as the MQTT broker and Caddy reverse-proxying the UIs. The ESP32 publishes to MQTT, Telegraf subscribes and writes to InfluxDB, Grafana queries InfluxDB.

## Key decisions

**RMS is computed on the device, not in the backend.** The microphone is sampled at ~2 kHz and reduced to a single RMS (loudness) value per second before being published. The driving reason is _privacy_: raw audio samples can be reconstructed into audible speech, and a database holding them would effectively be a permanent wiretap. An RMS-per-second is just a loudness curve — not reversible into audio. As a side benefit, this cuts the data rate by ~2000× and means downstream queries never have to decimate raw samples.
