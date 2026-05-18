# Firmware — screams-sensor-h2

ESP32-H2 Zigbee end device. Publishes a loudness scalar 10× per second to a ZBT-2
coordinator running ZHA in Home Assistant, and also accepts a buzz command back
the other way (HA → ZHA → device → GPIO → piezo beep).

## Build & flash

```sh
idf.py set-target esp32h2
idf.py build
idf.py flash monitor
```

A full erase is recommended after firmware changes that alter the Zigbee
attribute set, so the device re-pairs cleanly:

```sh
idf.py erase-flash flash monitor
```

## Layout

- [main/main.c](main/main.c) — wires the modules together
- [main/zigbee.c](main/zigbee.c) — radio, clusters, BDB join/rejoin/retry, On/Off attribute callback
- [main/source_fake.c](main/source_fake.c) — placeholder sample source (sine + spikes) until the real ADC/mic source lands
- [main/buzzer.c](main/buzzer.c) — GPIO10 active-piezo driver, one-shot 200 ms pulse

## Key choices

**Zigbee end device, not router.** Battery-class role. Radio sleeps between
parent polls when idle. We don't relay traffic for other devices.

**AnalogInput cluster (0x000C), `present_value`.** Generic float channel.
Paired with a ZHA quirk ([../quirks/](../quirks/)) so Home Assistant exposes it
as `Loudness RMS` instead of the default Temperature/°C.

**10 Hz publish rate.** Each tick the firmware writes `present_value` locally
and sends a `Report Attributes` command. The rate is the resolution / Zigbee
airtime tradeoff. Speech is not recoverable from amplitude envelopes at any
rate (no phase, no frequency content) — the privacy property comes from never
transmitting raw samples, not from the rate.

**Channel mask narrowed to 11.** ZHA on the ZBT-2 is on channel 11; scanning
just that channel makes rejoin ~16× faster than scanning the whole 2.4 GHz band.

**Plugged in via USB, not battery.** Continuous ADC sampling + always-awake
radio averages 20–40 mA. A 1-year run on batteries would require a hardware
wake-on-sound front-end; with USB we sidestep that. A baby monitor whose
battery dies at 3 a.m. is a worse product than one with a cable.

**NVS retained across reboots** (`esp_zb_nvram_erase_at_start(false)`). The
device rejoins its prior network on power cycle without needing to re-pair.

**On/Off cluster (0x0006) + GPIO10 active piezo.** Receives a "buzz" command
from HA. On any `On` write, firmware drives GPIO10 HIGH for 200 ms — that's the
whole device-side contract. HA owns the on→off cycle in its automation (turn_on,
short delay, turn_off), so the device never has to fight HA's optimistic state
model.

## ZHA pairing

ZHA "Add device" must be open while the H2 boots. The signal handler retries
network steering every ~3 s on failure, so leave the pairing window open and
the H2 will join within a couple of attempts.
