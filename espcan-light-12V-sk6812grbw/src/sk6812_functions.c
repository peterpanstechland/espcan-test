// SK6812 GRBW 控制功能实现

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_random.h"
#include "driver/rmt_tx.h"

// 外部变量和定义
extern rmt_channel_handle_t rmt_channel;
extern rmt_encoder_handle_t led_encoder;
extern const char *TAG;

#define WS2812_LEDS_COUNT 900
#define T0H 3
#define T0L 9 
#define T1H 6
#define T1L 6
#define TRS 800

// 前向声明内部函数
void setPixelGRBW(int index, uint8_t g, uint8_t r, uint8_t b, uint8_t w, rmt_symbol_word_t *led_data);
void refreshLEDs(rmt_symbol_word_t *led_data);
void sendPixels(rmt_symbol_word_t *pixel_data, size_t data_size);
void byteToRMTSymbols(uint8_t byte_val, rmt_symbol_word_t *symbols);

// 将字节值转换为RMT符号 (8个位)
void byteToRMTSymbols(uint8_t byte_val, rmt_symbol_word_t *symbols)
{
    for (int i = 7; i >= 0; i--) {  // MSB优先
        if (byte_val & (1 << i)) {
            // 发送'1'位
            symbols[7-i].level0 = 1;
            symbols[7-i].duration0 = T1H;
            symbols[7-i].level1 = 0;
            symbols[7-i].duration1 = T1L;
        } else {
            // 发送'0'位  
            symbols[7-i].level0 = 1;
            symbols[7-i].duration0 = T0H;
            symbols[7-i].level1 = 0;
            symbols[7-i].duration1 = T0L;
        }
    }
}

// 发送像素数据
void sendPixels(rmt_symbol_word_t *pixel_data, size_t data_size)
{
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // 不循环
    };

    esp_err_t ret = rmt_transmit(rmt_channel, led_encoder, pixel_data, 
                                data_size * sizeof(rmt_symbol_word_t), &tx_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "发送像素数据失败: %s", esp_err_to_name(ret));
    }

    // 等待传输完成
    ret = rmt_tx_wait_all_done(rmt_channel, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "等待传输完成超时: %s", esp_err_to_name(ret));
    }
}

// 设置单个LED的GRBW颜色
void setPixelGRBW(int index, uint8_t g, uint8_t r, uint8_t b, uint8_t w, 
                  rmt_symbol_word_t *led_data)
{
    if (index >= WS2812_LEDS_COUNT) return;
    
    int offset = index * 32;
    byteToRMTSymbols(g, &led_data[offset + 0]);   // 绿色
    byteToRMTSymbols(r, &led_data[offset + 8]);   // 红色  
    byteToRMTSymbols(b, &led_data[offset + 16]);  // 蓝色
    byteToRMTSymbols(w, &led_data[offset + 24]);  // 白色
}

// 更新整个LED条
void refreshLEDs(rmt_symbol_word_t *led_data)
{
    size_t total_symbols = WS2812_LEDS_COUNT * 32 + 1;
    
    // 添加最终的复位信号
    led_data[WS2812_LEDS_COUNT * 32].level0 = 0;
    led_data[WS2812_LEDS_COUNT * 32].duration0 = TRS;
    led_data[WS2812_LEDS_COUNT * 32].level1 = 0;
    led_data[WS2812_LEDS_COUNT * 32].duration1 = 0;
    
    // 发送数据
    sendPixels(led_data, total_symbols);
}

// 为所有LED设置相同颜色
void setAllLEDs(uint8_t g, uint8_t r, uint8_t b, uint8_t w)
{
    // 为所有LED分配内存：每个LED 32位数据 + 最后一个复位信号
    size_t total_symbols = WS2812_LEDS_COUNT * 32 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    
    if (led_data == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }
    
    // 为每个LED构建GRBW数据
    for (int led = 0; led < WS2812_LEDS_COUNT; led++) {
        setPixelGRBW(led, g, r, b, w, led_data);
    }
    
    // 更新显示
    refreshLEDs(led_data);
    
    // 释放内存
    free(led_data);
}

// 关闭所有LED
void clearAllLEDs(void)
{
    setAllLEDs(0, 0, 0, 0);  // 所有颜色设为0
}

// 彩虹效果实现 (GRBW版本)
void rainbow_effect_grbw(int delay_ms) {
    static uint8_t hue = 0;
    
    // 分配LED数据内存
    size_t total_symbols = WS2812_LEDS_COUNT * 32 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data == NULL) {
        return;
    }
    
    // 创建彩虹效果
    for (int i = 0; i < WS2812_LEDS_COUNT; i++) {
        // 计算每个LED的色调，形成彩虹
        uint8_t pos = (i * 256 / WS2812_LEDS_COUNT + hue) & 0xFF;
        
        // 将HSV转换为GRBW (简化版彩虹算法，白色通道设为0)
        if (pos < 85) {
            setPixelGRBW(i, pos * 3, 255 - pos * 3, 0, 0, led_data);
        } else if (pos < 170) {
            pos -= 85;
            setPixelGRBW(i, 255 - pos * 3, 0, pos * 3, 0, led_data);
        } else {
            pos -= 170;
            setPixelGRBW(i, 0, pos * 3, 255 - pos * 3, 0, led_data);
        }
    }
    
    // 更新显示
    refreshLEDs(led_data);
    free(led_data);
    
    // 移动彩虹
    hue += 2;
    
    // 延时
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

