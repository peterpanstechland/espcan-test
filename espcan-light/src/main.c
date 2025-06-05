#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "led_strip.h"
#include "esp_system.h"
#include "esp_random.h"

// 定义CAN引脚
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

// 定义LED引脚 - ESP32板载LED通常在GPIO2
#define LED_PIN GPIO_NUM_2

// 定义WS2812灯带控制引脚
#define WS2812_PIN GPIO_NUM_18
#define WS2812_LEDS_COUNT 200  // 原先是100，减少到20进行测试

// CAN消息ID
#define LED_CMD_ID 0x456      // LED控制命令ID
#define EMOTION_CMD_ID 0x789  // 情绪状态命令ID
#define RANDOM_CMD_ID 0xABC   // 随机效果命令ID

// LED控制命令
#define LED_CMD_OFF 0
#define LED_CMD_ON 1

// 情绪状态命令
#define EMOTION_HAPPY 1    // 开心 - 彩虹效果
#define EMOTION_SAD 2      // 伤心 - 紫色追逐
#define EMOTION_SURPRISE 3 // 惊讶 - 蓝色闪电
#define EMOTION_RANDOM 4   // 随机效果

// 随机效果命令
#define RANDOM_START 1     // 开始随机效果
#define RANDOM_STOP 0      // 停止随机效果

// 随机效果参数
typedef struct {
    uint8_t enabled;    // 是否启用随机效果
    uint8_t speed;      // 速度参数 (0-255)
    uint8_t brightness; // 亮度参数 (0-255)
    uint32_t timer;     // 效果计时器
} random_effect_params_t;

// 日志标签
static const char *TAG = "LIGHT_CTRL";

// 当前情绪状态
static uint8_t current_emotion = 0;

// 随机效果参数
static random_effect_params_t random_effect = {0};

// LED灯带句柄
led_strip_handle_t led_strip;

// 函数声明
void rainbow_effect(int delay_ms);
void blue_lightning_effect(int delay_ms);
void purple_chase_effect(int delay_ms);
void meteor_shower_effect(int delay_ms, uint8_t brightness);
void random_explosion_effect(int delay_ms, uint8_t brightness);
void breathing_light_effect(int delay_ms, uint8_t brightness);

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

// 初始化WS2812灯带
static void ws2812_init(void) {
    // LED灯带配置
    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_PIN,
        .max_leds = WS2812_LEDS_COUNT,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    // RMT驱动配置
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .mem_block_symbols = 64,
        .flags.with_dma = false,  // 标准ESP32不支持DMA
    };

    // 创建LED灯带驱动
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    
    // 初始清空灯带
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
}

// 清除所有LED
static void clear_leds(void) {
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
}

// 处理LED控制命令
void handle_led_command(twai_message_t *message) {
    if (message->data_length_code < 1) {
        ESP_LOGE(TAG, "LED命令数据长度不足");
        return;
    }
    
    uint8_t led_state = message->data[0];
    
    // 设置LED状态
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
    
    // 根据情绪状态输出日志
    switch (emotion_state) {
        case EMOTION_HAPPY:
            ESP_LOGI(TAG, "情绪状态设置为: 开心 (彩虹效果)");
            break;
            
        case EMOTION_SAD:
            ESP_LOGI(TAG, "情绪状态设置为: 伤心 (紫色追逐)");
            break;
            
        case EMOTION_SURPRISE:
            ESP_LOGI(TAG, "情绪状态设置为: 惊讶 (蓝色闪电)");
            break;
            
        case EMOTION_RANDOM:
            ESP_LOGI(TAG, "情绪状态设置为: 随机效果 (呼吸灯)");
            // 启用呼吸灯效果
            random_effect.enabled = 1;
            random_effect.speed = 50;      // 中等速度
            random_effect.brightness = 200; // 较高亮度
            break;
            
        default:
            ESP_LOGI(TAG, "情绪状态设置为: 未知");
            clear_leds();
            break;
    }
}

