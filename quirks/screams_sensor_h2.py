"""ZHA quirk for the screams-sensor-h2 ESP32-H2 device.

Maps the AnalogInput present_value attribute to a generic measurement sensor.
Without this quirk ZHA defaults the AnalogInput to a Temperature/Celsius entity.

The reported value is a normalized loudness RMS scalar (no physical unit), so
no device_class or unit_of_measurement is declared — the number is shown as-is.
"""

from zigpy.quirks.v2 import QuirkBuilder
from zigpy.quirks.v2.homeassistant.sensor import SensorStateClass
from zigpy.zcl.clusters.general import AnalogInput

(
    QuirkBuilder("Tiberius", "screams-sensor-h2")
    .sensor(
        attribute_name="present_value",
        cluster_id=AnalogInput.cluster_id,
        state_class=SensorStateClass.MEASUREMENT,
        translation_key="loudness_rms",
        fallback_name="Loudness RMS",
    )
    .add_to_registry()
)
