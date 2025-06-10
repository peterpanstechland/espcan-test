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
#include "driver/uart.h"

// 定义CAN引脚
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

// 定义木鱼传感器引脚
#define VIBRATION_SENSOR_PIN GPIO_NUM_22
#define BUZZER_SENSOR_PIN GPIO_NUM_23
#define WOODEN_FISH_DEBOUNCE_MS 50   // 消抖时间（毫秒）

// UART配置 - 用于接收TouchDesigner的控制命令
#define UART_NUM UART_NUM_0          // 使用UART0 (默认连接到USB)
#define UART_BAUD_RATE 115200        // 波特率
#define UART_BUF_SIZE 1024           // 缓冲区大小
#define UART_RX_TIMEOUT_MS 10        // 接收超时时间(毫秒)

// 消息ID
#define LED_CMD_ID 0x456          // LED控制命令ID
#define EMOTION_CMD_ID 0x789      // 情绪状态命令ID
#define RANDOM_CMD_ID 0xABC       // 随机效果命令ID
#define MOTOR_CMD_ID 0x301        // 电机控制命令ID
#define FOGGER_CMD_ID 0x321       // 雾化器控制命令ID
#define WOODEN_FISH_HIT_ID 0x123  // 木鱼敲击事件ID

// LED控制命令
#define LED_CMD_OFF 0
#define LED_CMD_ON 1

// 情绪状态命令 - 与TouchDesigner映射对齐
#define EMOTION_NEUTRAL 0  // 中性 - 呼吸灯切换颜色效果 - 音效：中性
#define EMOTION_HAPPY 1    // 开心 - 彩虹效果 - 音效：开心
#define EMOTION_SAD 2      // 伤心 - 紫色追逐效果 - 音效：小雨点，开启雾化器
#define EMOTION_SURPRISE 3 // 惊讶 - 闪电效果 - 音效：打雷闪电，电机开启
#define EMOTION_RANDOM 4   // 兼容性别名

// 随机效果命令
#define RANDOM_START 1     // 开始随机效果
#define RANDOM_STOP 0      // 停止随机效果

// 雾化器控制命令
#define FOGGER_CMD_OFF 0   // 关闭雾化器
#define FOGGER_CMD_ON 1    // 开启雾化器

// 日志标签
static const char *TAG = "MASTER_MUYU";

// 函数声明（解决编译顺序问题）
void send_led_command(uint8_t led_state);
void send_emotion_command(uint8_t emotion_state);
void send_random_command(uint8_t random_state, uint8_t param1, uint8_t param2);
void send_motor_command(uint8_t pwm_duty, uint8_t on_off, uint8_t fade_mode);
void send_fogger_command(uint8_t fogger_state);
void send_wooden_fish_hit_event(void);
void uart_init(void);
void wooden_fish_sensors_init(void);
void wooden_fish_detection_task(void *pvParameters);
void process_touchdesigner_command(const char* cmd);
void uart_rx_task(void *pvParameters);

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

// 过滤器配置 (接收所有消息)
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

// 发送LED控制命令
void send_led_command(uint8_t led_state) {
    twai_message_t tx_message;
    
    // 配置LED控制消息
    tx_message.identifier = LED_CMD_ID;
    tx_message.extd = 0;      // 标准帧
    tx_message.rtr = 0;       // 非远程帧
    tx_message.ss = 1;        // 单次发送
    tx_message.self = 0;      // 不是自发自收
    tx_message.data_length_code = 1;
    tx_message.data[0] = led_state;
    
    // 发送消息
    esp_err_t result = twai_transmit(&tx_message, pdMS_TO_TICKS(1000));
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "发送LED控制命令成功: %s", led_state ? "开启" : "关闭");
    } else {
        ESP_LOGE(TAG, "发送LED控制命令失败: %s", esp_err_to_name(result));
    }
}

