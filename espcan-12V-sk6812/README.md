# ESPCAN-12V-SK6812

基于ESP32的CAN总线控制的12V SK6812 LED灯带控制器。

## 功能特点

- 使用ESP32的RMT模块高精度控制SK6812 LED灯带
- 通过CAN总线接收控制命令
- 支持多种情绪状态效果：
  - 开心 - 彩虹效果
  - 伤心 - 闪电效果
  - 惊讶 - 紫色追逐效果
  - 中性 - 呼吸灯切换颜色效果
- 支持高达900个LED的控制

## 硬件连接

| 功能 | GPIO引脚 |
|------|---------|
| CAN TX | GPIO 5 |
| CAN RX | GPIO 4 |
| LED 控制 | GPIO 2 |
| SK6812 数据线 | GPIO 18 |

## CAN通信协议

### 消息ID

| 功能 | CAN ID |
|------|---------|
| LED控制 | 0x456 |
| 情绪状态 | 0x789 |
| 随机效果 | 0xABC |

### 消息格式

#### LED控制命令 (ID: 0x456)
- `data[0]`: LED状态 (0=关闭, 1=开启)

#### 情绪状态命令 (ID: 0x789)
- `data[0]`: 情绪状态
  - 0: 关闭所有效果
  - 1: 开心 (彩虹效果)
  - 2: 伤心 (闪电效果)
  - 3: 惊讶 (紫色追逐效果)
  - 4: 中性 (呼吸灯切换颜色效果)

#### 随机效果命令 (ID: 0xABC)
- `data[0]`: 随机效果状态 (0=停止, 1=开始)
- `data[1]`: 参数1 (速度/亮度)
- `data[2]`: 参数2 (颜色偏好)

## 编译和烧录

本项目使用PlatformIO进行构建和管理。

```bash
# 编译项目
pio run

# 编译并上传
pio run --target upload

# 监控串口输出
pio device monitor
```

## 配置参数

可以在`platformio.ini`文件中修改以下参数：

```ini
build_flags = 
    -DLED_STRIP_RMT_GPIO=18    # LED数据引脚
    -DLED_COUNT=900            # LED灯数量
    -DCAN_TX_PIN=5             # CAN TX引脚
    -DCAN_RX_PIN=4             # CAN RX引脚
```

## 与TouchDesigner集成

本项目可以与TouchDesigner集成，通过CAN总线接收来自TouchDesigner的控制命令。TouchDesigner可以通过串口向ESP32 Master发送命令，然后由Master通过CAN总线转发给本设备。

## 12V SK6812与SK6812 GRBW的区别

本项目与`espcan-light-12V-sk6812grbw`的主要区别在于：

1. 标准SK6812使用RGB通道 (24位/像素)，而SK6812 GRBW还有额外的白色通道 (32位/像素)
2. 颜色表示和控制逻辑不同
3. 内存使用效率更高 (每个LED只需24位而非32位)

其他功能和使用方法完全相同。 