// 处理随机效果命令
void handle_random_command(twai_message_t *message) {
    if (message->data_length_code < 1) {
        ESP_LOGE(TAG, "随机效果命令数据长度不足");
        return;
    }
    
    uint8_t random_state = message->data[0];
    random_effect.enabled = random_state;
    
    // 如果有额外参数则解析
    if (message->data_length_code >= 2) {
        random_effect.speed = message->data[1];
    } else {
        random_effect.speed = 128; // 默认中等速度
    }
    
    if (message->data_length_code >= 3) {
        random_effect.brightness = message->data[2];
    } else {
        random_effect.brightness = 200; // 默认较高亮度
    }
    
    // 重置计时器
    random_effect.timer = 0;
    
    ESP_LOGI(TAG, "随机效果状态设置为: %s (速度: %d, 亮度: %d)", 
             random_state ? "启动" : "停止", 
             random_effect.speed, 
             random_effect.brightness);
}

// 彩虹效果实现
void rainbow_effect(int delay_ms) {
    static uint8_t hue = 0;
    
    // 创建彩虹效果
    for (int i = 0; i < WS2812_LEDS_COUNT; i++) {
        // 计算每个LED的色调，形成彩虹
        uint8_t pos = (i * 256 / WS2812_LEDS_COUNT + hue) & 0xFF;
        
        // 将HSV转换为RGB (简化版彩虹算法)
        if (pos < 85) {
            led_strip_set_pixel(led_strip, i, 255 - pos * 3, pos * 3, 0);
        } else if (pos < 170) {
            pos -= 85;
            led_strip_set_pixel(led_strip, i, 0, 255 - pos * 3, pos * 3);
        } else {
            pos -= 170;
            led_strip_set_pixel(led_strip, i, pos * 3, 0, 255 - pos * 3);
        }
    }
    
    // 更新显示
    led_strip_refresh(led_strip);
    
    // 移动彩虹
    hue += 1;
    
    // 延时
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

// 蓝色闪电效果实现
void blue_lightning_effect(int delay_ms) {
    // 先清空所有LED
    clear_leds();
    
    // 随机生成闪电位置
    int num_flashes = 3 + esp_random() % 4; // 3-6个闪电点
    
    for (int i = 0; i < num_flashes; i++) {
        int pos = esp_random() % WS2812_LEDS_COUNT;
        
        // 设置闪电 - 蓝白色
        uint8_t intensity = 150 + esp_random() % 105; // 150-255
        led_strip_set_pixel(led_strip, pos, intensity/2, intensity/2, intensity);
        
        // 闪电周围有淡蓝色光晕
        if (pos > 0) {
            led_strip_set_pixel(led_strip, pos-1, 20, 20, 120);
        }
        if (pos < WS2812_LEDS_COUNT-1) {
            led_strip_set_pixel(led_strip, pos+1, 20, 20, 120);
        }
    }
    
    // 更新显示
    led_strip_refresh(led_strip);
    
    // 闪电持续时间短
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    
    // 随机决定是否有黑暗期
    if (esp_random() % 5 == 0) {
        clear_leds();
        vTaskDelay(pdMS_TO_TICKS(delay_ms * 2));
    }
}

// 紫色追逐效果实现
void purple_chase_effect(int delay_ms) {
    static int position = 0;
    
    // 先清空所有LED
    clear_leds();
    
    // 追逐灯的长度
    const int chase_length = 5;
    
    // 创建紫色追逐效果
    for (int i = 0; i < chase_length; i++) {
        int pos = (position + i) % WS2812_LEDS_COUNT;
        // 根据距离头部的位置，亮度逐渐降低
        uint8_t brightness = 255 - (i * 255 / chase_length);
        led_strip_set_pixel(led_strip, pos, brightness, 0, brightness);
    }
    
    // 更新显示
    led_strip_refresh(led_strip);
    
    // 移动追逐位置
    position = (position + 1) % WS2812_LEDS_COUNT;
    
    // 延时
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

// 随机流星效果实现
void meteor_shower_effect(int delay_ms, uint8_t brightness) {
    static uint32_t last_meteor = 0;
    static int meteor_positions[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    static uint8_t meteor_colors[10][3] = {0};
    
    // 根据亮度调整效果
    uint8_t max_brightness = brightness;
    
    // 先清空所有LED
    clear_leds();
    
    // 随机生成新流星
    random_effect.timer++;
    
    // 每隔一段时间生成新流星
    if (random_effect.timer - last_meteor > (300 - random_effect.speed)) {
        // 寻找空闲位置
        for (int i = 0; i < 10; i++) {
            if (meteor_positions[i] == -1) {
                // 创建新流星
                meteor_positions[i] = 0;
                
                // 随机颜色
                uint8_t color_type = esp_random() % 5;
                switch (color_type) {
                    case 0: // 白色
                        meteor_colors[i][0] = max_brightness;
                        meteor_colors[i][1] = max_brightness;
                        meteor_colors[i][2] = max_brightness;
                        break;
                    case 1: // 蓝色
                        meteor_colors[i][0] = 0;
                        meteor_colors[i][1] = 0;
                        meteor_colors[i][2] = max_brightness;
                        break;
                    case 2: // 绿色
                        meteor_colors[i][0] = 0;
                        meteor_colors[i][1] = max_brightness;
                        meteor_colors[i][2] = 0;
                        break;
                    case 3: // 红色
                        meteor_colors[i][0] = max_brightness;
                        meteor_colors[i][1] = 0;
                        meteor_colors[i][2] = 0;
                        break;
                    case 4: // 紫色
                        meteor_colors[i][0] = max_brightness;
                        meteor_colors[i][1] = 0;
                        meteor_colors[i][2] = max_brightness;
                        break;
                }
                
                last_meteor = random_effect.timer;
                break;
            }
        }
    }
    
    // 更新所有流星
    for (int i = 0; i < 10; i++) {
        if (meteor_positions[i] >= 0) {
            // 流星头部
            int pos = meteor_positions[i];
            
            if (pos < WS2812_LEDS_COUNT) {
                // 设置流星头部
                led_strip_set_pixel(led_strip, pos, 
                                   meteor_colors[i][0], 
                                   meteor_colors[i][1], 
                                   meteor_colors[i][2]);
                
                // 流星尾部
                for (int tail = 1; tail < 5; tail++) {
                    if (pos - tail >= 0) {
                        // 尾部亮度递减
                        float fade = 1.0f - (tail / 5.0f);
                        led_strip_set_pixel(led_strip, pos - tail, 
                                          meteor_colors[i][0] * fade, 
                                          meteor_colors[i][1] * fade, 
                                          meteor_colors[i][2] * fade);
                    }
                }
            }
            
            // 移动流星
            meteor_positions[i]++;
            
            // 如果流星离开了LED条，则标记为空闲
            if (meteor_positions[i] > WS2812_LEDS_COUNT + 5) {
                meteor_positions[i] = -1;
            }
        }
    }
    
    // 更新显示
    led_strip_refresh(led_strip);
    
    // 延时
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

// 随机颜色爆炸效果
void random_explosion_effect(int delay_ms, uint8_t brightness) {
    static uint32_t last_explosion = 0;
    static int explosion_center = -1;
    static uint8_t explosion_size = 0;
    static uint8_t explosion_color[3] = {0};
    
    // 先清空所有LED
    clear_leds();
    
    // 计时器更新
    random_effect.timer++;
    
    // 如果没有活跃的爆炸或爆炸已经完成
    if (explosion_center == -1 || explosion_size > 20) {
        // 每隔一段时间生成新爆炸
        if (random_effect.timer - last_explosion > (500 - random_effect.speed * 2)) {
            // 创建新爆炸
            explosion_center = esp_random() % WS2812_LEDS_COUNT;
            explosion_size = 0;
            
            // 随机颜色
            uint8_t color_r = esp_random() % 256;
            uint8_t color_g = esp_random() % 256;
            uint8_t color_b = esp_random() % 256;
            
            // 确保颜色足够亮
            while (color_r + color_g + color_b < 150) {
                color_r = esp_random() % 256;
                color_g = esp_random() % 256;
                color_b = esp_random() % 256;
            }
            
            // 应用亮度
            explosion_color[0] = (color_r * brightness) / 255;
            explosion_color[1] = (color_g * brightness) / 255;
            explosion_color[2] = (color_b * brightness) / 255;
            
            last_explosion = random_effect.timer;
        }
    }
    
    // 如果有活跃的爆炸
    if (explosion_center != -1) {
        // 爆炸中心亮度最高
        float center_brightness = 1.0f - (explosion_size / 20.0f);
        led_strip_set_pixel(led_strip, explosion_center, 
                           explosion_color[0] * center_brightness, 
                           explosion_color[1] * center_brightness, 
                           explosion_color[2] * center_brightness);
        
        // 爆炸向两侧扩散
        for (int i = 1; i <= explosion_size; i++) {
            // 计算衰减
            float fade = 1.0f - (i / (float)explosion_size);
            
            // 左侧
            if (explosion_center - i >= 0) {
                led_strip_set_pixel(led_strip, explosion_center - i, 
                                   explosion_color[0] * fade, 
                                   explosion_color[1] * fade, 
                                   explosion_color[2] * fade);
            }
            
            // 右侧
            if (explosion_center + i < WS2812_LEDS_COUNT) {
                led_strip_set_pixel(led_strip, explosion_center + i, 
                                   explosion_color[0] * fade, 
                                   explosion_color[1] * fade, 
                                   explosion_color[2] * fade);
            }
        }
        
        // 增加爆炸尺寸
        explosion_size++;
        
        // 如果爆炸完成
        if (explosion_size > 20) {
            explosion_center = -1; // 标记爆炸结束
        }
    }
    
    // 更新显示
    led_strip_refresh(led_strip);
    
    // 延时
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

// 呼吸灯效果实现
void breathing_light_effect(int delay_ms, uint8_t brightness) {
    static float breath_level = 0.0f;
    static int direction = 1;  // 1 = 增加亮度, -1 = 减少亮度
    
    // 呼吸灯的颜色 - 使用柔和的白色
    uint8_t base_r = 255;
    uint8_t base_g = 220;
    uint8_t base_b = 180;
    
    // 计算当前亮度级别
    float intensity = breath_level * breath_level; // 使用平方关系使变化看起来更自然
    
    // 应用主亮度参数
    intensity = intensity * brightness / 255.0f;
    
    // 设置所有LED为相同的呼吸亮度
    for (int i = 0; i < WS2812_LEDS_COUNT; i++) {
        led_strip_set_pixel(led_strip, i, 
                          base_r * intensity, 
                          base_g * intensity, 
                          base_b * intensity);
    }
    
    // 更新显示
    led_strip_refresh(led_strip);
    
    // 更新呼吸级别
    breath_level += direction * 0.01f;
    
    // 改变方向
    if (breath_level >= 1.0f) {
        breath_level = 1.0f;
        direction = -1;
    } else if (breath_level <= 0.0f) {
        breath_level = 0.0f;
        direction = 1;
    }
    
    // 延时
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
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
                // 伤心 - 紫色追逐
                purple_chase_effect(30);
                break;
                
            case EMOTION_SURPRISE:
                // 惊讶 - 蓝色闪电
                blue_lightning_effect(80);
                break;
                
            case EMOTION_RANDOM:
                // 随机效果 - 呼吸灯效果
                if (random_effect.enabled) {
                    // 使用呼吸灯效果替代原来的效果
                    breathing_light_effect(30, random_effect.brightness);
                } else {
                    clear_leds();
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                break;
                
            default:
                // 默认状态 - 关闭灯
                clear_leds();
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
        }
    }
}

// 闪烁LED指示CAN总线就绪
void blink_can_ready(void) {
    // 闪烁两次绿色灯以指示CAN总线就绪
    for (int i = 0; i < 2; i++) {
        // 设置所有LED为绿色
        for (int j = 0; j < 5; j++) {
            led_strip_set_pixel(led_strip, j, 0, 255, 0);  // 绿色
        }
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(300));  // 亮300ms
        
        // 关闭所有LED
        clear_leds();
        vTaskDelay(pdMS_TO_TICKS(300));  // 灭300ms
    }
}

void app_main(void)
{
    // 配置LED引脚
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0); // 初始状态为关闭
    
    // 初始化WS2812灯带
    ws2812_init();
    
    // 测试代码：设置几个固定颜色的LED，检查基本功能
    ESP_LOGI(TAG, "显示固定颜色测试 - 5秒");
    // 设置不同颜色块测试
    for (int i = 0; i < WS2812_LEDS_COUNT && i < 20; i++) {
        if (i < 5) {
            led_strip_set_pixel(led_strip, i, 255, 0, 0);   // 红色
        } else if (i < 10) {
            led_strip_set_pixel(led_strip, i, 0, 255, 0);   // 绿色
        } else if (i < 15) {
            led_strip_set_pixel(led_strip, i, 0, 0, 255);   // 蓝色
        } else {
            led_strip_set_pixel(led_strip, i, 255, 255, 255); // 白色
        }
    }
    led_strip_refresh(led_strip);
    
    // 延迟更长时间以便观察
    vTaskDelay(pdMS_TO_TICKS(5000));
    clear_leds();
    
    // 测试代码：设置一个固定的情绪状态进行测试
    current_emotion = 0; // 初始化为0，不显示任何情绪效果
    
    // 安装TWAI驱动
    ESP_LOGI(TAG, "CAN接收端初始化中...");
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI(TAG, "TWAI驱动安装成功");

    // 启动TWAI驱动
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "TWAI驱动启动成功");
    ESP_LOGI(TAG, "CAN接收端初始化完成，等待接收数据...");
    
    // 闪烁绿色LED两次以指示CAN总线就绪
    blink_can_ready();
    
    // 创建情绪动画任务
    xTaskCreate(emotion_animation_task, "emotion_animation", 4096, NULL, 5, NULL);

    twai_message_t rx_message;
    
    while (1) {
        // 接收CAN消息
        esp_err_t result = twai_receive(&rx_message, pdMS_TO_TICKS(10000));
        
        if (result == ESP_OK) {
            // 打印帧信息
            ESP_LOGI(TAG, "接收到CAN帧 - ID: 0x%lX", (unsigned long)rx_message.identifier);
            
            // 检查消息类型
            if (rx_message.identifier == LED_CMD_ID) {
                handle_led_command(&rx_message);
            } else if (rx_message.identifier == EMOTION_CMD_ID) {
                handle_emotion_command(&rx_message);
            } else if (rx_message.identifier == RANDOM_CMD_ID) {
                handle_random_command(&rx_message);
            } else if (rx_message.rtr) {
                ESP_LOGI(TAG, "[RTR] 请求长度: %d", rx_message.data_length_code);
            } else {
                // 打印ASCII数据
                printf("数据 (ASCII): ");
                for (int i = 0; i < rx_message.data_length_code; i++) {
                    printf("%c", rx_message.data[i]);
                }
                printf("\n");
                
                // 打印HEX格式数据
                printf("数据 (HEX): ");
                if (rx_message.extd) {
                    printf("扩展帧 ");
                } else {
                    printf("标准帧 ");
                }
                
                printf("数据长度: %d 字节 - ", rx_message.data_length_code);
                for (int i = 0; i < rx_message.data_length_code; i++) {
                    printf("0x%02X ", rx_message.data[i]);
                }
                printf("\n");
            }
        } else if (result == ESP_ERR_TIMEOUT) {
            ESP_LOGI(TAG, "等待接收超时，继续等待...");
        } else {
            ESP_LOGE(TAG, "接收CAN消息失败: %s", esp_err_to_name(result));
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
} 