/**
 * 测试命令处理模块 - 通过串口命令测试电机控制
 */

#ifndef TEST_COMMANDS_H
#define TEST_COMMANDS_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"

// 测试命令回调函数定义
typedef void (*set_pwm_duty_fn)(uint8_t duty);
typedef void (*set_ssr_state_fn)(uint8_t state);

// 注册测试命令
void register_test_commands(set_pwm_duty_fn pwm_fn, set_ssr_state_fn ssr_fn);

// 初始化控制台
void initialize_console(void);

#endif /* TEST_COMMANDS_H */ 