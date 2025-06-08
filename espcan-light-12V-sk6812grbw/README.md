# ESPCAN-LIGHT-12V-SK6812GRBW

基于ESP32的CAN总线控制SK6812 GRBW LED灯带项目，整合了`espcan-light`的CAN通信功能和`esp32-sk6812grbw`的SK6812 GRBW灯光控制。

## 项目特性

- **CAN总线通信**: 通过CAN总线接收控制命令
- **SK6812 GRBW控制**: 支持4通道GRBW LED灯带控制
- **12V电压支持**: 适配12V的SK6812灯带
- **多种动态灯光效果**: 支持彩虹、闪电、追逐、呼吸等动画效果
- **实时响应**: 快速响应CAN消息并更新灯光状态

## 硬件要求

- ESP32开发板
- SK6812 GRBW LED灯带（12V）
- CAN总线收发器（如MCP2515）
- 12V电源

## 引脚配置

| 功能 | 引脚 | 说明 |
|------|------|------|
| CAN TX | GPIO 5 | CAN总线发送 |
| CAN RX | GPIO 4 | CAN总线接收 |
| LED数据 | GPIO 18 | SK6812灯带数据线 |
| 状态LED | GPIO 2 | 板载LED状态指示 |

## 软件依赖

- ESP-IDF 5.0+
- PlatformIO (可选)

## CAN消息协议

### LED控制命令 (ID: 0x456)
- 数据长度: 1字节
- 数据[0]: LED状态 (0=关闭, 1=开启)

### 情绪状态命令 (ID: 0x789)
- 数据长度: 1字节
- 数据[0]: 情绪状态
  - 1: 开心 (彩虹效果)
  - 2: 伤心 (闪电效果)
  - 3: 惊讶 (紫色追逐效果)
  - 4: 中性 (呼吸灯切换颜色效果)
  - 其他: 关闭

### 随机效果命令 (ID: 0xABC)
- 数据长度: 1-3字节
- 数据[0]: 启用状态 (0=停止, 1=启动)
- 数据[1]: 速度参数 (0-255, 可选)
- 数据[2]: 亮度参数 (0-255, 可选)

## LED配置

- LED数量: 200个 (可在代码中修改 `WS2812_LEDS_COUNT`)
- 颜色格式: GRBW (绿-红-蓝-白)
- 通信协议: SK6812时序

## 动画效果说明

### 1. 彩虹效果 (开心状态)
- 流动的彩虹色带从LED条的一端移动到另一端
- 色彩平滑过渡，营造愉悦的视觉效果
- 更新频率: 50ms

### 2. 闪电效果 (伤心状态)
- 随机位置产生蓝白色闪电
- 闪电周围有淡蓝色光晕
- 随机黑暗期增加戏剧效果
- 更新频率: 80ms

### 3. 紫色追逐效果 (惊讶状态)
- 紫色光带在LED条上追逐移动
- 光带长度为8个LED，亮度逐渐衰减
- 平滑的追逐动画
- 更新频率: 60ms

### 4. 呼吸灯切换颜色效果 (中性状态)
- 呼吸式亮度变化 (渐亮渐暗)
- 颜色自动切换: 暖白 → 红 → 绿 → 蓝
- 每种颜色持续约10秒
- 更新频率: 30ms

## 编译和烧录

### 使用PlatformIO
```bash
cd espcan-light-12V-sk6812grbw
pio build
pio upload
```

### 使用ESP-IDF
```bash
cd espcan-light-12V-sk6812grbw
idf.py build
idf.py flash
```

## 监控输出
```bash
pio device monitor
# 或
idf.py monitor
```

## 项目结构

```
espcan-light-12V-sk6812grbw/
├── src/
│   ├── main.c              # 主程序文件
│   ├── sk6812_functions.c  # SK6812控制功能
│   └── CMakeLists.txt      # 源文件构建配置
├── platformio.ini          # PlatformIO配置
├── CMakeLists.txt          # 项目构建配置
└── README.md               # 项目说明文档
```

## 开发说明

本项目整合了以下两个项目的功能：

1. **espcan-light**: 提供CAN总线通信功能和基础的情绪灯光效果
2. **esp32-sk6812grbw**: 提供SK6812 GRBW LED的精确时序控制

### 主要改进

- 将WS2812的3通道RGB控制升级为SK6812的4通道GRBW控制
- 优化了内存使用，支持更多LED数量
- 保留了原有的CAN通信协议兼容性
- 增强了12V电压下的稳定性

## 故障排除

1. **LED不亮**: 检查电源电压、数据线连接和GPIO配置
2. **CAN通信失败**: 确认CAN总线连接和波特率设置
3. **颜色不正确**: 验证GRBW通道顺序和时序参数
4. **内存不足**: 减少LED数量或优化内存分配

## 🎮 主控端整合

本项目可与 `espcan-master-muyu` 主控端配合使用，实现完整的木鱼互动灯光系统：

### 系统架构
```
TouchDesigner → espcan-master-muyu → CAN总线 → espcan-light-12V-sk6812grbw → SK6812灯带
                       ↑                                      ↓
                 木鱼传感器检测                        动态灯光效果显示
```

### 快速整合
1. **同时部署两个项目**
2. **连接CAN总线** (GPIO 4/5)
3. **使用主控端命令控制灯光**

详细说明请参考：
- [`SYNC_CONTROL.md`](./SYNC_CONTROL.md) - 同步控制说明
- [`../espcan-master-muyu/INTEGRATION_GUIDE.md`](../espcan-master-muyu/INTEGRATION_GUIDE.md) - 完整整合指南

### 主控端命令示例
```bash
EMOTION:1  # 彩虹效果
EMOTION:2  # 闪电效果
EMOTION:3  # 紫色追逐
EMOTION:4  # 呼吸灯切换颜色
```

## 许可证

本项目基于原始项目的许可证条款。

## 贡献

欢迎提交Issue和Pull Request来改进项目。 