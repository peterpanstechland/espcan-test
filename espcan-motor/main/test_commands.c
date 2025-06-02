/**
 * 测试命令处理模块实现 - 通过串口命令测试电机控制
 */

#include "test_commands.h"

static const char* TAG = "test-cmd";

// 全局回调函数指针
static set_pwm_duty_fn g_set_pwm_duty = NULL;
static set_ssr_state_fn g_set_ssr_state = NULL;

/* 'pwm' 命令参数 */
static struct {
    struct arg_int *duty;
    struct arg_end *end;
} pwm_args;

/* 'ssr' 命令参数 */
static struct {
    struct arg_int *state;
    struct arg_end *end;
} ssr_args;

/* 'motor' 命令参数 */
static struct {
    struct arg_int *duty;
    struct arg_int *state;
    struct arg_end *end;
} motor_args;

/**
 * PWM控制命令处理函数
 */
static int cmd_pwm(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&pwm_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, pwm_args.end, argv[0]);
        return 1;
    }

    int duty = pwm_args.duty->ival[0];
    
    // 检查范围
    if (duty < 0 || duty > 255) {
        ESP_LOGE(TAG, "占空比需在0-255范围内");
        return 1;
    }

    // 调用回调函数
    if (g_set_pwm_duty) {
        g_set_pwm_duty((uint8_t)duty);
        printf("PWM占空比已设置为: %d\n", duty);
    } else {
        ESP_LOGE(TAG, "PWM控制函数未注册");
    }

    return 0;
}

/**
 * SSR控制命令处理函数
 */
static int cmd_ssr(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&ssr_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, ssr_args.end, argv[0]);
        return 1;
    }

    int state = ssr_args.state->ival[0];
    
    // 检查范围
    if (state < 0 || state > 1) {
        ESP_LOGE(TAG, "状态需为0(关闭)或1(开启)");
        return 1;
    }

    // 调用回调函数
    if (g_set_ssr_state) {
        g_set_ssr_state((uint8_t)state);
        printf("SSR状态已设置为: %s\n", state ? "开启" : "关闭");
    } else {
        ESP_LOGE(TAG, "SSR控制函数未注册");
    }

    return 0;
}

/**
 * 电机控制命令处理函数（同时设置PWM和SSR）
 */
static int cmd_motor(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&motor_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, motor_args.end, argv[0]);
        return 1;
    }

    int duty = motor_args.duty->ival[0];
    int state = motor_args.state->ival[0];
    
    // 检查范围
    if (duty < 0 || duty > 255) {
        ESP_LOGE(TAG, "占空比需在0-255范围内");
        return 1;
    }
    
    if (state < 0 || state > 1) {
        ESP_LOGE(TAG, "状态需为0(关闭)或1(开启)");
        return 1;
    }

    // 调用回调函数
    if (g_set_pwm_duty && g_set_ssr_state) {
        g_set_pwm_duty((uint8_t)duty);
        g_set_ssr_state((uint8_t)state);
        printf("电机参数已设置 - 占空比: %d, 状态: %s\n", 
               duty, state ? "开启" : "关闭");
    } else {
        ESP_LOGE(TAG, "控制函数未注册");
    }

    return 0;
}

/**
 * 帮助命令处理函数
 */
static int cmd_help(int argc, char **argv)
{
    printf("可用命令:\n");
    printf("  pwm <占空比>          - 设置PWM占空比 (0-255)\n");
    printf("  ssr <状态>            - 设置SSR状态 (0=关闭, 1=开启)\n");
    printf("  motor <占空比> <状态>  - 同时设置占空比和状态\n");
    printf("  help                  - 显示帮助信息\n");
    return 0;
}

/**
 * 注册测试命令
 */
void register_test_commands(set_pwm_duty_fn pwm_fn, set_ssr_state_fn ssr_fn)
{
    // 保存回调函数
    g_set_pwm_duty = pwm_fn;
    g_set_ssr_state = ssr_fn;
    
    // 初始化参数结构
    pwm_args.duty = arg_int0(NULL, NULL, "<占空比>", "PWM占空比 (0-255)");
    pwm_args.end = arg_end(2);
    
    ssr_args.state = arg_int0(NULL, NULL, "<状态>", "SSR状态 (0=关闭, 1=开启)");
    ssr_args.end = arg_end(2);
    
    motor_args.duty = arg_int0(NULL, NULL, "<占空比>", "PWM占空比 (0-255)");
    motor_args.state = arg_int0(NULL, NULL, "<状态>", "SSR状态 (0=关闭, 1=开启)");
    motor_args.end = arg_end(3);

    // 注册命令
    const esp_console_cmd_t pwm_cmd = {
        .command = "pwm",
        .help = "设置PWM占空比 (0-255)",
        .hint = NULL,
        .func = &cmd_pwm,
        .argtable = &pwm_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&pwm_cmd));

    const esp_console_cmd_t ssr_cmd = {
        .command = "ssr",
        .help = "设置SSR状态 (0=关闭, 1=开启)",
        .hint = NULL,
        .func = &cmd_ssr,
        .argtable = &ssr_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ssr_cmd));

    const esp_console_cmd_t motor_cmd = {
        .command = "motor",
        .help = "同时设置占空比和状态",
        .hint = NULL,
        .func = &cmd_motor,
        .argtable = &motor_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&motor_cmd));

    const esp_console_cmd_t help_cmd = {
        .command = "help",
        .help = "显示帮助信息",
        .hint = NULL,
        .func = &cmd_help,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&help_cmd));
    
    ESP_LOGI(TAG, "测试命令已注册");
}

/**
 * 初始化控制台
 */
void initialize_console(void)
{
    /* 禁用stdin缓冲 */
    setvbuf(stdin, NULL, _IONBF, 0);

    /* 初始化控制台 */
    esp_console_config_t console_config = {
        .max_cmdline_length = 256,
        .max_cmdline_args = 8,
        .hint_color = atoi(LOG_COLOR_CYAN)
    };
    ESP_ERROR_CHECK(esp_console_init(&console_config));

    /* 配置linenoise */
    linenoiseSetMultiLine(1);
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);
    linenoiseHistorySetMaxLen(100);

    /* 设置命令提示符 */
    esp_console_register_help_command();
    esp_console_set_prompt("motor> ");
    
    ESP_LOGI(TAG, "控制台初始化完成");
    printf("\n=============================\n");
    printf("ESP32 电机控制测试控制台\n");
    printf("输入 'help' 获取命令列表\n");
    printf("=============================\n\n");
} 