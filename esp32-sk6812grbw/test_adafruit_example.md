# 测试 Adafruit 风格示例

这份指南帮助你快速测试 `examples/adafruit_style_example.c` 示例程序。

## 🚀 快速测试步骤

### 1. 修改主程序文件
将 `examples/adafruit_style_example.c` 的内容复制到 `src/main.c`：

```bash
# Windows
copy examples\adafruit_style_example.c src\main.c

# Linux/macOS  
cp examples/adafruit_style_example.c src/main.c
```

### 2. 编译和上传
使用 VS Code + PlatformIO：
- 点击 ✓ 编译
- 点击 → 上传
- 点击 🔌 打开串口监视器

或使用命令行：
```bash
pio run --target upload --target monitor
```

### 3. 硬件连接确认
```
ESP32 GPIO 18  ---> SK6812 DIN
ESP32 GND      ---> SK6812 GND  
ESP32 3.3V/5V  ---> SK6812 VCC
```

## 🎮 测试交互

启动后你将看到：
```
=== ESP32 SK6812 GRBW 控制程序 ===
输入命令控制效果:
1: 彩虹流水效果
2: 闪烁效果
3: 跑马灯效果
4: 呼吸灯效果
5: 彩色追逐效果
+: 加速
-: 减速
当前速度: 50 ms
```

### 测试序列

1. **按 `1`** - 观察彩虹流水效果
   - 应该看到平滑的彩虹色沿灯带流动

2. **按 `2`** - 观察闪烁效果  
   - 所有LED应该同时闪烁随机颜色

3. **按 `3`** - 观察跑马灯效果
   - 单个绿色+白色LED点依次追逐

4. **按 `4`** - 观察呼吸灯效果
   - 暖白色应该缓慢渐变明暗

5. **按 `5`** - 观察彩色追逐效果
   - 5个连续LED以不同颜色追逐

6. **按 `+`** - 测试加速
   - 效果应该变快，串口显示新速度

7. **按 `-`** - 测试减速
   - 效果应该变慢，串口显示新速度

## 🔧 参数调整

在代码中可以调整：

```c
#define BRIGHTNESS  64     // 亮度 (0-255)
#define NUM_LEDS    LED_STRIP_LED_NUM  // LED数量
```

## 🐛 故障排除

| 问题 | 解决方案 |
|------|----------|
| 串口无响应 | 检查波特率是否为115200 |
| 灯带不亮 | 检查硬件连接和电源 |
| 颜色异常 | 确认使用SK6812 GRBW灯带 |
| 效果不流畅 | 降低LED数量或增加延时 |

## 💡 扩展想法

基于这个示例，你可以：

1. **添加新效果**：
   ```c
   case 6: myCustomEffect(); break;
   ```

2. **添加更多控制命令**：
   ```c
   case 'b': changeBrightness(); break;
   case 'c': changeColor(); break;
   ```

3. **添加网络控制**：
   - WiFi 配置界面
   - HTTP API 控制
   - MQTT 消息控制

4. **添加传感器响应**：
   - 声音响应
   - 温度变化
   - 运动检测

## 🎯 完成标志

测试成功的标志：
- ✅ 所有5种效果都能正常显示
- ✅ 串口命令响应及时
- ✅ 速度调节功能正常
- ✅ 颜色显示正确（特别是白色通道）
- ✅ 无闪烁或抖动现象

如果上述测试都通过，说明你的 SK6812 GRBW 控制系统已经工作正常！ 