/*
 * ESPCAN-12V-SK6812
 * 整合CAN通信和12V SK6812灯光控制
 * 基于espcan-light的CAN通信功能和espcan-light-12V-sk6812grbw的灯光控制
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "driver/rmt_tx.h"
#include "sdkconfig.h"

// 定义引脚和参数
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4
#define LED_PIN GPIO_NUM_2
#define WS2812_PIN_1 GPIO_NUM_18
#define WS2812_PIN_2 GPIO_NUM_17
#define WS2812_LEDS_PER_STRIP 900
#define WS2812_LEDS_TOTAL (WS2812_LEDS_PER_STRIP * 2)  // 总共1800个灯
#define RMT_RESOLUTION_HZ 10000000

// SK6812时序定义 (与WS2812B时序类似)
#define T0H 3
#define T0L 9 
#define T1H 6
#define T1L 6
#define TRS 800

// CAN消息ID
#define LED_CMD_ID 0x456
#define EMOTION_CMD_ID 0x789
#define RANDOM_CMD_ID 0xABC

// 情绪状态命令
#define EMOTION_HAPPY 1      // 开心 - 彩虹效果
#define EMOTION_SAD 2        // 伤心 - 紫色追逐效果  
#define EMOTION_SURPRISE 3   // 惊讶 - 闪电效果
#define EMOTION_NEUTRAL 4    // 中性 - 呼吸灯切换颜色效果
#define EMOTION_RANDOM 4     // 兼容性别名

const char *TAG = "ESPCAN_SK6812";
static uint8_t current_emotion = 0;

// RMT相关句柄
rmt_channel_handle_t rmt_channel_1 = NULL;
rmt_channel_handle_t rmt_channel_2 = NULL;
rmt_encoder_handle_t led_encoder_1 = NULL;
rmt_encoder_handle_t led_encoder_2 = NULL;

// TWAI配置
static const twai_general_config_t g_config = {
    .mode = TWAI_MODE_NORMAL,
    .tx_io = CAN_TX_PIN,
    .rx_io = CAN_RX_PIN,
    .clkout_io = TWAI_IO_UNUSED,
    .bus_off_io = TWAI_IO_UNUSED,
    .tx_queue_len = 5,
    .rx_queue_len = 5,
    .alerts_enabled = TWAI_ALERT_NONE,
    .clkout_divider = 0,
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
};

static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

// 函数声明
extern void byteToRMTSymbols(uint8_t byte_val, rmt_symbol_word_t *symbols);
extern void sendPixels(rmt_symbol_word_t *pixel_data, size_t data_size);
extern void setPixelRGB(int index, uint8_t r, uint8_t g, uint8_t b, rmt_symbol_word_t *led_data);
extern void refreshLEDs(rmt_symbol_word_t *led_data);
extern void setAllLEDs(uint8_t r, uint8_t g, uint8_t b);
extern void clearAllLEDs(void);

// 动画效果函数声明
extern void rainbow_effect(int delay_ms);
extern void purple_chase_effect(int delay_ms);
extern void blue_lightning_effect(int delay_ms);
extern void breathing_light_effect(int delay_ms);

// 动画任务函数声明
void emotion_animation_task(void *pvParameters);

/*
 * @brief 初始化RMT通道
 */
