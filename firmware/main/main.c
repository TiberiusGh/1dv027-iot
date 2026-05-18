#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_zigbee_core.h"

#include "buzzer.h"
#include "source_fake.h"
#include "zigbee.h"

static const char *TAG = "screams";

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    esp_zb_platform_config_t platform_cfg = {
        .radio_config = { .radio_mode = ZB_RADIO_MODE_NATIVE },
        .host_config  = { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE },
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&platform_cfg));

    ESP_LOGI(TAG, "Screams sensor booting (Zigbee ED)...");
    buzzer_init();
    zigbee_start();
    source_fake_start(ZIGBEE_ENDPOINT_ID);
}
