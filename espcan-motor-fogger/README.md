# ESP32 + CAN 雾化器 + 电机控制器

基于ESP32的CAN总线控制器，集成了雾化器和电机控制功能。

## 功能说明

此项目实现：
1. 使用ESP32的LEDC模块输出PWM信号控制电机速度
2. 使用GPIO控制SSR实现电机启停
3. 使用GPIO控制继电器实现雾化器启停
4. 通过CAN总线接收控制命令
5. 根据不同情绪触发不同设备：
   - 伤心情绪触发雾化器
   - 惊讶情绪触发电机

## 硬件连接

### 引脚定义

| 功能          | GPIO引脚    | 配置宏                    |
|--------------|------------|--------------------------|
| CAN TX       | GPIO 5     | CONFIG_CAN_TX_GPIO       |
| CAN RX       | GPIO 4     | CONFIG_CAN_RX_GPIO       |
| PWM输出       | GPIO 25    | CONFIG_PWM_GPIO          |
| SSR控制       | GPIO 26    | CONFIG_SSR_GPIO          |
| 继电器控制     | GPIO 27    | CONFIG_FOGGER_RELAY_GPIO |

### CAN总线ID

| 功能          | CAN ID     | 配置宏                    |
|--------------|------------|--------------------------|
| 电机控制       | 0x301     | CONFIG_CAN_MOTOR_ID      |
| 雾化器控制     | 0x321     | CONFIG_CAN_FOGGER_ID     |
| 情绪状态       | 0x789     | 代码中固定                 |

## CAN消息格式

### 电机控制命令 (ID: 0x301)
- `data[0]`: PWM占空比值(0-255)
- `data[1]`: 启停状态(0=停止, 1=启动)
- `data[2]`: 渐变模式(0=固定速度, 1=渐变速度)

### 雾化器控制命令 (ID: 0x321)
- `data[0]`: 启停状态(0=关闭, 1=开启)

### 情绪状态命令 (ID: 0x789)
- `data[0]`: 情绪状态值
  - 0: 中性
  - 1: 开心
  - 2: 伤心 (触发雾化器)
  - 3: 惊讶 (触发电机)

## 编译和烧录

使用PlatformIO:

```bash
# 编译
pio run

# 编译并上传
pio run --target upload

# 监视串口
pio device monitor
```

## 配置

可以在`platformio.ini`文件中修改配置参数:

```ini
build_flags =
    -DCONFIG_CAN_TX_GPIO=5
    -DCONFIG_CAN_RX_GPIO=4
    -DCONFIG_PWM_GPIO=25
    -DCONFIG_SSR_GPIO=26
    -DCONFIG_FOGGER_RELAY_GPIO=27
    -DCONFIG_PWM_FREQUENCY=20000
    -DCONFIG_CAN_BITRATE=500
    -DCONFIG_CAN_MOTOR_ID=0x301
    -DCONFIG_CAN_FOGGER_ID=0x321
```

## 注意事项

1. 确保CAN总线速率与网络中其他设备一致(默认500kbps)
2. 电机渐变模式会根据当前速度自动调整变化速率
3. 当收到伤心情绪状态时，系统会自动激活雾化器
4. 当收到惊讶情绪状态时，系统会自动激活电机 