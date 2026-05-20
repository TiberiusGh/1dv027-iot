#include "source_adc.h"

#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"

#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_analog_input.h"
#include "aps/esp_zigbee_aps.h"

static const char *TAG = "src_adc";

// MAX4466 OUT wired to GPIO1 = ADC1_CH0 on ESP32-H2.
#define ADC_UNIT             ADC_UNIT_1
#define ADC_CHANNEL          ADC_CHANNEL_0
#define ADC_ATTEN            ADC_ATTEN_DB_12
#define ADC_BIT_WIDTH        SOC_ADC_DIGI_MAX_BITWIDTH

#define SAMPLE_RATE_HZ       8000
#define WINDOW_MS            100
#define SAMPLES_PER_WINDOW   ((SAMPLE_RATE_HZ * WINDOW_MS) / 1000)

#define FRAME_BYTES          (SAMPLES_PER_WINDOW * SOC_ADC_DIGI_RESULT_BYTES)
#define POOL_BYTES           (FRAME_BYTES * 4)

// Every Nth window, log the current RMS at INFO level so the serial monitor
// confirms the pipeline is alive. ~5 s at 10 Hz publish.
#define LOG_EVERY_N_WINDOWS  50

static atomic_bool s_joined = false;
static uint8_t s_endpoint;

void source_adc_set_joined(bool joined)
{
    atomic_store(&s_joined, joined);
}

static float window_stddev(const uint16_t *samples, size_t n)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < n; i++) sum += samples[i];
    float mean = (float)sum / (float)n;

    double sq = 0.0;
    for (size_t i = 0; i < n; i++) {
        float d = (float)samples[i] - mean;
        sq += (double)(d * d);
    }
    return (float)sqrt(sq / (double)n);
}

static void publish_rms(float rms)
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

static adc_continuous_handle_t setup_adc(void)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_cfg = {
        .max_store_buf_size = POOL_BYTES,
        .conv_frame_size    = FRAME_BYTES,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_cfg, &handle));

    adc_digi_pattern_config_t pattern = {
        .atten     = ADC_ATTEN,
        .channel   = ADC_CHANNEL,
        .unit      = ADC_UNIT,
        .bit_width = ADC_BIT_WIDTH,
    };

    adc_continuous_config_t dig_cfg = {
        .pattern_num    = 1,
        .adc_pattern    = &pattern,
        .sample_freq_hz = SAMPLE_RATE_HZ,
        .conv_mode      = ADC_CONV_SINGLE_UNIT_1,
        .format         = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    return handle;
}

static void adc_task(void *arg)
{
    adc_continuous_handle_t handle = setup_adc();
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    uint8_t  *frame  = malloc(FRAME_BYTES);
    uint16_t *samples = malloc(SAMPLES_PER_WINDOW * sizeof(uint16_t));
    adc_continuous_data_t *parsed =
        malloc(SAMPLES_PER_WINDOW * sizeof(adc_continuous_data_t));
    ESP_ERROR_CHECK((frame && samples && parsed) ? ESP_OK : ESP_ERR_NO_MEM);

    uint32_t windows = 0;

    while (true) {
        uint32_t bytes_read = 0;
        esp_err_t err = adc_continuous_read(handle, frame, FRAME_BYTES,
                                            &bytes_read, portMAX_DELAY);
        if (err != ESP_OK || bytes_read == 0) {
            continue;
        }

        uint32_t n_parsed = 0;
        if (adc_continuous_parse_data(handle, frame, bytes_read,
                                      parsed, &n_parsed) != ESP_OK) {
            continue;
        }

        size_t n_out = 0;
        for (uint32_t i = 0; i < n_parsed && n_out < SAMPLES_PER_WINDOW; i++) {
            if (parsed[i].valid && parsed[i].channel == ADC_CHANNEL) {
                samples[n_out++] = (uint16_t)parsed[i].raw_data;
            }
        }
        if (n_out == 0) {
            continue;
        }

        float rms = window_stddev(samples, n_out);

        if ((windows++ % LOG_EVERY_N_WINDOWS) == 0) {
            ESP_LOGI(TAG, "RMS %.1f  (samples=%u, joined=%d)",
                     rms, (unsigned)n_out, atomic_load(&s_joined));
        }

        if (atomic_load(&s_joined)) {
            publish_rms(rms);
        }
    }
}

void source_adc_start(uint8_t endpoint)
{
    s_endpoint = endpoint;
    ESP_LOGI(TAG, "ADC source starting (endpoint=%u, %d Hz, %d ms windows, %d samples)",
             endpoint, SAMPLE_RATE_HZ, WINDOW_MS, SAMPLES_PER_WINDOW);
    xTaskCreate(adc_task, "src_adc", 4096, NULL, 5, NULL);
}
