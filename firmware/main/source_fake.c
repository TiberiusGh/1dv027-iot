#include "source_fake.h"

#include <math.h>
#include <stdatomic.h>

#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_analog_input.h"
#include "aps/esp_zigbee_aps.h"

static const char *TAG = "src_fake";

#define PUBLISH_PERIOD_MS    100      // 10 Hz publish rate.
#define FAKE_BASE            0.5f
#define FAKE_AMPLITUDE       0.3f
#define FAKE_PERIOD_TICKS    50       // sine period = 50 * 100 ms = 5 s.
#define FAKE_SPIKE_PROB_PCT  5

static atomic_bool s_joined = false;
static uint8_t s_endpoint;

void source_fake_set_joined(bool joined)
{
    atomic_store(&s_joined, joined);
}

static float next_sample(uint32_t t)
{
    float rms = FAKE_BASE + FAKE_AMPLITUDE * sinf((float)t * (2.0f * (float)M_PI / FAKE_PERIOD_TICKS));
    if (esp_random() % 100 < FAKE_SPIKE_PROB_PCT) {
        rms += 0.5f + (esp_random() / (float)UINT32_MAX);
    }
    return rms;
}

static void publish_one(float rms)
{
    if (!esp_zb_lock_acquire(portMAX_DELAY)) {
        return;
    }

    esp_zb_zcl_set_attribute_val(s_endpoint,
                                 ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
                                 ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                 ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID,
                                 &rms, false);

    esp_zb_zcl_report_attr_cmd_t cmd = {
        .zcl_basic_cmd  = { .src_endpoint = s_endpoint },
        .address_mode   = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
        .clusterID      = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
        .attributeID    = ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID,
        .direction      = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
    };
    esp_zb_zcl_report_attr_cmd_req(&cmd);

    esp_zb_lock_release();
}

static void fake_task(void *arg)
{
    uint32_t t = 0;
    while (true) {
        if (atomic_load(&s_joined)) {
            float rms = next_sample(t);
            publish_one(rms);
            t++;
        }
        vTaskDelay(pdMS_TO_TICKS(PUBLISH_PERIOD_MS));
    }
}

void source_fake_start(uint8_t endpoint)
{
    s_endpoint = endpoint;
    ESP_LOGI(TAG, "Fake source starting (endpoint=%u, %d ms period)", endpoint, PUBLISH_PERIOD_MS);
    xTaskCreate(fake_task, "src_fake", 4096, NULL, 5, NULL);
}