// 发送情绪状态命令
void send_emotion_command(uint8_t emotion_state) {
    twai_message_t tx_message;
    
    // 配置情绪状态消息
    tx_message.identifier = EMOTION_CMD_ID;
    tx_message.extd = 0;      // 标准帧
    tx_message.rtr = 0;       // 非远程帧
    tx_message.ss = 1;        // 单次发送
    tx_message.self = 0;      // 不是自发自收
    tx_message.data_length_code = 1;
    tx_message.data[0] = emotion_state;
    
    // 发送消息
    esp_err_t result = twai_transmit(&tx_message, pdMS_TO_TICKS(1000));
    
    const char* emotion_name;
    switch (emotion_state) {
        case EMOTION_HAPPY:
            emotion_name = "开心 (彩虹效果) 音效：开心";
            break;
        case EMOTION_SAD:
            emotion_name = "伤心 (紫色追逐效果) 音效：小雨点";
            // 自动开启雾化器
            send_fogger_command(FOGGER_CMD_ON);
            break;
        case EMOTION_SURPRISE:
            emotion_name = "惊讶 (闪电效果) 音效：打雷闪电";
            // 自动开启电机
            send_motor_command(200, 1, 0);  // PWM=200, 状态=启动, 模式=固定
            break;
        case EMOTION_NEUTRAL:
            emotion_name = "中性 (呼吸灯切换颜色效果) 音效：中性";
            break;
        default:
            emotion_name = "未知/关闭";
            break;
    }
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "发送情绪状态命令成功: %s 灯光：%s", 
            emotion_state == EMOTION_HAPPY ? "开心" :
            emotion_state == EMOTION_SAD ? "伤心" :
            emotion_state == EMOTION_SURPRISE ? "惊讶" : 
            emotion_state == EMOTION_NEUTRAL ? "中性" : "未知",
            emotion_name);
    } else {
        ESP_LOGE(TAG, "发送情绪状态命令失败: %s", esp_err_to_name(result));
    }
}

// 发送随机效果命令
void send_random_command(uint8_t random_state, uint8_t param1, uint8_t param2) {
    twai_message_t tx_message;
    
    // 配置随机效果消息
    tx_message.identifier = RANDOM_CMD_ID;
    tx_message.extd = 0;      // 标准帧
    tx_message.rtr = 0;       // 非远程帧
    tx_message.ss = 1;        // 单次发送
    tx_message.self = 0;      // 不是自发自收
    tx_message.data_length_code = 3;
    tx_message.data[0] = random_state;
    tx_message.data[1] = param1;  // 额外参数1（速度、密度等）
    tx_message.data[2] = param2;  // 额外参数2（亮度、颜色等）
    
    // 发送消息
    esp_err_t result = twai_transmit(&tx_message, pdMS_TO_TICKS(1000));
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "发送随机效果命令成功: %s (参数: %d, %d)", 
                 random_state ? "开始" : "停止", param1, param2);
    } else {
        ESP_LOGE(TAG, "发送随机效果命令失败: %s", esp_err_to_name(result));
    }
}

// 发送电机控制命令
void send_motor_command(uint8_t pwm_duty, uint8_t on_off, uint8_t fade_mode) {
    twai_message_t tx_message;
    
    // 配置电机控制消息
    tx_message.identifier = MOTOR_CMD_ID;
    tx_message.extd = 0;      // 标准帧
    tx_message.rtr = 0;       // 非远程帧
    tx_message.ss = 1;        // 单次发送
    tx_message.self = 0;      // 不是自发自收
    tx_message.data_length_code = 3;  // 更新为3字节
    tx_message.data[0] = pwm_duty;  // PWM占空比(0-255)
    tx_message.data[1] = on_off;    // 启停状态(0=停止,1=启动)
    tx_message.data[2] = fade_mode; // 渐变模式(0=固定速度,1=渐变速度)
    
    // 发送消息
    esp_err_t result = twai_transmit(&tx_message, pdMS_TO_TICKS(1000));
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "发送电机控制命令成功: 占空比=%d, 状态=%s, 模式=%s", 
                 pwm_duty, on_off ? "启动" : "停止", fade_mode ? "渐变" : "固定");
    } else {
        ESP_LOGE(TAG, "发送电机控制命令失败: %s", esp_err_to_name(result));
    }
}

// 发送雾化器控制命令
void send_fogger_command(uint8_t fogger_state) {
    twai_message_t tx_message;
    
    // 配置雾化器控制消息
    tx_message.identifier = FOGGER_CMD_ID;
    tx_message.extd = 0;      // 标准帧
    tx_message.rtr = 0;       // 非远程帧
    tx_message.ss = 1;        // 单次发送
    tx_message.self = 0;      // 不是自发自收
    tx_message.data_length_code = 1;
    tx_message.data[0] = fogger_state;
    
    // 发送消息
    esp_err_t result = twai_transmit(&tx_message, pdMS_TO_TICKS(1000));
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "发送雾化器控制命令成功: %s", fogger_state ? "开启" : "关闭");
    } else {
        ESP_LOGE(TAG, "发送雾化器控制命令失败: %s", esp_err_to_name(result));
    }
}

