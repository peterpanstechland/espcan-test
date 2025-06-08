/*
 * 兼容用户原始代码风格的 SK6812 GRBW 示例
 * 这个示例展示了如何将官方 led_strip API 风格的代码适配到我们的 SK6812 组件
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sk6812.h"

// 使用与用户原始代码相同的宏定义
#define EXAMPLE_LED_NUMBERS LED_STRIP_LED_NUM
#define EXAMPLE_LED_GPIO LED_STRIP_RMT_GPIO

// 创建一个兼容层，让 SK6812 组件的 API 看起来像 led_strip
sk6812_handle_t led_strip;

// 兼容函数：模拟 led_strip_set_pixel_grbw
esp_err_t led_strip_set_pixel_grbw_compat(int index, uint8_t g, uint8_t r, uint8_t b, uint8_t w)
{
    return sk6812_set_pixel_grbw(led_strip, index, g, r, b, w);
}

// 兼容函数：模拟 led_strip_refresh
esp_err_t led_strip_refresh_compat(void)
{
    return sk6812_refresh(led_strip);
}

// 兼容函数：模拟 led_strip_clear
esp_err_t led_strip_clear_compat(void)
{
    return sk6812_clear(led_strip);
}

// 兼容函数：模拟 led_strip_enable
esp_err_t led_strip_enable_compat(void)
{
    return sk6812_enable(led_strip);
}

void app_main(void)
{
    // 使用与用户原始代码类似的初始化方式
    printf("初始化 SK6812 GRBW 灯带...\n");
    
    // SK6812 配置
    sk6812_config_t config = {
        .gpio_num = EXAMPLE_LED_GPIO,
        .led_count = EXAMPLE_LED_NUMBERS,
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
    };

    // 创建并启用灯带 (类似原始代码的初始化过程)
    ESP_ERROR_CHECK(sk6812_new(&config, &led_strip));
    ESP_ERROR_CHECK(led_strip_enable_compat());

    printf("SK6812 初始化完成！GPIO: %d, LED数量: %d\n", EXAMPLE_LED_GPIO, EXAMPLE_LED_NUMBERS);

    while (1) {
        // 使用与用户原始代码相同的调用方式：白色 + 蓝色
        printf("设置为白色 + 蓝色效果\n");
        for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {
            // 对应用户原始代码：led_strip_set_pixel_grbw(led_strip, i, 0x00, 0x00, 0x10, 0x20);
            led_strip_set_pixel_grbw_compat(i, 0x00, 0x00, 0x10, 0x20);
        }
        led_strip_refresh_compat();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 全灭 (对应用户原始代码)
        printf("关闭所有 LED\n");
        led_strip_clear_compat();
        led_strip_refresh_compat();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 额外演示：其他 GRBW 组合
        printf("演示其他 GRBW 颜色组合\n");
        
        // 纯绿色
        for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {
            led_strip_set_pixel_grbw_compat(i, 0x30, 0x00, 0x00, 0x00);
        }
        led_strip_refresh_compat();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 纯红色
        for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {
            led_strip_set_pixel_grbw_compat(i, 0x00, 0x30, 0x00, 0x00);
        }
        led_strip_refresh_compat();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 纯蓝色
        for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {
            led_strip_set_pixel_grbw_compat(i, 0x00, 0x00, 0x30, 0x00);
        }
        led_strip_refresh_compat();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 纯白色
        for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {
            led_strip_set_pixel_grbw_compat(i, 0x00, 0x00, 0x00, 0x30);
        }
        led_strip_refresh_compat();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 清空准备下一轮
        led_strip_clear_compat();
        led_strip_refresh_compat();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
} 