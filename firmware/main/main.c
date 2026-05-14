#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define LOG_INTERVAL_MS 1000

static const char *TAG = "screams";

void app_main(void)
{
    ESP_LOGI(TAG, "Screams sensor booting...");

    while (true) {
        ESP_LOGI(TAG, "alive");
        vTaskDelay(pdMS_TO_TICKS(LOG_INTERVAL_MS));
    }
}
