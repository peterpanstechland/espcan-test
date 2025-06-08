/*
 * SK6812 GRBW 控制示例 - 基于用户原始代码风格，适配新 RMT API
 * 这个示例展示了如何手动构建 RMT 数据来控制 SK6812 GRBW 灯带
 */

// Std. C
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//ESP Specific
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define RMT_TX_GPIO LED_STRIP_RMT_GPIO  // 使用配置的 GPIO

/*
 * RMT 时钟配置
 * 设置 10MHz 时钟 (0.1us 分辨率)
 */
#define RMT_RESOLUTION_HZ 10000000  // 10MHz

#define striplen 1  // LED 数量

/*******************
 *SK6812 时序定义 (在 10MHz 时钟下的 tick 数)
********************/
#define T0H 3   // T0H for SK6812 -> 0.3 us (3 ticks)
#define T0L 9   // T0L for SK6812 -> 0.9 us (9 ticks) 
#define T1H 6   // T1H for SK6812 -> 0.6 us (6 ticks)
#define T1L 6   // T1L for SK6812 -> 0.6 us (6 ticks)
#define TRS 800 // TRES for SK6812 -> 80 us (800 ticks)

static const char *TAG = "sk6812_legacy";

// RMT 通道句柄
rmt_channel_handle_t rmt_channel = NULL;
rmt_encoder_handle_t led_encoder = NULL;

// 手动构建的像素数据 - 全白色 (GRBW = 255,255,255,255)
rmt_symbol_word_t pixels[] = {
    // 8 Bit G (绿色) - 全 1 (255)
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L}, // bit 7
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L}, // bit 6
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L}, // bit 5
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L}, // bit 4
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L}, // bit 3
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L}, // bit 2
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L}, // bit 1
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L}, // bit 0

    // 8 Bit R (红色) - 全 1 (255)
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},

    // 8 Bit B (蓝色) - 全 1 (255)
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},

    // 8 Bit W (白色) - 全 1 (255)
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},

    // Reset 信号 - 长时间低电平
    {.level0 = 0, .duration0 = TRS, .level1 = 0, .duration1 = 0}
};

// 不同颜色的预定义数据
rmt_symbol_word_t red_pixel[] = {
    // G = 0
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    
    // R = 255
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    {.level0 = 1, .duration0 = T1H, .level1 = 0, .duration1 = T1L},
    
    // B = 0
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    
    // W = 0
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    {.level0 = 1, .duration0 = T0H, .level1 = 0, .duration1 = T0L},
    
    // Reset
    {.level0 = 0, .duration0 = TRS, .level1 = 0, .duration1 = 0}
};

/*
 * @brief 初始化 RMT 通道 (使用新 API)
 */
static bool initRMT(void)
{
    ESP_LOGI(TAG, "初始化 RMT 通道，GPIO: %d", RMT_TX_GPIO);
    
    // 创建 RMT TX 通道
    rmt_tx_channel_config_t tx_config = {
        .gpio_num = RMT_TX_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags.invert_out = false,
        .flags.with_dma = false,
    };

    esp_err_t ret = rmt_new_tx_channel(&tx_config, &rmt_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 RMT TX 通道失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 创建简单的复制编码器
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ret = rmt_new_copy_encoder(&copy_encoder_config, &led_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建复制编码器失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 启用 RMT 通道
    ret = rmt_enable(rmt_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用 RMT 通道失败: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "RMT 初始化成功");
    return true;
}

/*
 * @brief 发送像素数据
 */
static void sendPixels(rmt_symbol_word_t *pixel_data, size_t data_size)
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
    ret = rmt_tx_wait_all_done(rmt_channel, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "等待传输完成超时: %s", esp_err_to_name(ret));
    }
}

/*
 * @brief 将字节值转换为 RMT 符号 (8个位)
 */
static void byteToRMTSymbols(uint8_t byte_val, rmt_symbol_word_t *symbols)
{
    for (int i = 7; i >= 0; i--) {  // MSB 优先
        if (byte_val & (1 << i)) {
            // 发送 '1' 位
            symbols[7-i].level0 = 1;
            symbols[7-i].duration0 = T1H;
            symbols[7-i].level1 = 0;
            symbols[7-i].duration1 = T1L;
        } else {
            // 发送 '0' 位  
            symbols[7-i].level0 = 1;
            symbols[7-i].duration0 = T0H;
            symbols[7-i].level1 = 0;
            symbols[7-i].duration1 = T0L;
        }
    }
}

/*
 * @brief 动态构建 GRBW 像素数据
 */
static void buildGRBWPixel(uint8_t g, uint8_t r, uint8_t b, uint8_t w, rmt_symbol_word_t *pixel_data)
{
    // 构建 G、R、B、W 各 8 位，总共 32 位 + 1 个复位位 = 33 个符号
    byteToRMTSymbols(g, &pixel_data[0]);   // 绿色
    byteToRMTSymbols(r, &pixel_data[8]);   // 红色  
    byteToRMTSymbols(b, &pixel_data[16]);  // 蓝色
    byteToRMTSymbols(w, &pixel_data[24]);  // 白色
    
    // 添加复位信号
    pixel_data[32].level0 = 0;
    pixel_data[32].duration0 = TRS;
    pixel_data[32].level1 = 0;
    pixel_data[32].duration1 = 0;
}

void app_main(void)
{
    ESP_LOGI(TAG, "SK6812 GRBW 演示程序启动 (传统风格)");
    
    if (!initRMT()) {
        ESP_LOGE(TAG, "RMT 初始化失败");
        return;
    }

    rmt_symbol_word_t dynamic_pixel[33];  // 32位数据 + 1个复位

    int color_step = 0;
    
    while (1) {
        switch (color_step) {
            case 0:
                ESP_LOGI(TAG, "发送预定义的白色像素");
                sendPixels(pixels, sizeof(pixels) / sizeof(pixels[0]));
                break;
                
            case 1:
                ESP_LOGI(TAG, "发送预定义的红色像素");
                sendPixels(red_pixel, sizeof(red_pixel) / sizeof(red_pixel[0]));
                break;
                
            case 2:
                ESP_LOGI(TAG, "发送动态构建的绿色像素 (G=100)");
                buildGRBWPixel(100, 0, 0, 0, dynamic_pixel);
                sendPixels(dynamic_pixel, 33);
                break;
                
            case 3:
                ESP_LOGI(TAG, "发送动态构建的蓝色像素 (B=150)");
                buildGRBWPixel(0, 0, 150, 0, dynamic_pixel);
                sendPixels(dynamic_pixel, 33);
                break;
                
            case 4:
                ESP_LOGI(TAG, "发送动态构建的纯白色像素 (W=200)");
                buildGRBWPixel(0, 0, 0, 200, dynamic_pixel);
                sendPixels(dynamic_pixel, 33);
                break;
                
            case 5:
                ESP_LOGI(TAG, "发送动态构建的混合色 (G=50, R=80, B=30, W=100)");
                buildGRBWPixel(50, 80, 30, 100, dynamic_pixel);
                sendPixels(dynamic_pixel, 33);
                break;
                
            default:
                color_step = -1;  // 重置到开始
                break;
        }
        
        color_step++;
        vTaskDelay(2000 / portTICK_PERIOD_MS);  // 2秒延时
    }
} 