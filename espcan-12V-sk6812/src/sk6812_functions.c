// SK6812 控制功能实现

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
void setPixelRGB(int index, uint8_t r, uint8_t g, uint8_t b, rmt_symbol_word_t *led_data);
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

// 设置单个LED的RGB颜色
void setPixelRGB(int index, uint8_t r, uint8_t g, uint8_t b, rmt_symbol_word_t *led_data)
{
    if (index >= WS2812_LEDS_COUNT) return;
    
    // SK6812的顺序是GRB
    int offset = index * 24;  // 每个LED使用24位 (3个字节)
    byteToRMTSymbols(g, &led_data[offset + 0]);   // 绿色
    byteToRMTSymbols(r, &led_data[offset + 8]);   // 红色  
    byteToRMTSymbols(b, &led_data[offset + 16]);  // 蓝色
}

// 更新整个LED条
void refreshLEDs(rmt_symbol_word_t *led_data)
{
    size_t total_symbols = WS2812_LEDS_COUNT * 24 + 1;
    
    // 添加最终的复位信号
    led_data[WS2812_LEDS_COUNT * 24].level0 = 0;
    led_data[WS2812_LEDS_COUNT * 24].duration0 = TRS;
    led_data[WS2812_LEDS_COUNT * 24].level1 = 0;
    led_data[WS2812_LEDS_COUNT * 24].duration1 = 0;
    
    // 发送数据
    sendPixels(led_data, total_symbols);
}

// 为所有LED设置相同颜色
void setAllLEDs(uint8_t r, uint8_t g, uint8_t b)
{
    // 为所有LED分配内存：每个LED 24位数据 + 最后一个复位信号
    size_t total_symbols = WS2812_LEDS_COUNT * 24 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    
    if (led_data == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }
    
    // 为每个LED构建RGB数据
    for (int led = 0; led < WS2812_LEDS_COUNT; led++) {
        setPixelRGB(led, r, g, b, led_data);
    }
    
    // 更新显示
    refreshLEDs(led_data);
    
    // 释放内存
    free(led_data);
}

// 关闭所有LED
void clearAllLEDs(void)
{
    setAllLEDs(0, 0, 0);  // 所有颜色设为0
}

// 彩虹效果实现
void rainbow_effect(int delay_ms) {
    static uint8_t hue = 0;
    
    // 分配LED数据内存
    size_t total_symbols = WS2812_LEDS_COUNT * 24 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data == NULL) {
        return;
    }
    
    // 创建彩虹效果
    for (int i = 0; i < WS2812_LEDS_COUNT; i++) {
        // 计算每个LED的色调，形成彩虹
        uint8_t pos = (i * 256 / WS2812_LEDS_COUNT + hue) & 0xFF;
        
        // 将HSV转换为RGB (简化版彩虹算法)
        if (pos < 85) {
            setPixelRGB(i, 255 - pos * 3, pos * 3, 0, led_data);
        } else if (pos < 170) {
            pos -= 85;
            setPixelRGB(i, 0, 255 - pos * 3, pos * 3, led_data);
        } else {
            pos -= 170;
            setPixelRGB(i, pos * 3, 0, 255 - pos * 3, led_data);
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

// 紫色追逐效果实现
void purple_chase_effect(int delay_ms) {
    static int position = 0;
    
    // 分配LED数据内存
    size_t total_symbols = WS2812_LEDS_COUNT * 24 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data == NULL) {
        return;
    }
    
    // 先清空所有LED
    for (int i = 0; i < WS2812_LEDS_COUNT; i++) {
        setPixelRGB(i, 0, 0, 0, led_data);
    }
    
    // 追逐灯的长度
    const int chase_length = 8;
    
    // 创建紫色追逐效果
    for (int i = 0; i < chase_length; i++) {
        int pos = (position + i) % WS2812_LEDS_COUNT;
        // 根据距离头部的位置，亮度逐渐降低
        uint8_t brightness = 255 - (i * 255 / chase_length);
        setPixelRGB(pos, brightness, 0, brightness, led_data); // 紫色：红+蓝
    }
    
    // 更新显示
    refreshLEDs(led_data);
    free(led_data);
    
    // 移动追逐位置
    position = (position + 1) % WS2812_LEDS_COUNT;
    
    // 延时
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

// 蓝色闪电效果实现
void blue_lightning_effect(int delay_ms) {
    // 分配LED数据内存
    size_t total_symbols = WS2812_LEDS_COUNT * 24 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data == NULL) {
        return;
    }
    
    // 先清空所有LED
    for (int i = 0; i < WS2812_LEDS_COUNT; i++) {
        setPixelRGB(i, 0, 0, 0, led_data);
    }
    
    // 随机生成闪电位置
    int num_flashes = 3 + (esp_random() % 4); // 3-6个闪电点
    
    for (int i = 0; i < num_flashes; i++) {
        int pos = esp_random() % WS2812_LEDS_COUNT;
        
        // 设置闪电 - 蓝白色
        uint8_t intensity = 150 + (esp_random() % 105); // 150-255
        setPixelRGB(pos, intensity/3, intensity/3, intensity, led_data);
        
        // 闪电周围有淡蓝色光晕
        if (pos > 0) {
            setPixelRGB(pos-1, 20, 20, 120, led_data);
        }
        if (pos < WS2812_LEDS_COUNT-1) {
            setPixelRGB(pos+1, 20, 20, 120, led_data);
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

// 呼吸灯切换颜色效果
void breathing_light_effect(int delay_ms) {
    static uint8_t brightness = 0;
    static int8_t direction = 1; // 1=增加亮度，-1=减少亮度
    static uint8_t color_index = 0;
    static const uint8_t colors[][3] = {
        {255, 0, 0},    // 红色
        {0, 255, 0},    // 绿色
        {0, 0, 255},    // 蓝色
        {255, 255, 0},  // 黄色
        {0, 255, 255},  // 青色
        {255, 0, 255},  // 紫色
    };
    static const int num_colors = sizeof(colors) / sizeof(colors[0]);
    
    // 分配LED数据内存
    size_t total_symbols = WS2812_LEDS_COUNT * 24 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data == NULL) {
        return;
    }
    
    // 应用当前亮度到当前颜色
    uint8_t r = (colors[color_index][0] * brightness) / 255;
    uint8_t g = (colors[color_index][1] * brightness) / 255;
    uint8_t b = (colors[color_index][2] * brightness) / 255;
    
    // 设置所有LED
    for (int i = 0; i < WS2812_LEDS_COUNT; i++) {
        setPixelRGB(i, r, g, b, led_data);
    }
    
    // 更新显示
    refreshLEDs(led_data);
    free(led_data);
    
    // 更新亮度
    brightness += (direction * 5);
    
    // 检查是否需要改变方向
    if (brightness >= 250) {
        direction = -1;  // 开始减少亮度
    } else if (brightness <= 5) {
        direction = 1;   // 开始增加亮度
        
        // 在最暗的时候切换颜色
        color_index = (color_index + 1) % num_colors;
    }
    
    // 延时
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
} 