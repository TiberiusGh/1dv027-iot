#include "buzzer.h"

#include <stdatomic.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "buzzer";

#define BUZZER_GPIO     GPIO_NUM_10
#define BUZZER_PULSE_MS 200

static esp_timer_handle_t s_off_timer;
static atomic_bool s_pulsing = false;

static void off_cb(void *arg)
{
    gpio_set_level(BUZZER_GPIO, 0);
    atomic_store(&s_pulsing, false);
}

void buzzer_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BUZZER_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(BUZZER_GPIO, 0);

    esp_timer_create_args_t args = {
        .callback = off_cb,
        .name     = "buzzer_off",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_off_timer));

    ESP_LOGI(TAG, "Buzzer ready on GPIO%d (%d ms pulse)", BUZZER_GPIO, BUZZER_PULSE_MS);
}

void buzzer_pulse(void)
{
    bool expected = false;
    if (!atomic_compare_exchange_strong(&s_pulsing, &expected, true)) {
        return;
    }
    gpio_set_level(BUZZER_GPIO, 1);
    esp_timer_start_once(s_off_timer, BUZZER_PULSE_MS * 1000);
}