// 发送木鱼敲击事件消息
void send_wooden_fish_hit_event(void) {
    twai_message_t tx_message;
    
    // 配置木鱼敲击事件消息
    tx_message.identifier = WOODEN_FISH_HIT_ID;
    tx_message.extd = 0;      // 标准帧
    tx_message.rtr = 0;       // 非远程帧
    tx_message.ss = 1;        // 单次发送
    tx_message.self = 0;      // 不是自发自收
    tx_message.data_length_code = 1;
    tx_message.data[0] = 1;   // 敲击事件
    
    // 发送消息
    esp_err_t result = twai_transmit(&tx_message, pdMS_TO_TICKS(1000));
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "发送木鱼敲击事件成功");
        
        // 通过UART也发送给TouchDesigner
        // 使用明确的格式并发送多次以确保接收
        // const char *hit_msg1 = "WOODEN_FISH_HIT\n";
        const char *hit_msg2 = "木鱼被敲击\n";
        // const char *hit_msg3 = "EVENT:WOODFISH_HIT\n";
        
        // uart_write_bytes(UART_NUM, hit_msg1, strlen(hit_msg1));
        // vTaskDelay(pdMS_TO_TICKS(10)); // 短暂延时确保消息分开
        uart_write_bytes(UART_NUM, hit_msg2, strlen(hit_msg2));
        // vTaskDelay(pdMS_TO_TICKS(10));
        // uart_write_bytes(UART_NUM, hit_msg3, strlen(hit_msg3));
    } else {
        ESP_LOGE(TAG, "发送木鱼敲击事件失败: %s", esp_err_to_name(result));
    }
}

// 初始化UART
void uart_init(void) {
    // UART配置
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    // 配置UART参数
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    
    // 设置UART引脚 (使用默认引脚)
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // 安装UART驱动
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0));
    
    // 清空接收缓冲区
    uart_flush(UART_NUM);
    
    ESP_LOGI(TAG, "UART初始化完成，波特率:%d", UART_BAUD_RATE);
}

// 初始化木鱼传感器GPIO
void wooden_fish_sensors_init(void) {
    // 配置GPIO
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;  // 禁用中断
    io_conf.mode = GPIO_MODE_INPUT;         // 输入模式
    io_conf.pin_bit_mask = (1ULL << VIBRATION_SENSOR_PIN) | (1ULL << BUZZER_SENSOR_PIN); // 设置引脚
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;  // 启用下拉电阻
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     // 禁用上拉电阻
    gpio_config(&io_conf);
    
    ESP_LOGI(TAG, "木鱼传感器GPIO初始化完成");
}

