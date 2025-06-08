/*
 * ESP32 SK6812 GRBW 控制示例 - Adafruit_NeoPixel 风格
 * 基于用户提供的 Adafruit 代码，适配到我们的 SK6812 组件
 * 支持串口命令控制多种效果
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "sk6812.h"
#include "esp_random.h"

// 硬件配置
#define LED_PIN     LED_STRIP_RMT_GPIO   // 控制引脚
#define NUM_LEDS    LED_STRIP_LED_NUM    // LED 数量  
#define BRIGHTNESS  64                   // 初始亮度 (0-255)

static const char *TAG = "neopixel_style";

// 效果参数
uint8_t current_effect = 0;       // 当前效果 (0-2)
uint16_t speed_delay = 50;        // 效果速度(ms)
uint32_t prev_millis = 0;

// SK6812 句柄
sk6812_handle_t led_strip;

// ================== Adafruit_NeoPixel 兼容层 ==================

typedef struct {
    sk6812_handle_t handle;
    uint16_t num_pixels;
    uint8_t brightness;
} NeoPixel_t;

static NeoPixel_t strip;

// 初始化"NeoPixel"
void strip_begin(void) {
    sk6812_config_t config = {
        .gpio_num = LED_PIN,
        .led_count = NUM_LEDS,
        .resolution_hz = 10 * 1000 * 1000,
    };
    
    ESP_ERROR_CHECK(sk6812_new(&config, &led_strip));
    ESP_ERROR_CHECK(sk6812_enable(led_strip));
    
    strip.handle = led_strip;
    strip.num_pixels = NUM_LEDS;
    strip.brightness = BRIGHTNESS;
    
    ESP_LOGI(TAG, "NeoPixel 风格灯带初始化完成，LED数量: %d", NUM_LEDS);
}

// 设置亮度 (模拟 setBrightness)
void strip_setBrightness(uint8_t brightness) {
    strip.brightness = brightness;
}

// 获取像素数量 (模拟 numPixels)
uint16_t strip_numPixels(void) {
    return strip.num_pixels;
}

// 创建颜色值 (模拟 Color)
uint32_t strip_Color(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    // 应用亮度调整
    r = (r * strip.brightness) / 255;
    g = (g * strip.brightness) / 255;
    b = (b * strip.brightness) / 255;
    w = (w * strip.brightness) / 255;
    
    return ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8) | w;
}

// HSV 转换为 RGB (简化版)
uint32_t strip_ColorHSV(uint16_t hue) {
    uint8_t r, g, b;
    uint8_t sector = hue >> 8;
    uint8_t offset = hue & 0xFF;
    
    switch(sector) {
        case 0: // Red -> Yellow
            r = 255;
            g = offset;
            b = 0;
            break;
        case 1: // Yellow -> Green  
            r = 255 - offset;
            g = 255;
            b = 0;
            break;
        case 2: // Green -> Cyan
            r = 0;
            g = 255;
            b = offset;
            break;
        case 3: // Cyan -> Blue
            r = 0;
            g = 255 - offset;
            b = 255;
            break;
        case 4: // Blue -> Magenta
            r = offset;
            g = 0;
            b = 255;
            break;
        default: // Magenta -> Red
            r = 255;
            g = 0;
            b = 255 - offset;
            break;
    }
    
    return strip_Color(r, g, b, 0);
}

// 设置单个像素颜色 (模拟 setPixelColor)
void strip_setPixelColor(uint16_t index, uint32_t color) {
    if (index >= strip.num_pixels) return;
    
    uint8_t w = color & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 24) & 0xFF;
    
    sk6812_set_pixel_grbw(strip.handle, index, g, r, b, w);
}

// 填充所有像素 (模拟 fill)
void strip_fill(uint32_t color) {
    for (int i = 0; i < strip.num_pixels; i++) {
        strip_setPixelColor(i, color);
    }
}

// 显示更新 (模拟 show)
void strip_show(void) {
    sk6812_refresh(strip.handle);
}

// ================== 串口处理 ==================

static void uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
}

static char uart_read_char(void) {
    uint8_t data;
    int len = uart_read_bytes(UART_NUM_0, &data, 1, 0);
    return (len > 0) ? (char)data : 0;
}

// ================== 效果实现 ==================

// 彩虹流水效果
void rainbowFlow(void) {
    static uint16_t hue = 0;
    
    for(int i = 0; i < strip_numPixels(); i++) {
        // 计算彩虹颜色（仅使用RGB通道）
        uint32_t color = strip_ColorHSV((hue + i * 65536L / strip_numPixels()) % 65536);
        // 转换RGB为RGBW（保持白色通道为0）
        strip_setPixelColor(i, color);
    }
    hue += 256; // 调整色调变化速度
}

// 闪烁效果
void blinkEffect(void) {
    static bool state = false;
    
    if(state) {
        // 随机RGBW颜色
        uint32_t color = strip_Color(esp_random() % 256, 
                                   esp_random() % 256, 
                                   esp_random() % 256, 
                                   esp_random() % 256);
        strip_fill(color);
    } else {
        strip_fill(0);
    }
    state = !state;
}

// 跑马灯效果  
void chaseEffect(void) {
    static int pos = 0;
    
    strip_fill(0); // 清除所有LED
    
    // 设置当前LED颜色（带白色通道）
    strip_setPixelColor(pos, strip_Color(0, 255, 0, 100)); // RGBW: 绿色+白色
    
    pos = (pos + 1) % strip_numPixels();
}

// 新增：呼吸灯效果
void breathingEffect(void) {
    static uint8_t brightness_val = 0;
    static int8_t direction = 1;
    
    // 所有LED设为暖白色
    uint32_t color = strip_Color(255, 255, 200, brightness_val);
    strip_fill(color);
    
    brightness_val += direction * 3;
    if (brightness_val >= 255 || brightness_val <= 0) {
        direction = -direction;
    }
}

// 新增：彩色追逐效果
void colorChaseEffect(void) {
    static int pos = 0;
    static uint8_t color_index = 0;
    
    // 定义几种颜色
    uint32_t colors[] = {
        strip_Color(255, 0, 0, 0),    // 红色
        strip_Color(0, 255, 0, 0),    // 绿色  
        strip_Color(0, 0, 255, 0),    // 蓝色
        strip_Color(0, 0, 0, 255),    // 白色
        strip_Color(255, 255, 0, 0),  // 黄色
        strip_Color(255, 0, 255, 0),  // 紫色
    };
    
    strip_fill(0);
    
    // 设置连续的几个LED
    for (int i = 0; i < 5; i++) {
        int led_pos = (pos + i) % strip_numPixels();
        strip_setPixelColor(led_pos, colors[color_index]);
    }
    
    pos = (pos + 1) % strip_numPixels();
    if (pos == 0) {
        color_index = (color_index + 1) % 6;
    }
}

// ================== 主程序 ==================

void app_main(void) {
    ESP_LOGI(TAG, "ESP32 SK6812 NeoPixel 风格控制程序启动");
    
    // 初始化
    uart_init();
    strip_begin();
    strip_setBrightness(BRIGHTNESS);
    strip_show(); // 初始化后关闭所有LED
    
    printf("\n=== ESP32 SK6812 GRBW 控制程序 ===\n");
    printf("输入命令控制效果:\n");
    printf("1: 彩虹流水效果\n");
    printf("2: 闪烁效果\n");  
    printf("3: 跑马灯效果\n");
    printf("4: 呼吸灯效果\n");
    printf("5: 彩色追逐效果\n");
    printf("+: 加速\n");
    printf("-: 减速\n");
    printf("当前速度: %d ms\n\n", speed_delay);
    
    while(1) {
        // 串口控制
        char cmd = uart_read_char();
        if (cmd != 0) {
            switch(cmd) {
                case '1': 
                    current_effect = 0; 
                    printf("彩虹流水效果\n"); 
                    break;
                case '2': 
                    current_effect = 1; 
                    printf("闪烁效果\n"); 
                    break;
                case '3': 
                    current_effect = 2; 
                    printf("跑马灯效果\n"); 
                    break;
                case '4': 
                    current_effect = 3; 
                    printf("呼吸灯效果\n"); 
                    break;
                case '5': 
                    current_effect = 4; 
                    printf("彩色追逐效果\n"); 
                    break;
                case '+': 
                    speed_delay = (speed_delay > 20) ? speed_delay - 10 : 20;
                    printf("加速，当前速度: %d ms\n", speed_delay);
                    break;
                case '-': 
                    speed_delay = (speed_delay < 500) ? speed_delay + 10 : 500;
                    printf("减速，当前速度: %d ms\n", speed_delay);
                    break;
                default:
                    if (cmd >= 32 && cmd <= 126) { // 可打印字符
                        printf("未知命令: %c\n", cmd);
                    }
                    break;
            }
        }
        
        // 定时执行效果
        uint32_t current_millis = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_millis - prev_millis >= speed_delay) {
            prev_millis = current_millis;
            
            switch(current_effect) {
                case 0: rainbowFlow(); break;
                case 1: blinkEffect(); break;
                case 2: chaseEffect(); break;
                case 3: breathingEffect(); break;
                case 4: colorChaseEffect(); break;
            }
            strip_show();
        }
        
        vTaskDelay(pdMS_TO_TICKS(1)); // 短暂延时避免占用过多CPU
    }
} 