static bool initRMT(void)
{
    ESP_LOGI(TAG, "初始化RMT通道1，GPIO: %d", WS2812_PIN_1);
    
    // 创建RMT TX通道1
    rmt_tx_channel_config_t tx_config_1 = {
        .gpio_num = WS2812_PIN_1,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags.invert_out = false,
        .flags.with_dma = false,
    };

    esp_err_t ret = rmt_new_tx_channel(&tx_config_1, &rmt_channel_1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建RMT TX通道1失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 创建简单的复制编码器1
    rmt_copy_encoder_config_t copy_encoder_config_1 = {};
    ret = rmt_new_copy_encoder(&copy_encoder_config_1, &led_encoder_1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建复制编码器1失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 启用RMT通道1
    ret = rmt_enable(rmt_channel_1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用RMT通道1失败: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "RMT通道1初始化成功");

    // 初始化第二个RMT通道
    ESP_LOGI(TAG, "初始化RMT通道2，GPIO: %d", WS2812_PIN_2);
    
    // 创建RMT TX通道2
    rmt_tx_channel_config_t tx_config_2 = {
        .gpio_num = WS2812_PIN_2,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags.invert_out = false,
        .flags.with_dma = false,
    };

    ret = rmt_new_tx_channel(&tx_config_2, &rmt_channel_2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建RMT TX通道2失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 创建简单的复制编码器2
    rmt_copy_encoder_config_t copy_encoder_config_2 = {};
    ret = rmt_new_copy_encoder(&copy_encoder_config_2, &led_encoder_2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建复制编码器2失败: %s", esp_err_to_name(ret));
        return false;
    }

    // 启用RMT通道2
    ret = rmt_enable(rmt_channel_2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用RMT通道2失败: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "RMT通道2初始化成功");
    return true;
}

// 处理LED控制命令
void handle_led_command(twai_message_t *message) {
    if (message->data_length_code < 1) {
        ESP_LOGE(TAG, "LED命令数据长度不足");
        return;
    }
    
    uint8_t led_state = message->data[0];
    gpio_set_level(LED_PIN, led_state);
    ESP_LOGI(TAG, "LED状态已设置为: %s", led_state ? "开启" : "关闭");
}

// 处理情绪状态命令
void handle_emotion_command(twai_message_t *message) {
    if (message->data_length_code < 1) {
        ESP_LOGE(TAG, "情绪状态命令数据长度不足");
        return;
    }
    
    uint8_t emotion_state = message->data[0];
    current_emotion = emotion_state;
    
    switch (emotion_state) {
        case EMOTION_HAPPY:
            ESP_LOGI(TAG, "情绪状态设置为: 开心 (彩虹效果)");
            break;
            
        case EMOTION_SAD:
            ESP_LOGI(TAG, "情绪状态设置为: 伤心 (紫色追逐效果)");
            break;
            
        case EMOTION_SURPRISE:
            ESP_LOGI(TAG, "情绪状态设置为: 惊讶 (闪电效果)");
            break;
            
        case EMOTION_NEUTRAL:
            ESP_LOGI(TAG, "情绪状态设置为: 中性 (呼吸灯切换颜色)");
            break;
            
        default:
            ESP_LOGI(TAG, "情绪状态设置为: 关闭");
            break;
    }
}

// 情绪灯光动画任务
void emotion_animation_task(void *pvParameters) {
    while (1) {
        // 根据当前情绪状态设置灯光效果
        switch (current_emotion) {
            case EMOTION_HAPPY:
                // 开心 - 彩虹效果
                rainbow_effect(50);
                break;
                
            case EMOTION_SAD:
                // 伤心 - 紫色追逐效果
                purple_chase_effect(30);
                break;
                
            case EMOTION_SURPRISE:
                // 惊讶 - 闪电效果
                blue_lightning_effect(80);
                break;
                
            case EMOTION_NEUTRAL:
                // 中性状态 - 呼吸灯切换颜色
                breathing_light_effect(30);
                break;
                
            default:
                // 默认状态 - 关闭灯
                clearAllLEDs();
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESPCAN-12V-SK6812 启动");
    ESP_LOGI(TAG, "SK6812 LED总数量: %d (两组各%d)", WS2812_LEDS_TOTAL, WS2812_LEDS_PER_STRIP);
    
    // 配置LED引脚
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);
    
    // 初始化RMT
    if (!initRMT()) {
        ESP_LOGE(TAG, "RMT初始化失败");
        return;
    }
    
    // 测试LED - 显示彩色测试5秒
    ESP_LOGI(TAG, "显示彩色测试 - 5秒");
    setAllLEDs(255, 0, 0);  // 红色
    vTaskDelay(pdMS_TO_TICKS(1000));
    setAllLEDs(0, 255, 0);  // 绿色
    vTaskDelay(pdMS_TO_TICKS(1000));
    setAllLEDs(0, 0, 255);  // 蓝色
    vTaskDelay(pdMS_TO_TICKS(1000));
    setAllLEDs(255, 255, 255);  // 白色 (全亮)
    vTaskDelay(pdMS_TO_TICKS(1000));
    clearAllLEDs();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 安装TWAI驱动
    ESP_LOGI(TAG, "CAN接收端初始化中...");
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI(TAG, "TWAI驱动安装成功");

    // 启动TWAI驱动
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "TWAI驱动启动成功");
    ESP_LOGI(TAG, "CAN接收端初始化完成，等待接收数据...");
    
    // 创建情绪动画任务
    xTaskCreate(emotion_animation_task, "emotion_animation", 4096, NULL, 5, NULL);
    
    // 接收CAN消息变量
    twai_message_t rx_message;
    
    while (1) {
        // 接收CAN消息，最多等待100ms
        esp_err_t result = twai_receive(&rx_message, pdMS_TO_TICKS(100));
        
        if (result == ESP_OK) {
            // 根据消息ID分发处理
            if (rx_message.identifier == LED_CMD_ID) {
                handle_led_command(&rx_message);
            } else if (rx_message.identifier == EMOTION_CMD_ID) {
                handle_emotion_command(&rx_message);
            } else if (rx_message.identifier == RANDOM_CMD_ID) {
                // 处理随机效果命令
                ESP_LOGI(TAG, "收到随机效果命令");
            } else {
                ESP_LOGW(TAG, "收到未知ID消息: 0x%lx", (unsigned long)rx_message.identifier);
            }
        } else if (result != ESP_ERR_TIMEOUT) {
            // 忽略超时错误
            ESP_LOGE(TAG, "CAN接收错误: %s", esp_err_to_name(result));
        }
        
        // 短暂延时
        vTaskDelay(pdMS_TO_TICKS(10));
    }
} 