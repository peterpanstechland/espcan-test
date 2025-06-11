// SK6812 控制功能实现

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_random.h"
#include "driver/rmt_tx.h"

// 外部变量和定义
extern rmt_channel_handle_t rmt_channel_1;
extern rmt_channel_handle_t rmt_channel_2;
extern rmt_encoder_handle_t led_encoder_1;
extern rmt_encoder_handle_t led_encoder_2;
extern const char *TAG;

#define WS2812_LEDS_PER_STRIP 900
#define WS2812_LEDS_TOTAL (WS2812_LEDS_PER_STRIP * 2)
#define T0H 3
#define T0L 9 
#define T1H 6
#define T1L 6
#define TRS 800

// 前向声明内部函数
void setPixelRGB(int strip, int index, uint8_t r, uint8_t g, uint8_t b, rmt_symbol_word_t *led_data);
void refreshLEDs(int strip, rmt_symbol_word_t *led_data);
void sendPixels(int strip, rmt_symbol_word_t *pixel_data, size_t data_size);
void byteToRMTSymbols(uint8_t byte_val, rmt_symbol_word_t *symbols);

// 简单的正弦函数近似，输入范围[0,1]，输出范围[0,1]
float simple_sine(float x) {
    // 调整输入范围到[0, PI]
    float pi_x = x * 3.14159;
    // 使用泰勒级数的简化近似
    return pi_x - pi_x * pi_x * pi_x / 6.0;
}

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
void sendPixels(int strip, rmt_symbol_word_t *pixel_data, size_t data_size)
{
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // 不循环
    };

    // 选择对应的RMT通道
    rmt_channel_handle_t channel = (strip == 1) ? rmt_channel_1 : rmt_channel_2;
    rmt_encoder_handle_t encoder = (strip == 1) ? led_encoder_1 : led_encoder_2;

    esp_err_t ret = rmt_transmit(channel, encoder, pixel_data, 
                                data_size * sizeof(rmt_symbol_word_t), &tx_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "发送像素数据到灯带%d失败: %s", strip, esp_err_to_name(ret));
    }

    // 等待传输完成
    ret = rmt_tx_wait_all_done(channel, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "等待灯带%d传输完成超时: %s", strip, esp_err_to_name(ret));
    }
}

// 设置单个LED的RGB颜色
void setPixelRGB(int strip, int index, uint8_t r, uint8_t g, uint8_t b, rmt_symbol_word_t *led_data)
{
    if (index >= WS2812_LEDS_PER_STRIP) return;
    
    // SK6812的顺序是GRB
    int offset = index * 24;  // 每个LED使用24位 (3个字节)
    byteToRMTSymbols(g, &led_data[offset + 0]);   // 绿色
    byteToRMTSymbols(r, &led_data[offset + 8]);   // 红色  
    byteToRMTSymbols(b, &led_data[offset + 16]);  // 蓝色
}

// 更新一个LED灯带
void refreshLEDs(int strip, rmt_symbol_word_t *led_data)
{
    size_t total_symbols = WS2812_LEDS_PER_STRIP * 24 + 1;
    
    // 添加最终的复位信号
    led_data[WS2812_LEDS_PER_STRIP * 24].level0 = 0;
    led_data[WS2812_LEDS_PER_STRIP * 24].duration0 = TRS;
    led_data[WS2812_LEDS_PER_STRIP * 24].level1 = 0;
    led_data[WS2812_LEDS_PER_STRIP * 24].duration1 = 0;
    
    // 发送数据
    sendPixels(strip, led_data, total_symbols);
}

// 为所有LED设置相同颜色
void setAllLEDs(uint8_t r, uint8_t g, uint8_t b)
{
    // 为所有LED分配内存：每个LED 24位数据 + 最后一个复位信号
    size_t total_symbols = WS2812_LEDS_PER_STRIP * 24 + 1;
    
    // 创建灯带1的数据
    rmt_symbol_word_t *led_data_1 = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data_1 == NULL) {
        ESP_LOGE(TAG, "灯带1内存分配失败");
        return;
    }
    
    // 创建灯带2的数据
    rmt_symbol_word_t *led_data_2 = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data_2 == NULL) {
        ESP_LOGE(TAG, "灯带2内存分配失败");
        free(led_data_1);
        return;
    }
    
    // 为每个LED构建RGB数据
    for (int led = 0; led < WS2812_LEDS_PER_STRIP; led++) {
        setPixelRGB(1, led, r, g, b, led_data_1);
        setPixelRGB(2, led, r, g, b, led_data_2);
    }
    
    // 并行更新两个灯带
    refreshLEDs(1, led_data_1);
    refreshLEDs(2, led_data_2);
    
    // 释放内存
    free(led_data_1);
    free(led_data_2);
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
    size_t total_symbols = WS2812_LEDS_PER_STRIP * 24 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data == NULL) {
        return;
    }
    
    // 创建彩虹效果
    for (int i = 0; i < WS2812_LEDS_PER_STRIP; i++) {
        // 计算每个LED的色调，形成彩虹
        uint8_t pos = (i * 256 / WS2812_LEDS_PER_STRIP + hue) & 0xFF;
        
        // 将HSV转换为RGB (简化版彩虹算法)
        if (pos < 85) {
            setPixelRGB(1, i, 255 - pos * 3, pos * 3, 0, led_data);
            setPixelRGB(2, i, 255 - pos * 3, pos * 3, 0, led_data);
        } else if (pos < 170) {
            pos -= 85;
            setPixelRGB(1, i, 0, 255 - pos * 3, pos * 3, led_data);
            setPixelRGB(2, i, 0, 255 - pos * 3, pos * 3, led_data);
        } else {
            pos -= 170;
            setPixelRGB(1, i, pos * 3, 0, 255 - pos * 3, led_data);
            setPixelRGB(2, i, pos * 3, 0, 255 - pos * 3, led_data);
        }
    }
    
    // 更新显示
    refreshLEDs(1, led_data);
    refreshLEDs(2, led_data);
    free(led_data);
    
    // 移动彩虹
    hue += 2;
    
    // 延时
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

