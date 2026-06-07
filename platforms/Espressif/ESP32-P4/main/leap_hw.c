#include "leap_hw.h"
#include "board_config.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "leap/leap_protocol.h"

static const char *TAG = "leap_hw";

static const int s_out_gpios[LEAP_DO_COUNT] = {
    LEAP_P4_GPIO_OUT0, LEAP_P4_GPIO_OUT1, LEAP_P4_GPIO_OUT2, LEAP_P4_GPIO_OUT3,
    LEAP_P4_GPIO_OUT4, LEAP_P4_GPIO_OUT5, LEAP_P4_GPIO_OUT6, LEAP_P4_GPIO_OUT7,
};

static const int s_in_gpios[LEAP_DI_COUNT] = {
    LEAP_P4_GPIO_IN0, LEAP_P4_GPIO_IN1, LEAP_P4_GPIO_IN2, LEAP_P4_GPIO_IN3,
    LEAP_P4_GPIO_IN4, LEAP_P4_GPIO_IN5, LEAP_P4_GPIO_IN6, LEAP_P4_GPIO_IN7,
};

static void gpio_outputs_init(void)
{
    gpio_config_t out_cfg = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    for (unsigned i = 0; i < LEAP_DO_COUNT; ++i) {
        out_cfg.pin_bit_mask |= (1ULL << s_out_gpios[i]);
    }
    gpio_config(&out_cfg);
}

static void gpio_inputs_init(void)
{
    gpio_config_t in_cfg = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    for (unsigned i = 0; i < LEAP_DI_COUNT; ++i) {
        in_cfg.pin_bit_mask |= (1ULL << s_in_gpios[i]);
    }
    gpio_config(&in_cfg);
}

static void locate_led_init(void)
{
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << LOCATE_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&led_cfg);
    leap_hw_set_locate_led(0);
    ESP_LOGI(TAG, "locate LED on GPIO%d (OUT_LE header)", LOCATE_LED_PIN);
}

static void write_outputs(uint16_t outputs)
{
    for (unsigned i = 0; i < LEAP_DO_COUNT; ++i) {
        gpio_set_level(s_out_gpios[i], (outputs >> i) & 1u);
    }
}

void leap_hw_init(void)
{
    gpio_outputs_init();
    gpio_inputs_init();
    locate_led_init();
    write_outputs(0u);

    ESP_LOGI(TAG, "%u digital outputs + %u digital inputs ready (40-pin header)",
             (unsigned)LEAP_DO_COUNT, (unsigned)LEAP_DI_COUNT);
}

void leap_hw_refresh_inputs(LeapP4IoShadow *io)
{
    uint16_t inputs = 0u;

    if (io == NULL) {
        return;
    }

    for (unsigned i = 0; i < LEAP_DI_COUNT; ++i) {
        if (gpio_get_level(s_in_gpios[i]) != 0) {
            inputs |= (uint16_t)(1u << i);
        }
    }

    io->digital_inputs = inputs;
}

void leap_hw_apply_outputs(LeapP4IoShadow *io, uint16_t outputs)
{
    uint16_t prev;

    if (io == NULL) {
        return;
    }

    prev = io->digital_outputs;
    io->safe_active = 0;
    io->digital_outputs = outputs;
    leap_hw_refresh_inputs(io);
    io->io_status = LEAP_DIO_STATUS_OK;

    write_outputs(outputs);

    if (prev != outputs) {
        ESP_LOGI(TAG, "GPIO outputs=0x%04X inputs=0x%04X", outputs, io->digital_inputs);
    }
}

void leap_hw_enter_safe(LeapP4IoShadow *io)
{
    if (io == NULL) {
        return;
    }

    io->safe_active = 1;
    io->digital_outputs = io->safe_outputs;
    write_outputs(io->safe_outputs);
    leap_hw_refresh_inputs(io);

    ESP_LOGW(TAG, "safe state: outputs off, PD outputs=0x%04X", io->digital_outputs);
}

void leap_hw_set_locate_led(uint8_t on)
{
#if LOCATE_LED_INVERT
    gpio_set_level(LOCATE_LED_PIN, on != 0u ? 0 : 1);
#else
    gpio_set_level(LOCATE_LED_PIN, on != 0u ? 1 : 0);
#endif
}
