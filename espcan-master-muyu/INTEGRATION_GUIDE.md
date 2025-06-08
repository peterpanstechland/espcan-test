# ESP32 木鱼灯光系统整合指南

本指南说明如何使用 `espcan-master-muyu` 主控端来控制 `espcan-light-12V-sk6812grbw` SK6812 GRBW LED灯带，实现完整的木鱼互动灯光系统。

## 🎯 系统架构

```
TouchDesigner/串口命令 → ESP32主控端 → CAN总线 → ESP32灯光端 → SK6812 GRBW LED灯带
      ↑                    ↓                            ↓
  木鱼敲击反馈      木鱼传感器检测             动态灯光效果显示
```

### 设备角色
- **espcan-master-muyu**: CAN总线主控端，负责接收命令和检测木鱼敲击
- **espcan-light-12V-sk6812grbw**: CAN总线从设备，负责控制SK6812 GRBW LED灯带

## 🔌 硬件连接

### CAN总线连接
```
主控端 (espcan-master-muyu)    灯光端 (espcan-light-12V-sk6812grbw)
GPIO 5 (CAN TX) ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ GPIO 4 (CAN RX)
GPIO 4 (CAN RX) ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ GPIO 5 (CAN TX)
```

**注意**: 
- 使用标准CAN收发器模块 (如MCP2515)
- 确保CAN_H和CAN_L正确连接
- 在总线两端添加120Ω终端电阻

### LED灯带连接
```
ESP32灯光端          SK6812 GRBW LED灯带
GPIO 18 ━━━━━━━━━━━━━ DATA IN
12V电源 ━━━━━━━━━━━━━ VCC
GND ━━━━━━━━━━━━━━━━━ GND
```

### 木鱼传感器连接 (主控端)
```
ESP32主控端          传感器
GPIO 22 ━━━━━━━━━━━━━ 震动传感器
GPIO 23 ━━━━━━━━━━━━━ 声音传感器
3.3V ━━━━━━━━━━━━━━━━ VCC
GND ━━━━━━━━━━━━━━━━━ GND
```

## 🎮 控制协议

### CAN消息映射

| 消息类型 | CAN ID | 数据格式 | 描述 |
|----------|--------|----------|------|
| 情绪控制 | 0x789 | [emotion_id] | 控制LED灯带动画效果 |
| LED控制 | 0x456 | [state] | 控制板载LED状态 |
| 木鱼敲击 | 0x123 | [1] | 木鱼敲击事件通知 |

### 情绪状态对应的动画效果

| 状态ID | 情绪名称 | 主控端命令 | 灯光效果 | 特点 |
|--------|----------|------------|----------|------|
| 1 | 开心 | EMOTION:1 | 🌈 彩虹效果 | 流动的彩虹色带，50ms更新 |
| 2 | 伤心 | EMOTION:2 | ⚡ 闪电效果 | 随机蓝白色闪电，80ms更新 |
| 3 | 惊讶 | EMOTION:3 | 💜 紫色追逐 | 紫色光带追逐，60ms更新 |
| 4 | 中性 | EMOTION:4 | 🫁 呼吸灯切换颜色 | 4色循环呼吸，30ms更新 |
| 0 | 关闭 | EMOTION:0 | ⚫ 关闭 | 所有LED熄灭 |

## 🚀 快速部署

### 1. 烧录固件

**主控端 (espcan-master-muyu)**:
```bash
cd espcan-master-muyu
pio build
pio upload
```

**灯光端 (espcan-light-12V-sk6812grbw)**:
```bash
cd espcan-light-12V-sk6812grbw
pio build
pio upload
```

### 2. 验证CAN通信

**主控端监控**:
```bash
pio device monitor -p COM_PORT_1
```

**灯光端监控**:
```bash
pio device monitor -p COM_PORT_2
```

预期输出：
```
[主控端] I MASTER_MUYU: CAN发送端初始化完成
[灯光端] I ESPCAN_SK6812: CAN接收端初始化完成
```

### 3. 测试基本功能

通过主控端串口发送测试命令：
```bash
EMOTION:1    # 测试彩虹效果
EMOTION:2    # 测试闪电效果  
EMOTION:3    # 测试紫色追逐
EMOTION:4    # 测试呼吸灯
EMOTION:0    # 关闭效果
```

## 🎭 TouchDesigner集成

### 串口通信设置
- **波特率**: 115200
- **数据位**: 8
- **停止位**: 1
- **奇偶校验**: 无

### 发送表情命令示例
```python
# TouchDesigner Python脚本
def sendEmotion(emotion_type):
    commands = {
        '开心': 'EXPRESSION:HAPPY',
        '伤心': 'EXPRESSION:SAD', 
        '惊讶': 'EXPRESSION:SURPRISE',
        '中性': 'EXPRESSION:NEUTRAL'
    }
    
    cmd = commands.get(emotion_type, 'EXPRESSION:NEUTRAL')
    op('serial1').send(cmd + '\n')
    print(f'📤 发送命令: {cmd}')
```

