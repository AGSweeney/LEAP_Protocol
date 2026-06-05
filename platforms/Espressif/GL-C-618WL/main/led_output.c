// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#include "led_output.h"
#include "board_config.h"

#include <stdlib.h>
#include <string.h>

#include "driver/rmt_tx.h"
#include "esp_log.h"

static const char *TAG = "led_output";

#define WS2812_T0H_NS   350
#define WS2812_T0L_NS   900
#define WS2812_T1H_NS   900
#define WS2812_T1L_NS   350
#define WS2812_RESET_NS 280000

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} ws2812_encoder_t;

typedef struct {
    rmt_channel_handle_t channel;
    rmt_encoder_t       *encoder;
    int                  gpio_num;
} led_strip_t;

static led_strip_t s_strips[LED_CHANNEL_COUNT];
static uint8_t     s_zero_pixels[DEFAULT_NUM_LEDS * 3u];

static size_t ws2812_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                            const void *primary_data, size_t data_size,
                            rmt_encode_state_t *ret_state)
{
    ws2812_encoder_t *ws = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    switch (ws->state) {
    case 0:
        encoded_symbols += ws->bytes_encoder->encode(
            ws->bytes_encoder, channel, primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws->state = 1;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            *ret_state = (rmt_encode_state_t)((int)*ret_state | (int)RMT_ENCODING_MEM_FULL);
            return encoded_symbols;
        }
        /* fall through */
    case 1:
        encoded_symbols += ws->copy_encoder->encode(
            ws->copy_encoder, channel, &ws->reset_code,
            sizeof(ws->reset_code), &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws->state = RMT_ENCODING_RESET;
            *ret_state = (rmt_encode_state_t)((int)*ret_state | (int)RMT_ENCODING_COMPLETE);
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            *ret_state = (rmt_encode_state_t)((int)*ret_state | (int)RMT_ENCODING_MEM_FULL);
        }
        break;
    }
    return encoded_symbols;
}

static esp_err_t ws2812_encoder_del(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *ws = __containerof(encoder, ws2812_encoder_t, base);
    if (ws->bytes_encoder) {
        ws->bytes_encoder->del(ws->bytes_encoder);
    }
    if (ws->copy_encoder) {
        ws->copy_encoder->del(ws->copy_encoder);
    }
    free(ws);
    return ESP_OK;
}

static esp_err_t ws2812_encoder_reset(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *ws = __containerof(encoder, ws2812_encoder_t, base);
    ws->bytes_encoder->reset(ws->bytes_encoder);
    ws->copy_encoder->reset(ws->copy_encoder);
    ws->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static esp_err_t create_ws2812_encoder(rmt_encoder_t **out_encoder)
{
    ws2812_encoder_t *ws;
    esp_err_t ret;

    ws = calloc(1, sizeof(ws2812_encoder_t));
    if (!ws) {
        return ESP_ERR_NO_MEM;
    }

    ws->base.encode = ws2812_encode;
    ws->base.del    = ws2812_encoder_del;
    ws->base.reset  = ws2812_encoder_reset;

    rmt_bytes_encoder_config_t bytes_cfg = {
        .bit0 = {
            .duration0 = WS2812_T0H_NS / 100,
            .level0    = 1,
            .duration1 = WS2812_T0L_NS / 100,
            .level1    = 0,
        },
        .bit1 = {
            .duration0 = WS2812_T1H_NS / 100,
            .level0    = 1,
            .duration1 = WS2812_T1L_NS / 100,
            .level1    = 0,
        },
        .flags.msb_first = true,
    };
    ret = rmt_new_bytes_encoder(&bytes_cfg, &ws->bytes_encoder);
    if (ret != ESP_OK) {
        free(ws);
        return ret;
    }

    rmt_copy_encoder_config_t copy_cfg = {};
    ret = rmt_new_copy_encoder(&copy_cfg, &ws->copy_encoder);
    if (ret != ESP_OK) {
        ws->bytes_encoder->del(ws->bytes_encoder);
        free(ws);
        return ret;
    }

    {
        uint32_t reset_ticks = WS2812_RESET_NS / 100;
        ws->reset_code = (rmt_symbol_word_t){
            .duration0 = reset_ticks / 2,
            .level0    = 0,
            .duration1 = reset_ticks / 2,
            .level1    = 0,
        };
    }

    *out_encoder = &ws->base;
    return ESP_OK;
}

esp_err_t led_output_init_channel(uint8_t channel, int gpio_num, uint16_t num_leds)
{
    led_strip_t *strip;
    esp_err_t ret;

    (void)num_leds;

    if (channel >= LED_CHANNEL_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    strip = &s_strips[channel];
    memset(strip, 0, sizeof(*strip));

    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num           = gpio_num,
        .clk_src            = RMT_CLK_SRC_DEFAULT,
        .resolution_hz      = 10000000,
        .mem_block_symbols  = 256,
        .trans_queue_depth  = 4,
    };
    ret = rmt_new_tx_channel(&tx_cfg, &strip->channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT TX channel %u creation failed: %s",
                 (unsigned)channel, esp_err_to_name(ret));
        return ret;
    }

    ret = create_ws2812_encoder(&strip->encoder);
    if (ret != ESP_OK) {
        rmt_del_channel(strip->channel);
        memset(strip, 0, sizeof(*strip));
        return ret;
    }

    ret = rmt_enable(strip->channel);
    if (ret != ESP_OK) {
        strip->encoder->del(strip->encoder);
        rmt_del_channel(strip->channel);
        memset(strip, 0, sizeof(*strip));
        return ret;
    }

    strip->gpio_num = gpio_num;
    ESP_LOGI(TAG, "LED channel %u initialized on GPIO%d",
             (unsigned)channel, gpio_num);
    return ESP_OK;
}

esp_err_t led_output_show_channel(uint8_t channel, const uint8_t *pixels, uint16_t num_leds)
{
    const led_strip_t *strip;
    rmt_transmit_config_t tx_config = { .loop_count = 0 };

    if (channel >= LED_CHANNEL_COUNT || pixels == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    strip = &s_strips[channel];
    if (!strip->channel || !strip->encoder) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = rmt_transmit(strip->channel, strip->encoder, pixels,
                                 num_leds * 3, &tx_config);
    if (ret != ESP_OK) {
        return ret;
    }

    return rmt_tx_wait_all_done(strip->channel, 1000);
}

esp_err_t led_output_clear_channel(uint8_t channel, uint16_t num_leds)
{
    if (num_leds > DEFAULT_NUM_LEDS) {
        return ESP_ERR_INVALID_ARG;
    }

    return led_output_show_channel(channel, s_zero_pixels, num_leds);
}

esp_err_t led_output_clear_all(uint16_t num_leds)
{
    uint8_t ch;
    esp_err_t ret = ESP_OK;

    for (ch = 0; ch < LED_CHANNEL_COUNT; ++ch) {
        esp_err_t ch_ret = led_output_clear_channel(ch, num_leds);
        if (ch_ret != ESP_OK) {
            ret = ch_ret;
        }
    }

    return ret;
}
