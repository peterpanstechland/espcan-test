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

// 定义继电器控制引脚
#define RELAY_PIN CONFIG_FOGGER_RELAY_GPIO

// 消息ID
#define FOGGER_CMD_ID CONFIG_CAN_FOGGER_ID

// 雾化器控制命令
#define FOGGER_CMD_OFF 0
#define FOGGER_CMD_ON 1

// 日志标签
static const char *TAG = "FOGGER_CTRL";

// 雾化器状态
static struct {
    uint8_t is_on;             // 当前状态，0=关闭，1=开启
    uint32_t last_cmd_time;    // 最后一次接收命令的时间(ms)
} fogger_state = {
    .is_on = 0,
    .last_cmd_time = 0
};

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

// 过滤器配置 - 只接收雾化器控制消息
static const twai_filter_config_t f_config = {
    .acceptance_code = (FOGGER_CMD_ID << 21),
    .acceptance_mask = ~(0x7FF << 21),  // 只匹配11位ID
    .single_filter = true
};

// 初始化继电器
void relay_init(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // 默认关闭雾化器
    gpio_set_level(RELAY_PIN, 0);
    ESP_LOGI(TAG, "继电器初始化完成，GPIO: %d, 默认状态: 关闭", RELAY_PIN);
}

// 设置雾化器状态
void set_fogger_state(uint8_t state) {
    fogger_state.is_on = state;
    fogger_state.last_cmd_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // 控制继电器
    gpio_set_level(RELAY_PIN, state);
    
    ESP_LOGI(TAG, "雾化器状态设置为: %s", state ? "开启" : "关闭");
    
    // 发送状态确认消息
    twai_message_t tx_message;
    tx_message.identifier = FOGGER_CMD_ID;
    tx_message.extd = 0;      // 标准帧
    tx_message.rtr = 0;       // 非远程帧
    tx_message.ss = 1;        // 单次发送
    tx_message.self = 0;      // 不是自发自收
    tx_message.data_length_code = 2;
    tx_message.data[0] = state;   // 当前状态
    tx_message.data[1] = 0x01;    // 确认标志
    
    twai_transmit(&tx_message, pdMS_TO_TICKS(100));
}

// 处理接收到的雾化器控制命令
void process_fogger_command(twai_message_t *message) {
    if (message->data_length_code < 1) {
        ESP_LOGW(TAG, "收到无效雾化器命令 (数据长度不足)");
        return;
    }
    
    uint8_t fogger_cmd = message->data[0];
    ESP_LOGI(TAG, "收到雾化器控制命令: %s", fogger_cmd ? "开启" : "关闭");
    
    // 设置雾化器状态
    set_fogger_state(fogger_cmd);
}

void app_main(void)
{
    // 安装TWAI驱动
    ESP_LOGI(TAG, "雾化器控制器初始化中...");
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI(TAG, "TWAI驱动安装成功");

    // 启动TWAI驱动
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "TWAI驱动启动成功");
    
    // 初始化继电器
    relay_init();
    
    ESP_LOGI(TAG, "雾化器控制器初始化完成，等待CAN控制命令...");
    ESP_LOGI(TAG, "CAN ID: 0x%lX, 控制引脚: %d", (unsigned long)FOGGER_CMD_ID, RELAY_PIN);
    
    // 接收CAN消息变量
    twai_message_t rx_message;
    
    while (1) {
        // 接收CAN消息，最多等待100ms
        esp_err_t result = twai_receive(&rx_message, pdMS_TO_TICKS(100));
        
        if (result == ESP_OK) {
            // 检查消息ID是否为雾化器控制ID
            if (rx_message.identifier == FOGGER_CMD_ID) {
                process_fogger_command(&rx_message);
            }
        }
        
        // 短暂延时
        vTaskDelay(pdMS_TO_TICKS(10));
    }
} 