// 紫色追逐效果实现 (GRBW版本)
void purple_chase_effect_grbw(int delay_ms) {
    static int position = 0;
    
    // 分配LED数据内存
    size_t total_symbols = WS2812_LEDS_COUNT * 32 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data == NULL) {
        return;
    }
    
    // 先清空所有LED
    for (int i = 0; i < WS2812_LEDS_COUNT; i++) {
        setPixelGRBW(i, 0, 0, 0, 0, led_data);
    }
    
    // 追逐灯的长度
    const int chase_length = 8;
    
    // 创建紫色追逐效果
    for (int i = 0; i < chase_length; i++) {
        int pos = (position + i) % WS2812_LEDS_COUNT;
        // 根据距离头部的位置，亮度逐渐降低
        uint8_t brightness = 255 - (i * 255 / chase_length);
        setPixelGRBW(pos, 0, brightness, brightness, 0, led_data); // 紫色：红+蓝
    }
    
    // 更新显示
    refreshLEDs(led_data);
    free(led_data);
    
    // 移动追逐位置
    position = (position + 1) % WS2812_LEDS_COUNT;
    
    // 延时
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

// 蓝色闪电效果实现 (GRBW版本)
void blue_lightning_effect_grbw(int delay_ms) {
    // 分配LED数据内存
    size_t total_symbols = WS2812_LEDS_COUNT * 32 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data == NULL) {
        return;
    }
    
    // 先清空所有LED
    for (int i = 0; i < WS2812_LEDS_COUNT; i++) {
        setPixelGRBW(i, 0, 0, 0, 0, led_data);
    }
    
    // 随机生成闪电位置
    int num_flashes = 3 + (esp_random() % 4); // 3-6个闪电点
    
    for (int i = 0; i < num_flashes; i++) {
        int pos = esp_random() % WS2812_LEDS_COUNT;
        
        // 设置闪电 - 蓝白色，使用白色通道增强亮度
        uint8_t intensity = 150 + (esp_random() % 105); // 150-255
        setPixelGRBW(pos, intensity/3, intensity/3, intensity, intensity/2, led_data);
        
        // 闪电周围有淡蓝色光晕
        if (pos > 0) {
            setPixelGRBW(pos-1, 20, 20, 120, 0, led_data);
        }
        if (pos < WS2812_LEDS_COUNT-1) {
            setPixelGRBW(pos+1, 20, 20, 120, 0, led_data);
        }
    }
    
    // 更新显示
    refreshLEDs(led_data);
    free(led_data);
    
    // 闪电持续时间短
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    
    // 随机决定是否有黑暗期
    if ((esp_random() % 5) == 0) {
        clearAllLEDs();
        vTaskDelay(pdMS_TO_TICKS(delay_ms * 2));
    }
}

// 呼吸灯效果实现 (GRBW版本) - 切换颜色
void breathing_light_effect_grbw(int delay_ms) {
    static float breath_level = 0.0f;
    static int direction = 1;  // 1 = 增加亮度, -1 = 减少亮度
    static int color_mode = 0; // 颜色模式: 0=暖白, 1=红, 2=绿, 3=蓝
    static uint32_t last_color_change = 0;
    static uint32_t tick_count = 0;
    
    tick_count++;
    
    // 每隔一段时间切换颜色
    if (tick_count - last_color_change > 200) { // 大约每10秒切换一次颜色
        color_mode = (color_mode + 1) % 4;
        last_color_change = tick_count;
    }
    
    // 根据颜色模式设置基础颜色
    uint8_t base_g = 0, base_r = 0, base_b = 0, base_w = 0;
    
    switch (color_mode) {
        case 0: // 暖白色
            base_g = 50; base_r = 80; base_b = 30; base_w = 255;
            break;
        case 1: // 红色
            base_r = 255;
            break;
        case 2: // 绿色
            base_g = 255;
            break;
        case 3: // 蓝色
            base_b = 255;
            break;
    }
    
    // 计算当前亮度级别
    float intensity = breath_level * breath_level; // 使用平方关系使变化看起来更自然
    
    // 设置所有LED为相同的呼吸亮度
    setAllLEDs(base_g * intensity, base_r * intensity, 
               base_b * intensity, base_w * intensity);
    
    // 更新呼吸级别
    breath_level += direction * 0.01f;
    
    // 改变方向
    if (breath_level >= 1.0f) {
        breath_level = 1.0f;
        direction = -1;
    } else if (breath_level <= 0.0f) {
        breath_level = 0.0f;
        direction = 1;
    }
    
    // 延时
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
} 