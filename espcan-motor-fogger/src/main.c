/**
 * ESP32 + 电机控制 + 雾化器控制 + CAN 通信
 * 
 * 此程序实现:
 * 1. 使用 ESP32 的 LEDC 模块输出 PWM 信号控制电机速度
 * 2. 使用 GPIO 控制 SSR 实现电机启停
 * 3. 使用 GPIO 控制继电器实现雾化器启停
 * 4. 通过 CAN 总线接收控制命令
 * 5. 根据不同情绪触发不同设备：伤心触发雾化器，惊讶触发电机
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
static const char *TAG = "MOTOR-FOGGER";

// CAN 引脚配置
#define CAN_TX_PIN CONFIG_CAN_TX_GPIO
#define CAN_RX_PIN CONFIG_CAN_RX_GPIO

// PWM 配置 (电机控制)
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          CONFIG_PWM_GPIO       // PWM输出GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_8_BIT      // 8位占空比分辨率（0-255）
#define LEDC_FREQUENCY          CONFIG_PWM_FREQUENCY  // PWM频率
#define LEDC_DUTY_MAX           255                   // 最大占空比值

// SSR 控制 (电机启停)
#define SSR_GPIO                CONFIG_SSR_GPIO       // SSR控制GPIO
#define SSR_ON                  1                     // SSR开启
#define SSR_OFF                 0                     // SSR关闭

// 继电器控制 (雾化器)
#define RELAY_PIN CONFIG_FOGGER_RELAY_GPIO

// CAN 消息ID
#define MOTOR_CMD_ID CONFIG_CAN_MOTOR_ID
#define FOGGER_CMD_ID CONFIG_CAN_FOGGER_ID
#define EMOTION_CMD_ID 0x789      // 情绪状态命令ID

// 电机控制命令
#define CMD_PWM_INDEX           0                     // 占空比值在Data[0]
#define CMD_ONOFF_INDEX         1                     // 启停命令在Data[1]

// 雾化器控制命令
#define FOGGER_CMD_OFF 0
#define FOGGER_CMD_ON 1

// 情绪状态定义
#define EMOTION_NEUTRAL 0  // 中性
#define EMOTION_HAPPY 1    // 开心
#define EMOTION_SAD 2      // 伤心 - 触发雾化器
#define EMOTION_SURPRISE 3 // 惊讶 - 触发电机

// 电机渐变模式
#define MOTOR_MODE_FIXED        0                     // 固定速度模式
#define MOTOR_MODE_GRADUAL      1                     // 渐变速度模式

// 电机状态
static struct {
    uint8_t duty;                              // 当前占空比(0-255)
    uint8_t is_running;                        // 当前运行状态
    uint8_t mode;                              // 运行模式(0=固定,1=渐变)
    uint8_t target_duty;                       // 目标占空比
    int direction;                             // 渐变方向(1=增加,-1=减少)
    uint32_t gradual_timer;                    // 渐变计时器
} motor_state = {
    .duty = 0,
    .is_running = 0,
    .mode = MOTOR_MODE_FIXED,
    .target_duty = 0,
    .direction = 1,
    .gradual_timer = 0
};

// 雾化器状态
static struct {
    uint8_t is_on;             // 当前状态，0=关闭，1=开启
    uint32_t last_cmd_time;    // 最后一次接收命令的时间(ms)
} fogger_state = {
    .is_on = 0,
    .last_cmd_time = 0
};

// 电机渐变任务句柄
TaskHandle_t gradual_task_handle = NULL;

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

// 初始化 CAN 控制器
static void can_init(void)
{
    // 常规配置
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    g_config.tx_queue_len = 10;
    g_config.rx_queue_len = 10;
    
    // 波特率配置 (500Kbps)
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    
    // 过滤器配置 - 接收电机、雾化器和情绪状态命令
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    
    // 安装TWAI驱动
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    
    // 启动TWAI驱动
    ESP_ERROR_CHECK(twai_start());
    
    ESP_LOGI(TAG, "CAN控制器初始化完成，TX: %d, RX: %d, 速率: 500kbps", 
             CAN_TX_PIN, CAN_RX_PIN);
}

// 电机渐变速度任务
void gradual_speed_task(void *pvParameters)
{
    uint8_t current_duty = 0;
    int step = 0;
    
    while (1) {
        // 检查是否处于渐变模式且电机运行中
        if (motor_state.mode == MOTOR_MODE_GRADUAL && motor_state.is_running) {
            // 获取当前占空比
            current_duty = motor_state.duty;
            
            // 计算步进值 (根据当前占空比动态调整步进值)
            if (current_duty < 50) {
                // 低速区域 - 小步进
                step = 1;
            } else if (current_duty < 150) {
                // 中速区域 - 中步进
                step = 2;
            } else {
                // 高速区域 - 大步进
                step = 3;
            }
            
            // 根据方向调整占空比
            if (motor_state.direction > 0) {
                // 增加方向
                if (current_duty < motor_state.target_duty) {
                    current_duty += step;
                    if (current_duty > motor_state.target_duty) {
                        current_duty = motor_state.target_duty;
                        motor_state.direction = -1;  // 改变方向
                    }
                    set_pwm_duty(current_duty);
                } else {
                    motor_state.direction = -1;  // 改变方向
                }
            } else {
                // 减少方向
                if (current_duty > 0) {
                    if (current_duty > step) {
                        current_duty -= step;
                    } else {
                        current_duty = 0;
                        motor_state.direction = 1;  // 改变方向
                    }
                    set_pwm_duty(current_duty);
                } else {
                    motor_state.direction = 1;  // 改变方向
                }
            }
            
            motor_state.gradual_timer++;
            
            // 延时，根据速度区域调整延时时间
            if (current_duty < 50) {
                vTaskDelay(pdMS_TO_TICKS(80));  // 低速区域 - 慢变化
            } else if (current_duty < 150) {
                vTaskDelay(pdMS_TO_TICKS(50));  // 中速区域 - 中等变化
            } else {
                vTaskDelay(pdMS_TO_TICKS(30));  // 高速区域 - 快变化
            }
        } else {
            // 非渐变模式或电机停止 - 降低CPU使用
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// 处理收到的电机控制命令
static void process_motor_command(twai_message_t *message)
{
    // 检查消息长度
    if (message->data_length_code < 2) {
        ESP_LOGW(TAG, "收到无效电机控制命令 (数据长度不足)");
        return;
    }
    
    // 获取PWM占空比
    uint8_t pwm_duty = message->data[CMD_PWM_INDEX];
    
    // 获取启停命令
    uint8_t on_off = message->data[CMD_ONOFF_INDEX] ? 1 : 0;
    
    // 获取额外参数 - 模式
    uint8_t mode = MOTOR_MODE_FIXED;  // 默认固定模式
    if (message->data_length_code >= 3) {
        mode = message->data[2] ? MOTOR_MODE_GRADUAL : MOTOR_MODE_FIXED;
    }
    
    ESP_LOGI(TAG, "收到电机控制命令 - 占空比: %d, 状态: %s, 模式: %s", 
             pwm_duty, on_off ? "启动" : "停止", 
             mode ? "渐变" : "固定");
    
    // 设置运行模式
    motor_state.mode = mode;
    
    if (mode == MOTOR_MODE_GRADUAL) {
        // 渐变模式 - 设置目标占空比
        motor_state.target_duty = pwm_duty;
        
        // 如果当前占空比为0，从低速开始
        if (motor_state.duty == 0) {
            motor_state.direction = 1;  // 增加方向
            set_pwm_duty(10);  // 从低速开始
        }
    } else {
        // 固定模式 - 直接设置PWM占空比
        set_pwm_duty(pwm_duty);
    }
    
    // 控制SSR状态
    set_ssr_state(on_off);
    
    // 发送状态确认消息
    twai_message_t tx_message;
    tx_message.identifier = MOTOR_CMD_ID;
    tx_message.extd = 0;      // 标准帧
    tx_message.rtr = 0;       // 非远程帧
    tx_message.ss = 1;        // 单次发送
    tx_message.self = 0;      // 不是自发自收
    tx_message.data_length_code = 4;
    tx_message.data[0] = motor_state.duty;   // 当前占空比
    tx_message.data[1] = on_off;             // 当前状态
    tx_message.data[2] = mode;               // 当前模式
    tx_message.data[3] = 0x01;               // 确认标志
    
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

// 处理情绪状态命令
void process_emotion_command(twai_message_t *message) {
    if (message->data_length_code < 1) {
        ESP_LOGW(TAG, "收到无效情绪状态命令 (数据长度不足)");
        return;
    }
    
    uint8_t emotion = message->data[0];
    ESP_LOGI(TAG, "收到情绪状态命令: %d", emotion);
    
    // 根据情绪状态触发不同设备
    switch (emotion) {
        case EMOTION_SAD:  // 伤心 - 触发雾化器
            ESP_LOGI(TAG, "检测到伤心情绪，激活雾化器");
            set_fogger_state(1);  // 开启雾化器
            break;
            
        case EMOTION_SURPRISE:  // 惊讶 - 触发电机
            ESP_LOGI(TAG, "检测到惊讶情绪，激活电机");
            // 电机使用渐变模式，中等速度
            motor_state.mode = MOTOR_MODE_GRADUAL;
            motor_state.target_duty = 180;  // 中高速
            motor_state.direction = 1;      // 增加方向
            set_pwm_duty(30);              // 从低速开始
            set_ssr_state(1);              // 开启SSR
            break;
            
        default:
            // 其他情绪状态不触发任何设备
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 电机和雾化器控制系统初始化中...");
    
    // 初始化外设
    pwm_init();
    ssr_init();
    relay_init();
    can_init();
    
    // 创建渐变速度任务
    xTaskCreate(gradual_speed_task, "gradual_speed", 2048, NULL, 5, &gradual_task_handle);
    
    ESP_LOGI(TAG, "系统初始化完成，等待CAN控制命令...");
    ESP_LOGI(TAG, "电机控制ID: 0x%lX, 雾化器控制ID: 0x%lX, 情绪状态ID: 0x%lX", 
             (unsigned long)MOTOR_CMD_ID, (unsigned long)FOGGER_CMD_ID, (unsigned long)EMOTION_CMD_ID);
    
    // CAN消息接收缓冲区
    twai_message_t rx_message;
    
    // 主循环
    while (1) {
        // 接收CAN消息，最多等待100ms
        esp_err_t result = twai_receive(&rx_message, pdMS_TO_TICKS(100));
        
        if (result == ESP_OK) {
            // 根据消息ID分发处理
            if (rx_message.identifier == MOTOR_CMD_ID) {
                process_motor_command(&rx_message);
            } else if (rx_message.identifier == FOGGER_CMD_ID) {
                process_fogger_command(&rx_message);
            } else if (rx_message.identifier == EMOTION_CMD_ID) {
                process_emotion_command(&rx_message);
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