### 接收木鱼事件示例
```python
def onSerialReceive(data):
    if '木鱼被敲击' in data:
        # 触发相应的视觉效果
        op('trigger_effect').pulse()
        print('🥢 检测到木鱼敲击!')
```

## 🧪 完整测试流程

### 1. 硬件测试
1. **CAN通信测试**: 主控端发送`LED:1`，观察灯光端板载LED是否点亮
2. **基础动画测试**: 依次发送`EMOTION:1-4`，观察对应的动画效果
3. **木鱼传感器测试**: 发送`WOODFISH_TEST`模拟敲击事件

### 2. TouchDesigner测试
1. **串口连接**: 确认TouchDesigner能接收主控端的欢迎消息
2. **表情控制**: 通过TouchDesigner发送表情命令
3. **事件反馈**: 敲击木鱼，观察TouchDesigner是否收到事件

### 3. 动画效果验证

**彩虹效果检查点**:
- [ ] 颜色从红→绿→蓝平滑过渡
- [ ] 彩虹带在LED条上流动
- [ ] 没有颜色跳跃或闪烁

**闪电效果检查点**:
- [ ] 随机位置出现蓝白色闪电
- [ ] 闪电周围有蓝色光晕
- [ ] 偶尔出现黑暗期

**紫色追逐检查点**:
- [ ] 紫色光带在LED条上移动
- [ ] 光带长度约8个LED
- [ ] 尾部亮度逐渐衰减

**呼吸灯检查点**:
- [ ] 亮度平滑渐变
- [ ] 颜色自动切换：暖白→红→绿→蓝
- [ ] 每种颜色持续约10秒

## 🔧 参数调整

### LED数量调整
修改 `espcan-light-12V-sk6812grbw/src/main.c`:
```c
#define WS2812_LEDS_COUNT 200  // 改为实际LED数量
```

### 动画速度调整
修改延时参数：
```c
rainbow_effect_grbw(50);        // 彩虹效果速度
blue_lightning_effect_grbw(80); // 闪电效果速度  
purple_chase_effect_grbw(60);   // 追逐效果速度
breathing_light_effect_grbw(30);// 呼吸灯速度
```

### 木鱼传感器灵敏度
修改 `espcan-master-muyu/src/main.c`:
```c
#define WOODEN_FISH_DEBOUNCE_MS 50  // 消抖时间，减小=更灵敏
```

## 🐛 故障排除

### CAN通信失败
1. **检查连线**: 确认CAN_H/CAN_L没有接反
2. **检查终端电阻**: 总线两端各需一个120Ω电阻
3. **检查波特率**: 两端都使用500Kbps
4. **检查CAN模块**: 确认模块供电正常

### 动画效果异常
1. **LED供电**: 确认12V电源容量足够
2. **数据线**: 检查GPIO18到LED条的连接
3. **LED数量**: 确认代码中的数量设置正确
4. **时序问题**: 检查RMT时钟配置

### 木鱼检测问题
1. **传感器连接**: 检查GPIO22/23连接
2. **传感器供电**: 确认传感器电源电压正确
3. **灵敏度调整**: 减小消抖时间或调整硬件灵敏度
4. **测试模式**: 使用`WOODFISH_TEST`命令验证功能

### TouchDesigner通信问题
1. **COM端口**: 确认选择正确的串口
2. **波特率**: 确认设置为115200
3. **命令格式**: 确认命令末尾有换行符`\n`
4. **缓冲区**: 检查TouchDesigner串口缓冲区设置

## 📊 性能指标

| 指标 | 数值 | 说明 |
|------|------|------|
| CAN波特率 | 500Kbps | 标准工业控制速率 |
| LED更新频率 | 30-80ms | 根据效果类型变化 |
| 木鱼响应时间 | <100ms | 包含消抖处理 |
| 最大LED数量 | 200个 | 可根据内存调整 |
| 串口波特率 | 115200 | 标准USB串口速率 |

## 📝 开发扩展

### 添加新的动画效果
1. 在 `sk6812_functions.c` 中实现新效果函数
2. 在 `emotion_animation_task` 中添加新的case
3. 在主控端添加对应的情绪ID

### 集成更多传感器
1. 在主控端添加新的GPIO配置
2. 创建相应的检测任务
3. 定义新的CAN消息类型

### 扩展TouchDesigner功能
1. 添加更复杂的命令解析
2. 实现双向通信协议
3. 添加系统状态反馈

---

**🎉 恭喜！您现在拥有了一个完整的木鱼互动灯光系统！**

通过这个整合系统，您可以：
- 🥢 检测真实的木鱼敲击
- 🎮 通过TouchDesigner控制表情
- 🌈 显示丰富的动态灯光效果
- 📡 实现可靠的CAN总线通信 