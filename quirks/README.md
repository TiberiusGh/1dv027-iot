# ZHA quirks

A ZHA quirk teaches Home Assistant how to interpret a Zigbee device whose
clusters don't map cleanly to a standard HA entity type.

## screams_sensor_h2.py

Without this quirk, ZHA exposes the device's AnalogInput cluster as a Temperature
sensor in °C — the default for an unknown analog reading on a Simple Sensor
device. The quirk relabels the `present_value` attribute as a generic
`Loudness RMS` measurement (no `device_class`, no unit), which is what the
firmware actually reports.

## Install (Home Assistant Container)

1. Copy this directory into the HA config volume:
   ```
   cp screams_sensor_h2.py <ha-config>/quirks/
   ```
2. Add to `<ha-config>/configuration.yaml`:
   ```yaml
   zha:
     custom_quirks_path: /config/quirks/
   ```
3. Restart HA, then delete + re-pair the device in ZHA so the quirk applies on join.
