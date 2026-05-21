# IoT — room noise monitor

A 1DV027 course project. Measures noise level in a room and visualises it over time in a Grafana dashboard. An ESP32 with a microphone reads audio, computes a per-second loudness value, and ships it to a TIG stack running locally in Docker.

## Stack

The TIG stack — Telegraf (ingest), InfluxDB 3 Core (storage), Grafana (dashboards) — with Mosquitto as the MQTT broker and Caddy reverse-proxying the UIs. The ESP32 publishes to MQTT, Telegraf subscribes and writes to InfluxDB, Grafana queries InfluxDB.

## Key decisions

**RMS is computed on the device, not in the backend.** The microphone is sampled at ~2 kHz and reduced to a single RMS (loudness) value per second before being published. The driving reason is _privacy_: raw audio samples can be reconstructed into audible speech, and a database holding them would effectively be a permanent wiretap. An RMS-per-second is just a loudness curve — not reversible into audio. As a side benefit, this cuts the data rate by ~2000× and means downstream xwqueries never have to decimate raw samples.

**dB values in the dashboard are relative, not calibrated.** Grafana converts the raw RMS scalar to a decibel scale with `20 * log10(rms / noise_floor)`, where `noise_floor` is an observed value picked during a quiet moment — not measured against a reference SPL meter (which would require a calibrated source like a 94 dB piston phone). The numbers are useful for relative comparison ("scream is N dB above background") and should not be read as real-world dB SPL.

**Buzzer credentials never ship in the public bundle.** They live in `.env` and are served by Caddy at `/whoami`, which is gated by Cloudflare Access — only an authenticated owner gets them. The MQTT ACL also restricts the `buzzer` user to write-only on `screams/cmd/#` as defense-in-depth.

**Mixed auth: public dashboard, gated controls.** Cloudflare Access gates three paths under the public hostname; the rest is open:

| Path              | Who        | What                                                  |
| ----------------- | ---------- | ----------------------------------------------------- |
| `/`, `/grafana/*` | everyone   | static page + read-only embedded charts               |
| `/signin`         | owner only | redirect endpoint — triggers login flow, lands at `/` |
| `/whoami`         | owner only | returns MQTT credentials as JSON to the frontend JS   |
| `/mqtt*`          | owner only | MQTT-over-WebSocket endpoint for the Buzz button      |

The `CF_Authorization` cookie is set by Cloudflare (HttpOnly, Secure, SameSite); the app never reads or writes it.

## Setup

Uses **InfluxDB 3 Core**, which requires an admin token. After `docker compose up -d`:

```sh
docker compose exec influx influxdb3 create token --admin
docker compose exec influx influxdb3 create database iot --token "$TOKEN"
```

Save the token from the first command as `TOKEN=…` in `.env`.
