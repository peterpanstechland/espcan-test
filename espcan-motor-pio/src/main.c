/**
 * ESP32 + PWM 调速 + CAN 控制 + DC SSR 启停验证
 * 
 * 此程序实现:
 * 1. 使用 ESP32 的 LEDC 模块输出 PWM 信号控制电机速度
 * 2. 使用 CAN 接收控制命令(占空比和启停)
 * 3. 使用 GPIO 控制 SSR 实现电机启停
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/twai.h"

// 日志标签
static const char *TAG = "espcan-motor";

// PWM 配置
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          CONFIG_PWM_GPIO       // PWM输出GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_8_BIT      // 8位占空比分辨率（0-255）
#define LEDC_FREQUENCY          CONFIG_PWM_FREQUENCY  // PWM频率
#define LEDC_DUTY_MAX           255                   // 最大占空比值

// SSR 控制
#define SSR_GPIO                CONFIG_SSR_GPIO       // SSR控制GPIO
#define SSR_ON                  1                     // SSR开启
#define SSR_OFF                 0                     // SSR关闭

// CAN 引脚配置
#define CAN_TX_GPIO             CONFIG_CAN_TX_GPIO    // CAN TX引脚
#define CAN_RX_GPIO             CONFIG_CAN_RX_GPIO    // CAN RX引脚
#define CAN_CONTROL_ID          CONFIG_CAN_CONTROL_ID // 控制命令CAN ID

// CAN 命令结构
#define CMD_PWM_INDEX           0                     // 占空比值在Data[0]
#define CMD_ONOFF_INDEX         1                     // 启停命令在Data[1]

// 电机状态
static struct {
    uint8_t duty;                              // 当前占空比(0-255)
    uint8_t is_running;                        // 当前运行状态
} motor_state = {
    .duty = 0,
    .is_running = 0
};

// 初始化 LEDC 模块用于 PWM 输出
static void pwm_init(void)
{
    // 准备LEDC定时器配置
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    
    // 设置LEDC定时器
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 准备LEDC通道配置
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // 初始占空比为0
        .hpoint         = 0
    };
    
    // 配置LEDC通道
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    
    ESP_LOGI(TAG, "PWM初始化完成，GPIO: %d, 频率: %dHz, 分辨率: 8位", 
             LEDC_OUTPUT_IO, LEDC_FREQUENCY);
}

// 设置PWM占空比
static void set_pwm_duty(uint8_t duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
    motor_state.duty = duty;
    ESP_LOGI(TAG, "PWM占空比设置为: %d", duty);
}

// 初始化 SSR 控制 GPIO
static void ssr_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << SSR_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // 默认关闭SSR
    gpio_set_level(SSR_GPIO, SSR_OFF);
    ESP_LOGI(TAG, "SSR控制初始化完成，GPIO: %d, 默认状态: 关闭", SSR_GPIO);
}

// 控制SSR开关
static void set_ssr_state(uint8_t state)
{
    gpio_set_level(SSR_GPIO, state ? SSR_ON : SSR_OFF);
    motor_state.is_running = state;
    ESP_LOGI(TAG, "SSR状态设置为: %s", state ? "开启" : "关闭");
}

// 选择适当的CAN总线时序配置
static twai_timing_config_t get_can_timing_config(int bitrate_kbps)
{
    twai_timing_config_t t_config;
    
    // 默认初始化
    t_config.clk_src = TWAI_CLK_SRC_DEFAULT;
    t_config.triple_sampling = false;
    
    switch (bitrate_kbps) {
        case 100:
            t_config.quanta_resolution_hz = 2000000;
            t_config.brp = 0;
            t_config.tseg_1 = 15;
            t_config.tseg_2 = 4;
            t_config.sjw = 3;
            break;
        case 125:
            t_config.quanta_resolution_hz = 2500000;
            t_config.brp = 0;
            t_config.tseg_1 = 15;
            t_config.tseg_2 = 4;
            t_config.sjw = 3;
            break;
        case 250:
            t_config.quanta_resolution_hz = 5000000;
            t_config.brp = 0;
            t_config.tseg_1 = 15;
            t_config.tseg_2 = 4;
            t_config.sjw = 3;
            break;
        case 800:
            t_config.quanta_resolution_hz = 20000000;
            t_config.brp = 0;
            t_config.tseg_1 = 16;
            t_config.tseg_2 = 8;
            t_config.sjw = 3;
            break;
        case 1000:
            t_config.quanta_resolution_hz = 20000000;
            t_config.brp = 0;
            t_config.tseg_1 = 15;
            t_config.tseg_2 = 4;
            t_config.sjw = 3;
            break;
        case 500:
        default:
            t_config.quanta_resolution_hz = 10000000;
            t_config.brp = 0;
            t_config.tseg_1 = 15;
            t_config.tseg_2 = 4;
            t_config.sjw = 3;
            break;
    }
    
    return t_config;
}

// 初始化 CAN 控制器
static void can_init(void)
{
    // 常规配置
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);
    
    // 设置CAN总线速率
    twai_timing_config_t t_config = get_can_timing_config(CONFIG_CAN_BITRATE);
    
    // 过滤器配置 - 只接收特定ID的消息
    twai_filter_config_t f_config = {
        .acceptance_code = (CAN_CONTROL_ID << 21),
        .acceptance_mask = ~(0x7FF << 21),  // 只匹配11位ID
        .single_filter = true
    };
    
    // 安装TWAI驱动
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    
    // 启动TWAI驱动
    ESP_ERROR_CHECK(twai_start());
    
    ESP_LOGI(TAG, "CAN控制器初始化完成，TX: %d, RX: %d, 速率: %dkbps, 监听ID: 0x%lx", 
             CAN_TX_GPIO, CAN_RX_GPIO, CONFIG_CAN_BITRATE, (unsigned long)CAN_CONTROL_ID);
}

// 处理收到的CAN控制命令
static void process_can_command(twai_message_t *message)
{
    // 检查消息长度
    if (message->data_length_code < 2) {
        ESP_LOGW(TAG, "收到无效CAN命令 (数据长度不足)");
        return;
    }
    
    // 获取PWM占空比
    uint8_t pwm_duty = message->data[CMD_PWM_INDEX];
    
    // 获取启停命令
    uint8_t on_off = message->data[CMD_ONOFF_INDEX] ? 1 : 0;
    
    ESP_LOGI(TAG, "收到CAN控制命令 - 占空比: %d, 状态: %s", 
             pwm_duty, on_off ? "启动" : "停止");
    
    // 设置PWM占空比
    set_pwm_duty(pwm_duty);
    
    // 控制SSR状态
    set_ssr_state(on_off);
    
    // 发送状态确认消息
    twai_message_t tx_message;
    tx_message.identifier = CAN_CONTROL_ID;
    tx_message.extd = 0;      // 标准帧
    tx_message.rtr = 0;       // 非远程帧
    tx_message.ss = 1;        // 单次发送
    tx_message.self = 0;      // 不是自发自收
    tx_message.data_length_code = 3;
    tx_message.data[0] = pwm_duty;   // 当前占空比
    tx_message.data[1] = on_off;     // 当前状态
    tx_message.data[2] = 0x01;       // 确认标志
    
    twai_transmit(&tx_message, pdMS_TO_TICKS(100));
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 + PWM 调速 + CAN 控制 + DC SSR 启停系统启动...");
    
    // 初始化外设
    pwm_init();
    ssr_init();
    can_init();
    
    ESP_LOGI(TAG, "系统初始化完成，等待CAN控制命令...");
    
    // CAN消息接收缓冲区
    twai_message_t rx_message;
    
    // 主循环
    while (1) {
        // 接收CAN消息，最多等待100ms
        esp_err_t result = twai_receive(&rx_message, pdMS_TO_TICKS(100));
        
        if (result == ESP_OK) {
            // 检查消息ID
            if (rx_message.identifier == CAN_CONTROL_ID) {
                process_can_command(&rx_message);
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