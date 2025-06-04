# ESP32 CAN发送端 (ESP-IDF版本)

这个项目实现了ESP32作为CAN总线通信的发送端，使用ESP-IDF框架开发。在ESP-IDF中，CAN控制器被称为TWAI (Two-Wire Automotive Interface)。该项目可接收来自TouchDesigner的串口命令，并将其转换为CAN总线消息发送给接收设备，同时还支持木鱼敲击事件的监测与通知。

## 目录

- [硬件连接](#硬件连接)
- [功能说明](#功能说明)
- [代码结构](#代码结构)
- [使用说明](#使用说明)
- [支持的串口命令](#支持的串口命令)
- [CAN消息格式](#CAN消息格式)
- [配置参数](#配置参数)
- [木鱼敲击监测](#木鱼敲击监测)
- [TouchDesigner集成](#TouchDesigner集成)
- [注意事项](#注意事项)
- [故障排除](#故障排除)

## 硬件连接

### ESP32 CAN引脚连接
- **GPIO 4** -> CAN模块的RX
- **GPIO 5** -> CAN模块的TX
- **3.3V** -> CAN模块的VCC
- **GND** -> CAN模块的GND

### 木鱼传感器连接
- **GPIO 18** -> 震动传感器输出
- **GPIO 19** -> 敲击声音传感器输出
- **3.3V/5V** -> 传感器VCC (根据传感器要求)
- **GND** -> 传感器GND

### 串口连接
- ESP32的USB接口 -> 计算机USB端口 (用于接收TouchDesigner命令)

## 功能说明

本项目提供以下主要功能：

1. **CAN总线通信**
   - 使用ESP-IDF的TWAI接口初始化CAN总线，波特率500kbps
   - 可发送多种类型的CAN消息，包括LED控制、情绪状态、随机效果等
   - 接收来自其他设备的CAN响应

2. **串口命令处理**
   - 接收来自TouchDesigner的串口命令
   - 支持多种命令格式，包括EMOTION、EXPRESSION、LED等
   - 实时显示接收到的原始数据和命令处理状态

3. **木鱼敲击事件监测**
   - 通过GPIO监测木鱼的震动和声音传感器
   - 当检测到敲击事件时，通过CAN总线发送通知
   - 同时通过串口发送事件通知给TouchDesigner

4. **LED和特效控制**
   - 控制接收端的LED灯效果
   - 支持多种情绪表达效果，如开心、伤心、惊讶等
   - 支持随机效果和参数调整

5. **电机和雾化器控制**
   - 可控制外部设备如电机和雾化器的开关状态
   - 支持PWM调速功能

## 代码结构

主要代码文件为`src/main.c`，包含以下关键部分：

### 1. 常量和配置定义
```c
// 定义CAN引脚
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

// 消息ID
#define LED_CMD_ID 0x456          // LED控制命令ID
#define EMOTION_CMD_ID 0x789      // 情绪状态命令ID
#define RANDOM_CMD_ID 0xABC       // 随机效果命令ID
#define MOTOR_CMD_ID 0x301        // 电机控制命令ID
#define FOGGER_CMD_ID 0x321       // 雾化器控制命令ID
#define WOODEN_FISH_HIT_ID 0x123  // 木鱼敲击事件ID
```

### 2. TWAI (CAN) 配置
```c
// TWAI配置
static const twai_general_config_t g_config = {
    .mode = TWAI_MODE_NORMAL,
    .tx_io = CAN_TX_PIN,
    .rx_io = CAN_RX_PIN,
    // ... 其他配置
};

// 波特率配置 (500Kbps)
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

// 过滤器配置 (接收所有消息)
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
```

### 3. 消息发送函数
代码包含多个发送不同类型CAN消息的函数：
```c
void send_led_command(uint8_t led_state);
void send_emotion_command(uint8_t emotion_state);
void send_random_command(uint8_t random_state, uint8_t param1, uint8_t param2);
void send_motor_command(uint8_t pwm_duty, uint8_t on_off);
void send_fogger_command(uint8_t fogger_state);
void send_wooden_fish_hit_event(void);
```

### 4. 串口命令处理
```c
void process_touchdesigner_command(const char* cmd) {
    // 解析命令并调用相应的发送函数
    if (strncmp(cmd, "EMOTION:", 8) == 0) {
        // 处理情绪命令
    } else if (strncmp(cmd, "EXPRESSION:", 11) == 0) {
        // 处理表情命令
    } // ... 其他命令处理
}

void uart_rx_task(void *pvParameters) {
    // 接收串口数据并处理命令
}
```

### 5. 木鱼敲击检测
```c
void wooden_fish_detection_task(void *pvParameters) {
    // 监测传感器状态并检测敲击事件
}
```

### 6. 主函数
```c
void app_main(void) {
    // 初始化TWAI驱动
    // 初始化UART和传感器
    // 创建任务
    // 发送欢迎消息和帮助信息
    // 主循环接收CAN响应
}
```

## 使用说明

1. **准备工作**
   - 确保安装了PlatformIO和ESP-IDF环境
   - 将ESP32连接到计算机USB端口
   - 正确连接CAN模块和传感器（如果使用）

2. **编译和上传**
   - 使用PlatformIO编译项目：`platformio run`
   - 上传程序到ESP32：`platformio run --target upload`
   - 或通过PlatformIO IDE的界面操作

3. **监视输出**
   - 打开串口监视器(波特率115200)查看输出信息
   - 可以使用命令：`platformio device monitor`
   - 或通过PlatformIO IDE的终端监视器

4. **发送命令**
   - 可以通过串口监视器手动发送命令进行测试
   - 或通过TouchDesigner发送命令（参见TouchDesigner集成部分）

5. **连接CAN设备**
   - 确保CAN_H和CAN_L线正确连接到接收端设备
   - 检查终端电阻是否正确安装

## 支持的串口命令

本项目支持以下串口命令格式:

### 1. 情绪控制命令
- `EMOTION:1` - 设置开心情绪
- `EMOTION:2` - 设置伤心情绪
- `EMOTION:3` - 设置惊讶情绪
- `EMOTION:4` - 设置随机情绪

### 2. 表情控制命令 (TouchDesigner格式)
- `EXPRESSION:HAPPY` - 设置开心表情
- `EXPRESSION:SAD` - 设置伤心表情
- `EXPRESSION:SURPRISE` - 设置惊讶表情
- `EXPRESSION:UNKNOWN` - 设置随机表情

### 3. LED控制命令
- `LED:1` - 打开LED
- `LED:0` - 关闭LED

### 4. 随机效果命令
- `RANDOM:1:speed:brightness` - 启动随机效果
  - speed: 速度参数（0-255）
  - brightness: 亮度参数（0-255）
- `RANDOM:0` - 停止随机效果

### 5. 电机控制命令
- `MOTOR:pwm:state` - 控制电机
  - pwm: PWM占空比（0-255）
  - state: 开关状态（0=关闭，1=开启）

### 6. 雾化器控制命令
- `FOGGER:1` - 开启雾化器
- `FOGGER:0` - 关闭雾化器

### 7. 测试命令
- `WOODFISH_TEST` 或 `TEST_HIT` - 模拟木鱼敲击事件

## CAN消息格式

本项目发送的CAN消息使用以下格式：

1. **LED控制消息**
   - ID: 0x456
   - 数据长度: 1字节
   - 数据[0]: LED状态（0=关闭，1=开启）

2. **情绪状态消息**
   - ID: 0x789
   - 数据长度: 1字节
   - 数据[0]: 情绪值（1=开心，2=伤心，3=惊讶，4=随机）

3. **随机效果消息**
   - ID: 0xABC
   - 数据长度: 3字节
   - 数据[0]: 随机效果状态（0=停止，1=开始）
   - 数据[1]: 参数1（速度、密度等）
   - 数据[2]: 参数2（亮度、颜色等）

4. **电机控制消息**
   - ID: 0x301
   - 数据长度: 2字节
   - 数据[0]: PWM占空比（0-255）
   - 数据[1]: 开关状态（0=停止，1=启动）

5. **雾化器控制消息**
   - ID: 0x321
   - 数据长度: 1字节
   - 数据[0]: 雾化器状态（0=关闭，1=开启）

6. **木鱼敲击事件消息**
   - ID: 0x123
   - 数据长度: 1字节
   - 数据[0]: 敲击事件（1=敲击）

## 配置参数

可以通过修改代码中的常量来调整系统行为：

### CAN配置
- `CAN_TX_PIN` 和 `CAN_RX_PIN`: CAN通信引脚
- 波特率: 通过 `TWAI_TIMING_CONFIG_500KBITS()` 设置为500kbps

### 木鱼传感器配置
- `VIBRATION_SENSOR_PIN`: 震动传感器引脚（默认GPIO 18）
- `BUZZER_SENSOR_PIN`: 声音传感器引脚（默认GPIO 19）
- `WOODEN_FISH_DEBOUNCE_MS`: 消抖时间（默认50ms）

### UART配置
- `UART_BAUD_RATE`: 波特率（默认115200）
- `UART_BUF_SIZE`: 缓冲区大小（默认1024字节）
- `UART_RX_TIMEOUT_MS`: 接收超时时间（默认10ms）

### 消息ID
可以根据需要修改各种消息的ID：
- `LED_CMD_ID`: LED控制命令ID（默认0x456）
- `EMOTION_CMD_ID`: 情绪状态命令ID（默认0x789）
- 等等...

## 木鱼敲击监测

木鱼敲击监测功能通过两个传感器协同工作：

1. **传感器原理**
   - 震动传感器检测物理震动
   - 声音传感器检测敲击声音
   - 两个传感器同时触发才认为是有效敲击

2. **消抖处理**
   - 使用`WOODEN_FISH_DEBOUNCE_MS`设置的时间间隔（默认50ms）防止重复触发
   - 只有当上次敲击后经过了消抖时间才会处理新的敲击

3. **事件通知**
   - 敲击事件通过CAN总线发送给接收设备
   - 同时通过串口发送三种格式的通知给TouchDesigner:
     ```
     WOODEN_FISH_HIT
     木鱼被敲击
     EVENT:WOODFISH_HIT
     ```

## TouchDesigner集成

本项目设计为与TouchDesigner软件集成，使用串口通信：

1. **发送命令示例**
   在TouchDesigner中，可以使用类似以下Python代码发送命令：
   ```python
   def onTableChange(dat):
       expr = dat[0, 0].val
       if '开心' in expr:
           cmd = 'EXPRESSION:HAPPY'
       elif '伤心' in expr:
           cmd = 'EXPRESSION:SAD'
       elif '惊讶' in expr:
           cmd = 'EXPRESSION:SURPRISE'
       else:
           cmd = 'EXPRESSION:UNKNOWN'
       
       op('serial1').send(cmd + '\n')
       debug('📤 发送:', cmd)
       return
   ```

2. **接收事件**
   TouchDesigner可以监听串口数据接收木鱼敲击事件：
   ```python
   def onSerialReceive(data):
       if 'WOODEN_FISH_HIT' in data or 'EVENT:WOODFISH_HIT' in data:
           # 处理木鱼敲击事件
           pass
   ```

3. **串口配置**
   - 在TouchDesigner中设置串口波特率为115200
   - 确保选择正确的COM端口（连接ESP32的端口）
   - 配置为8N1（8位数据位，无奇偶校验，1位停止位）

## 注意事项

1. **CAN总线连接**
   - 确保CAN总线两端都有120Ω终端电阻
   - 检查CAN_H和CAN_L线是否正确连接，不要接反
   - 保持CAN总线布线尽量短且远离干扰源

2. **电源供应**
   - 确保ESP32有稳定的电源供应
   - 如果使用外部传感器，确保电源电压匹配

3. **代码修改**
   - 如需更改CAN通信引脚，请修改代码中的`CAN_TX_PIN`和`CAN_RX_PIN`常量
   - 如需更改传感器引脚，修改`VIBRATION_SENSOR_PIN`和`BUZZER_SENSOR_PIN`常量

4. **框架使用**
   - 此项目使用ESP-IDF框架，不再使用Arduino库
   - 如果从Arduino迁移，注意API差异

## 故障排除

1. **CAN通信问题**
   - 检查CAN模块连接是否正确
   - 确认终端电阻是否安装
   - 检查波特率设置是否匹配
   - 尝试使用示波器或逻辑分析仪观察CAN信号

2. **串口命令不响应**
   - 检查串口监视器波特率是否设置为115200
   - 确保命令格式正确，注意大小写和冒号
   - 检查命令后是否有换行符（\n）

3. **木鱼敲击不触发**
   - 检查传感器连接和电源
   - 调整`WOODEN_FISH_DEBOUNCE_MS`值
   - 检查传感器灵敏度，可能需要硬件调整

4. **编译错误**
   - 确保ESP-IDF环境正确安装
   - 检查所有必需的库是否已安装
   - 查看编译日志了解具体错误 