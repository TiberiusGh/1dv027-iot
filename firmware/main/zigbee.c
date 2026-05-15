#include "zigbee.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_zigbee_core.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "zcl/esp_zigbee_zcl_analog_input.h"

#include "source_fake.h"

static const char *TAG = "zigbee";

#define ED_AGING_TIMEOUT    ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE_MS    3000

// Coordinator (ZBT-2 via ZHA) is configured on channel 11. Narrow mask = fast rejoin.
#define PRIMARY_CHANNEL_MASK (1u << 11)

// 802.15.4 scan duration encoding: time per channel = ~15.36 ms * (2^n + 1).
// n=5 -> ~500 ms/channel; with a one-channel mask, ~500 ms total per attempt.
#define SCAN_DURATION       5

// ESP32-H2 max TX power. Battery runtime is dwarfed by sample/idle cost, not radio.
#define TX_POWER_DBM        13

// Zigbee ZCL character strings are length-prefixed (first byte = char count, no null).
#define MANUFACTURER_NAME   "\x08""Tiberius"
#define MODEL_IDENTIFIER    "\x11""screams-sensor-h2"
#define AI_DESCRIPTION      "\x0c""Loudness RMS"

static void retry_steering(uint8_t mode)
{
    esp_zb_bdb_start_top_level_commissioning(mode);
}

static esp_zb_cluster_list_t *build_clusters(void)
{
    esp_zb_cluster_list_t *clusters = esp_zb_zcl_cluster_list_create();

    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version  = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_BATTERY,
    };
    esp_zb_attribute_list_t *basic = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER);
    esp_zb_cluster_list_add_basic_cluster(clusters, basic, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_identify_cluster_cfg_t identify_cfg = { .identify_time = 0 };
    esp_zb_attribute_list_t *identify = esp_zb_identify_cluster_create(&identify_cfg);
    esp_zb_cluster_list_add_identify_cluster(clusters, identify, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_analog_input_cluster_cfg_t ai_cfg = {
        .out_of_service = false,
        .present_value  = 0.0f,
        .status_flags   = 0,
    };
    esp_zb_attribute_list_t *ai = esp_zb_analog_input_cluster_create(&ai_cfg);
    esp_zb_analog_input_cluster_add_attr(ai, ESP_ZB_ZCL_ATTR_ANALOG_INPUT_DESCRIPTION_ID, AI_DESCRIPTION);
    esp_zb_cluster_list_add_analog_input_cluster(clusters, ai, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    return clusters;
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*p_sg_p;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "First start: not commissioned, starting network steering...");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGW(TAG, "First start failed (%s)", esp_err_to_name(err));
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Joined Zigbee network (pan=0x%04x ch=%d short=0x%04x)",
                     esp_zb_get_pan_id(),
                     esp_zb_get_current_channel(),
                     esp_zb_get_short_address());
            source_fake_set_joined(true);
        } else {
            ESP_LOGW(TAG, "Steering failed (%s) -> retry in 1s", esp_err_to_name(err));
            esp_zb_scheduler_alarm(retry_steering, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Rejoined Zigbee network (pan=0x%04x ch=%d short=0x%04x)",
                     esp_zb_get_pan_id(),
                     esp_zb_get_current_channel(),
                     esp_zb_get_short_address());
            source_fake_set_joined(true);
        } else {
            ESP_LOGW(TAG, "Rejoin failed (%s) -> start steering", esp_err_to_name(err));
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        }
        break;

    default:
        ESP_LOGI(TAG, "ZDO signal %s (0x%x) status %s",
                 esp_zb_zdo_signal_to_string(sig_type),
                 (unsigned int)sig_type,
                 esp_err_to_name(err));
        break;
    }
}

static void zigbee_task(void *arg)
{
    esp_zb_cfg_t cfg = {
        .esp_zb_role         = ESP_ZB_DEVICE_TYPE_ED,
        .install_code_policy = false,
        .nwk_cfg.zed_cfg = {
            .ed_timeout = ED_AGING_TIMEOUT,
            .keep_alive = ED_KEEP_ALIVE_MS,
        },
    };
    esp_zb_init(&cfg);
    esp_zb_nvram_erase_at_start(false);

    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t ep_cfg = {
        .endpoint           = ZIGBEE_ENDPOINT_ID,
        .app_profile_id     = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id      = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID,
        .app_device_version = 0,
    };
    esp_zb_ep_list_add_ep(ep_list, build_clusters(), ep_cfg);
    esp_zb_device_register(ep_list);

    esp_zb_set_primary_network_channel_set(PRIMARY_CHANNEL_MASK);
    esp_zb_set_tx_power(TX_POWER_DBM);
    esp_zb_bdb_set_scan_duration(SCAN_DURATION);

    uint8_t ieee[8];
    esp_read_mac(ieee, ESP_MAC_IEEE802154);
    ESP_LOGI(TAG, "Local IEEE address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
             ieee[0], ieee[1], ieee[2], ieee[3], ieee[4], ieee[5], ieee[6], ieee[7]);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

void zigbee_start(void)
{
    xTaskCreate(zigbee_task, "zigbee_main", 4096, NULL, 5, NULL);
}
