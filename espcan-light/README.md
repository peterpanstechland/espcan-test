# ESP32 灯光控制模块 (ESP-IDF版本)

这个项目实现了ESP32控制WS2812灯带的功能，通过CAN总线接收情绪状态命令，并显示不同的灯光效果。使用ESP-IDF框架开发。在ESP-IDF中，CAN控制器被称为TWAI (Two-Wire Automotive Interface)。

## 硬件连接

1. ESP32 CAN引脚连接:
   - GPIO 4 -> CAN模块的RX
   - GPIO 5 -> CAN模块的TX
   - 3.3V -> CAN模块的VCC
   - GND -> CAN模块的GND

2. WS2812灯带连接:
   - GPIO 18 -> WS2812数据线
   - 5V -> WS2812电源线
   - GND -> WS2812地线

## 功能说明

- 使用ESP-IDF的TWAI接口初始化CAN总线，波特率500kbps
- 监听CAN总线上的情绪状态命令(ID: 0x789)
- 根据不同情绪状态显示不同灯光效果:
  - 开心(EMOTION_HAPPY=1): 彩虹效果
  - 伤心(EMOTION_SAD=2): 紫色追逐效果
  - 惊讶(EMOTION_SURPRISE=3): 蓝色闪电效果
  - 随机(EMOTION_RANDOM=4): 呼吸灯效果

## 使用说明

1. 使用PlatformIO编译并上传程序到ESP32
2. 打开串口监视器(波特率115200)查看接收到的消息
3. 确保CAN_H和CAN_L线正确连接到发送端设备

## 注意事项

- 确保CAN总线两端都有120Ω终端电阻
- 如需更改CAN通信引脚，请修改代码中的`CAN_TX_PIN`和`CAN_RX_PIN`常量
- 如需更改WS2812灯带引脚，请修改代码中的`WS2812_PIN`常量
- 此项目使用ESP-IDF框架 