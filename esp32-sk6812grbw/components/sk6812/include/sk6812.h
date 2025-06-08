#ifndef SK6812_H
#define SK6812_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/rmt_tx.h"

#ifdef __cplusplus
extern "C" {
#endif

// SK6812 时序定义 (单位: 纳秒)
#define SK6812_T0H_NS    300   // 0码高电平时间
#define SK6812_T0L_NS    900   // 0码低电平时间  
#define SK6812_T1H_NS    600   // 1码高电平时间
#define SK6812_T1L_NS    600   // 1码低电平时间
#define SK6812_RESET_US  80    // 复位时间 (微秒)

// 颜色结构体 (GRBW)
typedef struct {
    uint8_t g;  // Green
    uint8_t r;  // Red
    uint8_t b;  // Blue
    uint8_t w;  // White
} sk6812_color_t;

// SK6812 配置结构体
typedef struct {
    uint8_t gpio_num;           // GPIO引脚
    uint16_t led_count;         // LED数量
    uint32_t resolution_hz;     // RMT时钟分辨率
    rmt_channel_handle_t rmt_channel;  // RMT通道句柄
} sk6812_config_t;

// SK6812 句柄
typedef struct sk6812_strip_t* sk6812_handle_t;

/**
 * @brief 创建 SK6812 灯带实例
 * 
 * @param config 配置参数
 * @param handle 返回的句柄
 * @return esp_err_t 
 */
esp_err_t sk6812_new(const sk6812_config_t *config, sk6812_handle_t *handle);

/**
 * @brief 删除 SK6812 灯带实例
 * 
 * @param handle 句柄
 * @return esp_err_t 
 */
esp_err_t sk6812_del(sk6812_handle_t handle);

/**
 * @brief 设置单个像素颜色
 * 
 * @param handle 句柄
 * @param index 像素索引
 * @param color 颜色值
 * @return esp_err_t 
 */
esp_err_t sk6812_set_pixel(sk6812_handle_t handle, uint16_t index, sk6812_color_t color);

/**
 * @brief 设置单个像素颜色 (GRBW分量)
 * 
 * @param handle 句柄
 * @param index 像素索引
 * @param g 绿色分量
 * @param r 红色分量
 * @param b 蓝色分量
 * @param w 白色分量
 * @return esp_err_t 
 */
esp_err_t sk6812_set_pixel_grbw(sk6812_handle_t handle, uint16_t index, uint8_t g, uint8_t r, uint8_t b, uint8_t w);

/**
 * @brief 清空所有像素
 * 
 * @param handle 句柄
 * @return esp_err_t 
 */
esp_err_t sk6812_clear(sk6812_handle_t handle);

/**
 * @brief 刷新显示
 * 
 * @param handle 句柄
 * @return esp_err_t 
 */
esp_err_t sk6812_refresh(sk6812_handle_t handle);

/**
 * @brief 启用灯带
 * 
 * @param handle 句柄
 * @return esp_err_t 
 */
esp_err_t sk6812_enable(sk6812_handle_t handle);

/**
 * @brief 禁用灯带
 * 
 * @param handle 句柄
 * @return esp_err_t 
 */
esp_err_t sk6812_disable(sk6812_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // SK6812_H 