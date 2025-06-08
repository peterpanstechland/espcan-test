# 🎮 与主控端同步控制说明

本项目 `espcan-light-12V-sk6812grbw` 作为CAN总线从设备，配合 `espcan-master-muyu` 主控端使用，实现完整的木鱼互动灯光系统。

## 🔗 项目关系

```
espcan-master-muyu (主控端)  ──CAN总线──>  espcan-light-12V-sk6812grbw (灯光端)
       ↑                                            ↓
TouchDesigner/木鱼传感器                    SK6812 GRBW LED灯带
```

## ⚡ 快速启动

### 1. 确保两个项目都已烧录
- **主控端**: `espcan-master-muyu` 
- **灯光端**: `espcan-light-12V-sk6812grbw` (本项目)

### 2. 连接CAN总线
确保两个ESP32通过CAN总线正确连接 (GPIO 4/5)

### 3. 测试基本功能
在主控端串口发送测试命令：
```bash
EMOTION:1  # 彩虹效果
EMOTION:2  # 闪电效果  
EMOTION:3  # 紫色追逐
EMOTION:4  # 呼吸灯切换颜色
```

## 🎨 支持的动画效果

| 主控端命令 | 动画效果 | 描述 |
|-----------|----------|------|
| `EMOTION:1` | 🌈 彩虹效果 | 流动的彩虹色带 |
| `EMOTION:2` | ⚡ 闪电效果 | 随机蓝白色闪电 |
| `EMOTION:3` | 💜 紫色追逐 | 紫色光带追逐移动 |
| `EMOTION:4` | 🫁 呼吸灯切换颜色 | 4色循环呼吸 |
| `EMOTION:0` | ⚫ 关闭 | 所有LED熄灭 |

## 📡 通信协议

### CAN消息格式
- **消息ID**: 0x789
- **数据长度**: 1字节
- **数据内容**: 情绪状态ID (1-4)

### TouchDesigner表情命令
主控端也支持TouchDesigner格式的表情命令：
- `EXPRESSION:HAPPY` → 彩虹效果
- `EXPRESSION:SAD` → 闪电效果
- `EXPRESSION:SURPRISE` → 紫色追逐
- `EXPRESSION:NEUTRAL` → 呼吸灯

## 🔧 配置参数

### LED数量调整
修改 `src/main.c` 中的LED数量：
```c
#define WS2812_LEDS_COUNT 200  // 改为您的实际LED数量
```

### 动画速度调整
修改各效果函数的延时参数：
```c
rainbow_effect_grbw(50);        // 彩虹: 50ms/帧
blue_lightning_effect_grbw(80); // 闪电: 80ms/帧
purple_chase_effect_grbw(60);   // 追逐: 60ms/帧  
breathing_light_effect_grbw(30);// 呼吸: 30ms/帧
```

## 🧪 完整测试

### 自动测试
使用主控端提供的Python测试脚本：
```bash
cd ../espcan-master-muyu
python test_integration.py COM3
```

### 手动测试
1. 监控灯光端输出：
   ```bash
   pio device monitor
   ```

2. 在主控端串口发送命令并观察效果

## 📋 验证检查点

### 彩虹效果 ✅
- [ ] 颜色从红→绿→蓝平滑过渡
- [ ] 彩虹带在LED条上流动
- [ ] 没有颜色跳跃或闪烁

### 闪电效果 ⚡
- [ ] 随机位置出现蓝白色闪电
- [ ] 闪电周围有蓝色光晕  
- [ ] 偶尔出现黑暗期增加戏剧效果

### 紫色追逐 💜
- [ ] 紫色光带在LED条上移动
- [ ] 光带长度约8个LED
- [ ] 尾部亮度逐渐衰减

### 呼吸灯 🫁
- [ ] 亮度平滑地渐亮渐暗
- [ ] 颜色自动切换：暖白→红→绿→蓝
- [ ] 每种颜色持续约10秒

## 🐛 故障排除

### LED不亮
1. 检查12V电源是否充足
2. 确认GPIO18数据线连接
3. 验证LED数量配置正确

### 动画不流畅
1. 检查电源稳定性
2. 减少LED数量测试
3. 调整动画延时参数

### CAN通信失败
1. 检查CAN总线连接
2. 确认波特率500Kbps
3. 检查终端电阻120Ω

## 📖 完整文档

详细的整合指南请参考：
- **主控端项目**: `../espcan-master-muyu/INTEGRATION_GUIDE.md`
- **测试脚本**: `../espcan-master-muyu/test_integration.py`

## 🚀 系统特性

- ✅ **实时响应**: CAN消息<100ms响应时间
- ✅ **动态效果**: 4种精美的LED动画
- ✅ **GRBW支持**: 充分利用4通道颜色
- ✅ **大容量**: 支持200个LED（可配置）
- ✅ **稳定通信**: 500Kbps CAN总线
- ✅ **木鱼集成**: 敲击事件自动响应

---

**🎉 享受您的木鱼互动灯光系统！** 