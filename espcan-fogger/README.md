# ESP32 雾化器 CAN 控制器

本项目实现了基于 ESP32 的雾化器控制器，通过 CAN 总线接收控制命令，控制外部继电器来启停雾化器设备。

## 功能特点

- 通过 CAN 总线接收控制命令
- 使用继电器控制雾化器的启停
- 发送状态确认消息到 CAN 总线
- 系统默认上电状态为关闭，安全可靠

## 硬件连接

### ESP32 接线

| 功能 | ESP32 引脚 | 外部连接 |
|-----|-----------|---------|
| CAN TX | GPIO5 | CAN 收发器 TX 引脚 |
| CAN RX | GPIO4 | CAN 收发器 RX 引脚 |
| 继电器控制 | GPIO26 | 继电器模块信号输入端 |

### 继电器接线

- 继电器输入端：连接到 ESP32 GPIO26
- 继电器常开端（NO）：连接到雾化器电源正极
- 继电器公共端（COM）：连接到电源正极
- 继电器常闭端（NC）：不连接

## CAN 通信协议

- **消息 ID**：0x321
- **数据格式**：
  - `Data[0]`：雾化器状态（0=关闭，1=开启）
  - `Data[1]`：确认标志（仅在响应中使用，固定为0x01）

### 示例：

1. 开启雾化器：
   ```
   ID: 0x321, Data: [0x01]
   ```

2. 关闭雾化器：
   ```
   ID: 0x321, Data: [0x00]
   ```

## 编译与烧录

本项目基于 PlatformIO 开发，请确保已安装 PlatformIO 环境。

```bash
# 编译项目
pio run

# 烧录到 ESP32
pio run --target upload

# 查看日志输出
pio device monitor
```

## 配置选项

项目默认配置在 `platformio.ini` 文件中，可根据需要修改：

```ini
build_flags = 
    -D CONFIG_CAN_TX_GPIO=5
    -D CONFIG_CAN_RX_GPIO=4
    -D CONFIG_CAN_BITRATE=500
    -D CONFIG_FOGGER_RELAY_GPIO=26
    -D CONFIG_CAN_FOGGER_ID=0x321
```

## 注意事项

1. 继电器选择：建议使用带光耦隔离的继电器模块，确保信号和负载电路隔离
2. 雾化器电源：根据雾化器功率选择合适的继电器和电源
3. 接线安全：处理高压电路时务必确保安全，必要时请专业人士操作 