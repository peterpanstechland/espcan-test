# ESP32 CAN发送端 (ESP-IDF版本)

这个项目实现了ESP32 CAN总线通信的发送端，使用ESP-IDF框架开发。在ESP-IDF中，CAN控制器被称为TWAI (Two-Wire Automotive Interface)。

## 硬件连接

1. ESP32 CAN引脚连接:
   - GPIO 4 -> CAN模块的RX
   - GPIO 5 -> CAN模块的TX
   - 3.3V -> CAN模块的VCC
   - GND -> CAN模块的GND

## 功能说明

- 使用ESP-IDF的TWAI接口初始化CAN总线，波特率500kbps
- 每隔1秒发送一条带有递增计数器的CAN消息
- 通过串口输出发送状态信息
- 同时也具备接收来自其他设备响应的能力

## 使用说明

1. 使用PlatformIO编译并上传程序到ESP32
2. 打开串口监视器(波特率115200)查看发送的消息
3. 确保CAN_H和CAN_L线正确连接到接收端设备

## 配置参数

可以通过修改以下常量来调整发送行为:
- `CAN_ID`: CAN消息的标识符(默认0x123)
- `SEND_INTERVAL_MS`: 发送间隔，单位毫秒(默认1000ms)

## 注意事项

- 确保CAN总线两端都有120Ω终端电阻
- 如需更改CAN通信引脚，请修改代码中的`CAN_TX_PIN`和`CAN_RX_PIN`常量
- 此项目使用ESP-IDF框架，不再使用Arduino库 