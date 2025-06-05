#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/twai.h"

// 定义CAN引脚
#define CAN_TX_PIN CONFIG_CAN_TX_GPIO
#define CAN_RX_PIN CONFIG_CAN_RX_GPIO

// 定义声音控制引脚
#define THUNDER_SOUND_PIN CONFIG_THUNDER_SOUND_GPIO  // GPIO23 - 打雷闪电音效（惊讶）
#define RAIN_SOUND_PIN CONFIG_RAIN_SOUND_GPIO      // GPIO22 - 小雨点音效（伤心）

// 消息ID
#define EMOTION_CMD_ID CONFIG_CAN_EMOTION_ID  // 情绪状态命令ID

// 情绪状态命令
#define EMOTION_HAPPY 1    // 开心
#define EMOTION_SAD 2      // 伤心 - 小雨点
#define EMOTION_SURPRISE 3 // 惊讶 - 打雷闪电
#define EMOTION_RANDOM 4   // 随机效果

// 日志标签
static const char *TAG = "SOUND_CTRL";

// 当前情绪状态
static uint8_t current_emotion = 0;

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

// 波特率配置 (500Kbps)
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

// 过滤器配置 (接收情绪状态命令)
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

// 初始化声音控制GPIO
void sound_gpio_init(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << THUNDER_SOUND_PIN) | (1ULL << RAIN_SOUND_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // 默认关闭所有声音
    gpio_set_level(THUNDER_SOUND_PIN, 1); // 高电平无效（继电器常闭）
    gpio_set_level(RAIN_SOUND_PIN, 1);    // 高电平无效（继电器常闭）
    
    ESP_LOGI(TAG, "声音控制GPIO初始化完成");
    ESP_LOGI(TAG, "打雷闪电音效引脚: %d", THUNDER_SOUND_PIN);
    ESP_LOGI(TAG, "小雨点音效引脚: %d", RAIN_SOUND_PIN);
}

// 控制声音
void control_sounds(uint8_t emotion) {
    // 首先关闭所有声音
    gpio_set_level(THUNDER_SOUND_PIN, 1); // 高电平无效
    gpio_set_level(RAIN_SOUND_PIN, 1);    // 高电平无效
    
    // 根据情绪状态控制声音
    switch (emotion) {
        case EMOTION_SURPRISE:
            // 惊讶 - 打雷闪电音效
            ESP_LOGI(TAG, "触发打雷闪电音效");
            gpio_set_level(THUNDER_SOUND_PIN, 0); // 低电平有效
            break;
            
        case EMOTION_SAD:
            // 伤心 - 小雨点音效
            ESP_LOGI(TAG, "触发小雨点音效");
            gpio_set_level(RAIN_SOUND_PIN, 0); // 低电平有效
            break;
            
        default:
            // 其他情绪 - 不播放特殊音效
            break;
    }
}

// 处理情绪状态命令
void handle_emotion_command(twai_message_t *message) {
    if (message->data_length_code < 1) {
        ESP_LOGE(TAG, "情绪状态命令数据长度不足");
        return;
    }
    
    uint8_t emotion_state = message->data[0];
    
    // 更新当前情绪状态
    current_emotion = emotion_state;
    
    // 根据情绪状态输出日志
    switch (emotion_state) {
        case EMOTION_HAPPY:
            ESP_LOGI(TAG, "情绪状态设置为: 开心");
            break;
            
        case EMOTION_SAD:
            ESP_LOGI(TAG, "情绪状态设置为: 伤心 (小雨点音效)");
            break;
            
        case EMOTION_SURPRISE:
            ESP_LOGI(TAG, "情绪状态设置为: 惊讶 (打雷闪电音效)");
            break;
            
        case EMOTION_RANDOM:
            ESP_LOGI(TAG, "情绪状态设置为: 随机效果");
            break;
            
        default:
            ESP_LOGI(TAG, "情绪状态设置为: 未知");
            break;
    }
    
    // 控制声音
    control_sounds(emotion_state);
}

void app_main(void)
{
    // 安装TWAI驱动
    ESP_LOGI(TAG, "声音控制器初始化中...");
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI(TAG, "TWAI驱动安装成功");

    // 启动TWAI驱动
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "TWAI驱动启动成功");
    
    // 初始化声音控制GPIO
    sound_gpio_init();
    
    ESP_LOGI(TAG, "声音控制器初始化完成，等待情绪状态命令...");
    ESP_LOGI(TAG, "情绪状态命令ID: 0x%lX", (unsigned long)EMOTION_CMD_ID);
    
    // 接收CAN消息变量
    twai_message_t rx_message;
    
    while (1) {
        // 接收CAN消息
        esp_err_t result = twai_receive(&rx_message, pdMS_TO_TICKS(1000));
        
        if (result == ESP_OK) {
            // 打印帧信息
            ESP_LOGI(TAG, "接收到CAN帧 - ID: 0x%lX", (unsigned long)rx_message.identifier);
            
            // 检查消息类型
            if (rx_message.identifier == EMOTION_CMD_ID) {
                handle_emotion_command(&rx_message);
            }
        }
        
        // 短暂延时
        vTaskDelay(pdMS_TO_TICKS(10));
    }
} 