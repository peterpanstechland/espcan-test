#include "sk6812.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sk6812";

// SK6812 灯带结构体
struct sk6812_strip_t {
    rmt_channel_handle_t rmt_channel;
    rmt_encoder_handle_t encoder;
    uint8_t *pixel_buf;
    uint16_t led_count;
    uint8_t gpio_num;
};

// RMT编码器结构体
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} sk6812_encoder_t;

// 编码器状态
enum {
    SK6812_ENCODE_DATA = 0,
    SK6812_ENCODE_RESET,
    SK6812_ENCODE_COMPLETE
};

// RMT符号定义
static const rmt_symbol_word_t sk6812_bit0 = {
    .level0 = 1,
    .duration0 = 3,  // T0H = 300ns (at 10MHz)
    .level1 = 0,
    .duration1 = 9   // T0L = 900ns (at 10MHz)
};

static const rmt_symbol_word_t sk6812_bit1 = {
    .level0 = 1,
    .duration0 = 6,  // T1H = 600ns (at 10MHz)
    .level1 = 0,
    .duration1 = 6   // T1L = 600ns (at 10MHz)
};

// 编码器回调函数
static size_t sk6812_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                           const void *primary_data, size_t data_size,
                           rmt_encode_state_t *ret_state)
{
    sk6812_encoder_t *sk6812_encoder = __containerof(encoder, sk6812_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = sk6812_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = sk6812_encoder->copy_encoder;
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    
    switch (sk6812_encoder->state) {
        case SK6812_ENCODE_DATA:
            encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                sk6812_encoder->state = SK6812_ENCODE_RESET;
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out;
            }
            // fall-through
        case SK6812_ENCODE_RESET:
            encoded_symbols += copy_encoder->encode(copy_encoder, channel, &sk6812_encoder->reset_code,
                                                  sizeof(sk6812_encoder->reset_code), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                sk6812_encoder->state = SK6812_ENCODE_COMPLETE;
                state |= RMT_ENCODING_COMPLETE;
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out;
            }
            // fall-through
        case SK6812_ENCODE_COMPLETE:
            break;
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

// 编码器重置
static esp_err_t sk6812_encoder_reset(rmt_encoder_t *encoder)
{
    sk6812_encoder_t *sk6812_encoder = __containerof(encoder, sk6812_encoder_t, base);
    rmt_encoder_reset(sk6812_encoder->bytes_encoder);
    rmt_encoder_reset(sk6812_encoder->copy_encoder);
    sk6812_encoder->state = SK6812_ENCODE_DATA;
    return ESP_OK;
}

// 编码器删除
static esp_err_t sk6812_encoder_del(rmt_encoder_t *encoder)
{
    sk6812_encoder_t *sk6812_encoder = __containerof(encoder, sk6812_encoder_t, base);
    rmt_del_encoder(sk6812_encoder->bytes_encoder);
    rmt_del_encoder(sk6812_encoder->copy_encoder);
    free(sk6812_encoder);
    return ESP_OK;
}

// 创建编码器
static esp_err_t sk6812_new_encoder(uint32_t resolution, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    sk6812_encoder_t *sk6812_encoder = NULL;
    
    ESP_GOTO_ON_FALSE(ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    
    sk6812_encoder = calloc(1, sizeof(sk6812_encoder_t));
    ESP_GOTO_ON_FALSE(sk6812_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for sk6812 encoder");
    
    sk6812_encoder->base.encode = sk6812_encode;
    sk6812_encoder->base.del = sk6812_encoder_del;
    sk6812_encoder->base.reset = sk6812_encoder_reset;
    
    // 创建字节编码器
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = sk6812_bit0,
        .bit1 = sk6812_bit1,
        .flags.msb_first = 1
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &sk6812_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");
    
    // 创建复制编码器
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &sk6812_encoder->copy_encoder), err, TAG, "create copy encoder failed");
    
    // 计算复位码
    uint32_t reset_ticks = resolution / 1000000 * SK6812_RESET_US;
    sk6812_encoder->reset_code = (rmt_symbol_word_t) {
        .level0 = 0,
        .duration0 = reset_ticks,
        .level1 = 0,
        .duration1 = 0,
    };
    
    *ret_encoder = &sk6812_encoder->base;
    return ESP_OK;
    
err:
    if (sk6812_encoder) {
        if (sk6812_encoder->bytes_encoder) {
            rmt_del_encoder(sk6812_encoder->bytes_encoder);
        }
        if (sk6812_encoder->copy_encoder) {
            rmt_del_encoder(sk6812_encoder->copy_encoder);
        }
        free(sk6812_encoder);
    }
    return ret;
}

esp_err_t sk6812_new(const sk6812_config_t *config, sk6812_handle_t *handle)
{
    esp_err_t ret = ESP_OK;
    
    ESP_RETURN_ON_FALSE(config && handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(config->led_count > 0, ESP_ERR_INVALID_ARG, TAG, "invalid led count");
    
    struct sk6812_strip_t *strip = calloc(1, sizeof(struct sk6812_strip_t));
    ESP_RETURN_ON_FALSE(strip, ESP_ERR_NO_MEM, TAG, "no mem for strip");
    
    // 分配像素缓冲区 (每个像素4字节: GRBW)
    strip->pixel_buf = calloc(config->led_count, 4);
    if (!strip->pixel_buf) {
        free(strip);
        ESP_RETURN_ON_ERROR(ESP_ERR_NO_MEM, TAG, "no mem for pixel buffer");
    }
    
    strip->led_count = config->led_count;
    strip->gpio_num = config->gpio_num;
    
    // 创建RMT发送通道
    rmt_tx_channel_config_t tx_config = {
        .gpio_num = config->gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = config->resolution_hz,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags.invert_out = false,
        .flags.with_dma = false,
    };
    
    ESP_GOTO_ON_ERROR(rmt_new_tx_channel(&tx_config, &strip->rmt_channel), err, TAG, "create RMT TX channel failed");
    
    // 创建编码器
    ESP_GOTO_ON_ERROR(sk6812_new_encoder(config->resolution_hz, &strip->encoder), err, TAG, "create encoder failed");
    
    *handle = strip;
    return ESP_OK;
    
err:
    if (strip) {
        if (strip->rmt_channel) {
            rmt_del_channel(strip->rmt_channel);
        }
        if (strip->pixel_buf) {
            free(strip->pixel_buf);
        }
        free(strip);
    }
    return ret;
}

esp_err_t sk6812_del(sk6812_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    
    if (handle->encoder) {
        rmt_del_encoder(handle->encoder);
    }
    if (handle->rmt_channel) {
        rmt_del_channel(handle->rmt_channel);
    }
    if (handle->pixel_buf) {
        free(handle->pixel_buf);
    }
    free(handle);
    
    return ESP_OK;
}

esp_err_t sk6812_set_pixel(sk6812_handle_t handle, uint16_t index, sk6812_color_t color)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(index < handle->led_count, ESP_ERR_INVALID_ARG, TAG, "index out of range");
    
    uint8_t *pixel = &handle->pixel_buf[index * 4];
    pixel[0] = color.g;
    pixel[1] = color.r;
    pixel[2] = color.b;
    pixel[3] = color.w;
    
    return ESP_OK;
}

esp_err_t sk6812_set_pixel_grbw(sk6812_handle_t handle, uint16_t index, uint8_t g, uint8_t r, uint8_t b, uint8_t w)
{
    sk6812_color_t color = {.g = g, .r = r, .b = b, .w = w};
    return sk6812_set_pixel(handle, index, color);
}

esp_err_t sk6812_clear(sk6812_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    
    memset(handle->pixel_buf, 0, handle->led_count * 4);
    return ESP_OK;
}

esp_err_t sk6812_refresh(sk6812_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    
    ESP_RETURN_ON_ERROR(rmt_transmit(handle->rmt_channel, handle->encoder, handle->pixel_buf, 
                                   handle->led_count * 4, &tx_config), TAG, "transmit failed");
    
    return ESP_OK;
}

esp_err_t sk6812_enable(sk6812_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    
    ESP_RETURN_ON_ERROR(rmt_enable(handle->rmt_channel), TAG, "enable RMT channel failed");
    return ESP_OK;
}

esp_err_t sk6812_disable(sk6812_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    
    ESP_RETURN_ON_ERROR(rmt_disable(handle->rmt_channel), TAG, "disable RMT channel failed");
    return ESP_OK;
} 