// 紫色追逐效果实现
void purple_chase_effect(int delay_ms) {
    static int position = 0;
    static uint8_t brightness_level = 255; // 用于脉冲亮度变化
    static int8_t brightness_direction = -1; // 亮度变化方向
    
    // 分配LED数据内存
    size_t total_symbols = WS2812_LEDS_PER_STRIP * 24 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data == NULL) {
        return;
    }
    
    // 先清空所有LED
    for (int i = 0; i < WS2812_LEDS_PER_STRIP; i++) {
        setPixelRGB(1, i, 0, 0, 0, led_data);
        setPixelRGB(2, i, 0, 0, 0, led_data);
    }
    
    // 追逐灯的长度 - 增加到约150个LED一组
    const int chase_length = 150;
    
    // 创建渐变的紫色光束 (紫色脉冲波)
    for (int i = 0; i < chase_length; i++) {
        int pos = (position + i) % WS2812_LEDS_PER_STRIP;
        
        // 根据位置计算渐变强度 - 使用正弦波形创建更平滑的渐变
        float progress = (float)i / chase_length;
        // 使用自定义简单正弦近似函数
        float intensity = simple_sine(progress) * brightness_level / 255.0;
        
        // 计算紫色亮度
        uint8_t r_val = (uint8_t)(220 * intensity); // 红色成分
        uint8_t b_val = (uint8_t)(255 * intensity); // 蓝色成分
        
        // 在光束的不同部分使用不同的紫色色调
        if (i < chase_length / 3) {
            // 偏蓝紫色
            setPixelRGB(1, pos, r_val * 0.7, 0, b_val, led_data);
            setPixelRGB(2, pos, r_val * 0.7, 0, b_val, led_data);
        } else if (i < 2 * chase_length / 3) {
            // 标准紫色
            setPixelRGB(1, pos, r_val, 0, b_val, led_data);
            setPixelRGB(2, pos, r_val, 0, b_val, led_data);
        } else {
            // 偏红紫色
            setPixelRGB(1, pos, r_val, 0, b_val * 0.7, led_data);
            setPixelRGB(2, pos, r_val, 0, b_val * 0.7, led_data);
        }
    }
    
    // 创建第二个紫色追逐组，与第一个距离适当间隔
    int second_group_pos = (position + WS2812_LEDS_PER_STRIP/2) % WS2812_LEDS_PER_STRIP;
    for (int i = 0; i < chase_length; i++) {
        int pos = (second_group_pos + i) % WS2812_LEDS_PER_STRIP;
        
        // 与第一组类似但颜色亮度稍有不同
        float progress = (float)i / chase_length;
        // 使用自定义简单正弦近似函数
        float intensity = simple_sine(progress) * brightness_level / 255.0;
        
        uint8_t r_val = (uint8_t)(200 * intensity); // 稍暗的红色
        uint8_t b_val = (uint8_t)(255 * intensity); // 蓝色
        
        // 第二组使用略有不同的紫色色调
        if (i < chase_length / 3) {
            setPixelRGB(1, pos, r_val * 0.8, 0, b_val, led_data);
            setPixelRGB(2, pos, r_val * 0.8, 0, b_val, led_data);
        } else if (i < 2 * chase_length / 3) {
            setPixelRGB(1, pos, r_val, 0, b_val * 0.9, led_data);
            setPixelRGB(2, pos, r_val, 0, b_val * 0.9, led_data);
        } else {
            setPixelRGB(1, pos, r_val, 0, b_val * 0.8, led_data);
            setPixelRGB(2, pos, r_val, 0, b_val * 0.8, led_data);
        }
    }
    
    // 更新显示
    refreshLEDs(1, led_data);
    refreshLEDs(2, led_data);
    free(led_data);
    
    // 移动追逐位置 - 加快移动速度
    position = (position + 5) % WS2812_LEDS_PER_STRIP;
    
    // 更新亮度级别，制造脉冲呼吸效果
    brightness_level += brightness_direction * 5;
    if (brightness_level <= 180) {
        brightness_direction = 1;  // 开始增加亮度
    } else if (brightness_level >= 255) {
        brightness_direction = -1; // 开始减少亮度
    }
    
    // 降低延时提高频率
    vTaskDelay(pdMS_TO_TICKS(delay_ms / 3));
}