// 木鱼敲击检测任务
void wooden_fish_detection_task(void *pvParameters) {
    // 只保留最基本的变量
    uint32_t last_hit_time = 0;
    
    while (1) {
        // 读取传感器状态
        bool vibration_state = gpio_get_level(VIBRATION_SENSOR_PIN);
        bool buzzer_state = gpio_get_level(BUZZER_SENSOR_PIN);
        
        // 获取当前时间
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // 检测两个传感器是否同时触发 && 已经过了消抖时间
        if (vibration_state && buzzer_state && 
            (current_time - last_hit_time > WOODEN_FISH_DEBOUNCE_MS)) {
            
            // 记录敲击时间
            last_hit_time = current_time;
            
            // 发送木鱼敲击事件
            ESP_LOGI(TAG, "检测到木鱼敲击！");
            send_wooden_fish_hit_event();
        }
        
        // 短暂延时
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 处理来自TouchDesigner的命令
void process_touchdesigner_command(const char* cmd) {
    ESP_LOGI(TAG, "收到TouchDesigner命令: %s", cmd);
    
    // 检查单个数字输入 (0-4)
    if (strlen(cmd) == 1 && cmd[0] >= '0' && cmd[0] <= '4') {
        int emotion_val = cmd[0] - '0';
        ESP_LOGI(TAG, "收到情绪数字命令: %d", emotion_val);
        
        // 特殊处理状态4 - 关闭所有子系统
        if (emotion_val == 4) {
            ESP_LOGI(TAG, "关闭所有子系统");
            // 关闭LED灯带
            send_emotion_command(EMOTION_NEUTRAL);  // 设为中性状态
            send_led_command(LED_CMD_OFF);          // 关闭LED
            
            // 关闭雾化器
            send_fogger_command(FOGGER_CMD_OFF);
            
            // 关闭电机
            send_motor_command(0, 0, 0);  // PWM=0, 状态=停止, 模式=固定
            
            // 关闭随机效果
            send_random_command(RANDOM_STOP, 0, 0);
            
            // 发送确认消息到TouchDesigner
            const char *shutdown_msg = "所有子系统已关闭\n";
            uart_write_bytes(UART_NUM, shutdown_msg, strlen(shutdown_msg));
            return;
        }
        
        const char* emotion_name = "未知";  // 初始化默认值
        switch (emotion_val) {
            case 0:
                emotion_name = "neutral (中性)";
                break;
            case 1:
                emotion_name = "happy (开心)";
                break;
            case 2:
                emotion_name = "sad (伤心)";
                break;
            case 3:
                emotion_name = "surprise (惊讶)";
                break;
            default:
                emotion_name = "未知";  // 再次设置以确保安全
                break;
        }
        
        // 在切换到新状态之前，根据需要关闭特定子系统
        if (emotion_val != EMOTION_SAD) {
            // 不是伤心状态，确保雾化器关闭
            send_fogger_command(FOGGER_CMD_OFF);
        }
        
        if (emotion_val != EMOTION_SURPRISE) {
            // 不是惊讶状态，确保电机关闭
            send_motor_command(0, 0, 0);
        }
        
        ESP_LOGI(TAG, "设置情绪状态: %s", emotion_name);
        send_emotion_command((uint8_t)emotion_val);
        return;
    }
    
    // 解析命令
    if (strncmp(cmd, "EMOTION:", 8) == 0) {
        // 情绪控制命令格式: "EMOTION:1" (0=中性, 1=开心, 2=伤心, 3=惊讶)
        int emotion_val = atoi(cmd + 8);
        if (emotion_val >= 0 && emotion_val <= 3) {
            // 在切换到新状态之前，根据需要关闭特定子系统
            if (emotion_val != EMOTION_SAD) {
                // 不是伤心状态，确保雾化器关闭
                send_fogger_command(FOGGER_CMD_OFF);
            }
            
            if (emotion_val != EMOTION_SURPRISE) {
                // 不是惊讶状态，确保电机关闭
                send_motor_command(0, 0, 0);
            }
            
            send_emotion_command((uint8_t)emotion_val);
        } else {
            ESP_LOGE(TAG, "情绪值无效: %d", emotion_val);
        }
    } else if (strncmp(cmd, "EXPRESSION:", 11) == 0) {
        // 表情控制命令格式: "EXPRESSION:HAPPY" (HAPPY=开心, SAD=伤心, SURPRISE=惊讶, NEUTRAL=中性)
        const char* expr_type = cmd + 11;
        
        if (strcmp(expr_type, "HAPPY") == 0) {
            ESP_LOGI(TAG, "设置表情: 开心");
            // 确保关闭不需要的子系统
            send_fogger_command(FOGGER_CMD_OFF);
            send_motor_command(0, 0, 0);
            send_emotion_command(EMOTION_HAPPY);
        } else if (strcmp(expr_type, "SAD") == 0) {
            ESP_LOGI(TAG, "设置表情: 伤心");
            // 确保关闭不需要的子系统
            send_motor_command(0, 0, 0);
            // 雾化器会在send_emotion_command中自动开启
            send_emotion_command(EMOTION_SAD);
        } else if (strcmp(expr_type, "SURPRISE") == 0) {
            ESP_LOGI(TAG, "设置表情: 惊讶");
            // 确保关闭不需要的子系统
            send_fogger_command(FOGGER_CMD_OFF);
            // 电机会在send_emotion_command中自动开启
            send_emotion_command(EMOTION_SURPRISE);
        } else if (strcmp(expr_type, "NEUTRAL") == 0) {
            ESP_LOGI(TAG, "设置表情: 中性");
            // 确保关闭所有额外子系统
            send_fogger_command(FOGGER_CMD_OFF);
            send_motor_command(0, 0, 0);
            send_emotion_command(EMOTION_NEUTRAL);
        } else if (strcmp(expr_type, "UNKNOWN") == 0) {
            ESP_LOGI(TAG, "设置表情: 随机/中性");
            // 确保关闭所有额外子系统
            send_fogger_command(FOGGER_CMD_OFF);
            send_motor_command(0, 0, 0);
            send_emotion_command(EMOTION_NEUTRAL);
        } else {
            ESP_LOGW(TAG, "未知表情类型: %s", expr_type);
        }
    } else if (strncmp(cmd, "LED:", 4) == 0) {
        // LED控制命令格式: "LED:1" (1=开, 0=关)
        int led_val = atoi(cmd + 4);
        send_led_command(led_val ? 1 : 0);
    } else if (strncmp(cmd, "RANDOM:", 7) == 0) {
        // 随机效果命令格式: "RANDOM:1:100:200" (状态:参数1:参数2)
        char *state_str = strtok((char*)(cmd + 7), ":");
        char *param1_str = strtok(NULL, ":");
        char *param2_str = strtok(NULL, ":");
        
        uint8_t state = (state_str != NULL) ? atoi(state_str) : 1;
        uint8_t param1 = (param1_str != NULL) ? atoi(param1_str) : 128;  // 默认值
        uint8_t param2 = (param2_str != NULL) ? atoi(param2_str) : 200;  // 默认值
        
        send_random_command(state, param1, param2);
    } else if (strncmp(cmd, "MOTOR:", 6) == 0) {
        // 电机控制命令格式: "MOTOR:pwm:state:fade" (pwm=0-255, state=0/1, fade=0/1)
        char *pwm_str = strtok((char*)(cmd + 6), ":");
        char *state_str = strtok(NULL, ":");
        char *fade_str = strtok(NULL, ":");
        
        if (pwm_str != NULL && state_str != NULL) {
            uint8_t pwm = atoi(pwm_str);
            uint8_t state = atoi(state_str) ? 1 : 0;
            uint8_t fade = (fade_str != NULL) ? atoi(fade_str) : 0;  // 默认不渐变
            
            send_motor_command(pwm, state, fade);
        } else {
            ESP_LOGE(TAG, "电机控制命令格式错误，应为MOTOR:pwm:state[:fade]");
        }
    } else if (strncmp(cmd, "FOGGER:", 7) == 0) {
        // 雾化器控制命令格式: "FOGGER:1" (1=开, 0=关)
        int fogger_val = atoi(cmd + 7);
        send_fogger_command(fogger_val ? 1 : 0);
    } else if (strcmp(cmd, "WOODFISH_TEST") == 0 || strcmp(cmd, "TEST_HIT") == 0) {
        // 木鱼敲击测试命令 - 模拟敲击事件
        ESP_LOGI(TAG, "模拟木鱼敲击事件");
        send_wooden_fish_hit_event();
    } else if (strlen(cmd) == 1 && cmd[0] >= '5' && cmd[0] <= '9') {
        // 单个数字，但不在0-4范围内的处理
        ESP_LOGW(TAG, "收到数字命令 %c，但只支持0-4的情绪值/控制命令", cmd[0]);
    } else {
        ESP_LOGW(TAG, "未知命令格式: %s", cmd);
    }
}

// UART接收任务
void uart_rx_task(void *pvParameters) {
    uint8_t data[UART_BUF_SIZE];
    char command[UART_BUF_SIZE] = {0};
    int cmd_index = 0;

    while (1) {
        int len = uart_read_bytes(UART_NUM, data, UART_BUF_SIZE - 1, pdMS_TO_TICKS(UART_RX_TIMEOUT_MS));
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                char ch = (char)data[i];

                if (ch == '\r' || ch == '\n') {
                    if (cmd_index > 0) {
                        command[cmd_index] = '\0';
                        ESP_LOGI(TAG, "处理命令: %s", command);
                        process_touchdesigner_command(command);
                        cmd_index = 0; // 重置缓冲
                        memset(command, 0, sizeof(command)); // 清空内容防止干扰
                    }
                    continue;
                }

                // 普通字符追加到命令缓冲区
                if (cmd_index < UART_BUF_SIZE - 1) {
                    command[cmd_index++] = ch;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    // 安装TWAI驱动
    ESP_LOGI(TAG, "CAN发送端初始化中...");
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI(TAG, "TWAI驱动安装成功");

    // 启动TWAI驱动
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "TWAI驱动启动成功");
    ESP_LOGI(TAG, "CAN发送端初始化完成，准备接收TouchDesigner命令...");
    
    // 初始化UART
    uart_init();
    
    // 初始化木鱼传感器
    wooden_fish_sensors_init();
    
    // 创建UART接收任务
    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 5, NULL);
    
    // 创建木鱼敲击检测任务
    xTaskCreate(wooden_fish_detection_task, "wooden_fish_task", 4096, NULL, 5, NULL);
    
    // 发送欢迎消息到TouchDesigner
    const char *welcome_msg = "ESP32 CAN主机已就绪，等待命令...\n";
    uart_write_bytes(UART_NUM, welcome_msg, strlen(welcome_msg));
    
    // 发送命令帮助信息
    const char *help_msg = "🎮 SK6812 GRBW 灯光控制命令:\n"
                          "0 - 中性 (呼吸灯切换颜色效果) - 音效：中性\n"
                          "1 - 开心 (彩虹效果) - 音效：开心\n"
                          "2 - 伤心 (紫色追逐效果) - 音效：小雨点，开启雾化器\n"
                          "3 - 惊讶 (闪电效果) - 音效：打雷闪电，电机开启\n"
                          "4 - 关闭所有子系统 (LED灯带、雾化器、电机等)\n"
                          "EMOTION:0-3 - 同上 (兼容旧格式)\n"
                          "\n🎭 TouchDesigner表情命令:\n"
                          "EXPRESSION:NEUTRAL - 中性表情 (呼吸灯)\n"
                          "EXPRESSION:HAPPY - 开心表情 (彩虹)\n"
                          "EXPRESSION:SAD - 伤心表情 (闪电)\n"
                          "EXPRESSION:SURPRISE - 惊讶表情 (紫色追逐)\n"
                          "EXPRESSION:UNKNOWN - 默认中性表情\n"
                          "\n⚡ 其他设备控制:\n"
                          "LED:1/0 - 开关板载LED\n"
                          "MOTOR:pwm:state:fade - 电机控制\n"
                          "FOGGER:1/0 - 雾化器控制\n"
                          "RANDOM:1:speed:brightness - 随机效果\n"
                          "\n🥢 木鱼测试:\n"
                          "WOODFISH_TEST - 模拟敲击事件\n"
                          "* 真实木鱼敲击将自动检测并发送 *\n";
    uart_write_bytes(UART_NUM, help_msg, strlen(help_msg));
    
    // 发送情绪状态说明
    const char *emotion_info[] = {
        "INFO:情绪状态0=中性(呼吸灯切换颜色效果,音效:中性)\n",
        "INFO:情绪状态1=开心(彩虹效果,音效:开心)\n",
        "INFO:情绪状态2=伤心(紫色追逐效果,音效:小雨点,开启雾化器)\n",
        "INFO:情绪状态3=惊讶(闪电效果,音效:打雷闪电,电机开启)\n",
        "INFO:情绪状态4=关闭所有子系统\n",
        NULL
    };
    
    for (int i = 0; emotion_info[i] != NULL; i++) {
        uart_write_bytes(UART_NUM, emotion_info[i], strlen(emotion_info[i]));
        vTaskDelay(pdMS_TO_TICKS(50));  // 短暂延时确保消息接收
    }

    // 接收CAN消息变量
    twai_message_t rx_message;

    while (1) {
        // 尝试接收来自其他设备的响应
        esp_err_t result = twai_receive(&rx_message, pdMS_TO_TICKS(100));
        
        if (result == ESP_OK) {
            // 打印帧信息
            ESP_LOGI(TAG, "接收到响应 - ID: 0x%lX", (unsigned long)rx_message.identifier);
            
            if (rx_message.rtr) {
                ESP_LOGI(TAG, "[RTR]");
            } else {
                // 打印ASCII数据
                printf("数据: ");
                for (int i = 0; i < rx_message.data_length_code; i++) {
                    printf("%c", rx_message.data[i]);
                }
                printf("\n");
            }
        }
        
        // 短暂延时
        vTaskDelay(pdMS_TO_TICKS(10));
    }
} 