// 蓝色闪电效果实现
void blue_lightning_effect(int delay_ms) {
    // 分配LED数据内存
    size_t total_symbols = WS2812_LEDS_PER_STRIP * 24 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data == NULL) {
        return;
    }
    
    // 先清空所有LED
    for (int i = 0; i < WS2812_LEDS_PER_STRIP; i++) {
        setPixelRGB(1, i, 0, 0, 0, led_data);
        setPixelRGB(2, i, 0, 0, 0, led_data);
    }
    
    // 增加闪电数量和随机性
    int num_flashes = 8 + (esp_random() % 8); // 8-15个闪电点
    
    // 随机决定是全白、全蓝或者白蓝交替
    uint8_t lightning_type = esp_random() % 3; // 0=白色, 1=蓝色, 2=混合
    
    for (int i = 0; i < num_flashes; i++) {
        // 随机生成闪电位置，可能形成区域性闪电
        int base_pos = esp_random() % WS2812_LEDS_PER_STRIP;
        int lightning_length = 1 + (esp_random() % 5); // 1-5个LED的闪电长度
        
        // 随机决定闪电强度
        uint8_t intensity = 200 + (esp_random() % 55); // 200-255，更亮
        
        for (int j = 0; j < lightning_length; j++) {
            int pos = (base_pos + j) % WS2812_LEDS_PER_STRIP;
            
            // 根据闪电类型和位置设置颜色
            if (lightning_type == 0 || (lightning_type == 2 && (i % 2 == 0))) {
                // 纯白色闪电
                setPixelRGB(1, pos, intensity, intensity, intensity, led_data);
                setPixelRGB(2, pos, intensity, intensity, intensity, led_data);
            } else {
                // 蓝色闪电
                setPixelRGB(1, pos, intensity/8, intensity/5, intensity, led_data);
                setPixelRGB(2, pos, intensity/8, intensity/5, intensity, led_data);
            }
            
            // 添加较大的光晕效果
            int halo_size = 2 + (esp_random() % 3); // 2-4个LED的光晕
            for (int k = 1; k <= halo_size; k++) {
                // 光晕强度随距离衰减
                uint8_t halo_intensity = intensity / (k * 2);
                
                // 前面的光晕
                if (pos - k >= 0) {
                    if (lightning_type == 0 || (lightning_type == 2 && (i % 2 == 0))) {
                        // 白色光晕
                        setPixelRGB(1, pos - k, halo_intensity, halo_intensity, halo_intensity, led_data);
                        setPixelRGB(2, pos - k, halo_intensity, halo_intensity, halo_intensity, led_data);
                    } else {
                        // 蓝色光晕
                        setPixelRGB(1, pos - k, halo_intensity/8, halo_intensity/5, halo_intensity, led_data);
                        setPixelRGB(2, pos - k, halo_intensity/8, halo_intensity/5, halo_intensity, led_data);
                    }
                }
                
                // 后面的光晕
                if (pos + k < WS2812_LEDS_PER_STRIP) {
                    if (lightning_type == 0 || (lightning_type == 2 && (i % 2 == 0))) {
                        // 白色光晕
                        setPixelRGB(1, pos + k, halo_intensity, halo_intensity, halo_intensity, led_data);
                        setPixelRGB(2, pos + k, halo_intensity, halo_intensity, halo_intensity, led_data);
                    } else {
                        // 蓝色光晕
                        setPixelRGB(1, pos + k, halo_intensity/8, halo_intensity/5, halo_intensity, led_data);
                        setPixelRGB(2, pos + k, halo_intensity/8, halo_intensity/5, halo_intensity, led_data);
                    }
                }
            }
        }
    }
    
    // 更新显示
    refreshLEDs(1, led_data);
    refreshLEDs(2, led_data);
    free(led_data);
    
    // 闪电持续时间更短，更爆闪
    vTaskDelay(pdMS_TO_TICKS(delay_ms / 2));
    
    // 高几率出现黑暗期以增强爆闪效果
    if ((esp_random() % 3) == 0) {
        clearAllLEDs();
        vTaskDelay(pdMS_TO_TICKS(delay_ms / 4));
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
    size_t total_symbols = WS2812_LEDS_PER_STRIP * 24 + 1;
    rmt_symbol_word_t *led_data = malloc(total_symbols * sizeof(rmt_symbol_word_t));
    if (led_data == NULL) {
        return;
    }
    
    // 应用当前亮度到当前颜色
    uint8_t r = (colors[color_index][0] * brightness) / 255;
    uint8_t g = (colors[color_index][1] * brightness) / 255;
    uint8_t b = (colors[color_index][2] * brightness) / 255;
    
    // 设置所有LED
    for (int i = 0; i < WS2812_LEDS_PER_STRIP; i++) {
        setPixelRGB(1, i, r, g, b, led_data);
        setPixelRGB(2, i, r, g, b, led_data);
    }
    
    // 更新显示
    refreshLEDs(1, led_data);
    refreshLEDs(2, led_data);
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