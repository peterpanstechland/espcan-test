# ESP32 SK6812 GRBW 灯带控制项目

这是一个基于 PlatformIO 和 ESP-IDF 框架的 ESP32 项目，用于控制 SK6812 GRBW (绿-红-蓝-白) 灯带。

## 功能特性

- ✅ 支持 SK6812 GRBW 四通道灯带
- ✅ 使用 ESP32 RMT 外设进行精确时序控制
- ✅ 自定义 SK6812 驱动组件，避免官方 led_strip 组件的兼容性问题
- ✅ 多种灯光效果演示：
  - GRBW 基础颜色测试
  - 彩虹效果
  - 白色呼吸灯
  - 流水灯效果
  - 随机闪烁效果

## 硬件要求

- ESP32 开发板 (ESP32DevKit)
- SK6812 GRBW 灯带
- 适当的电源供应（根据灯带功率需求）

## 引脚配置

默认配置：
- **GPIO 18**: 数据引脚（连接到 SK6812 灯带的 DIN）
- **LED 数量**: 30个（可在 `platformio.ini` 中修改）

## 🚀 快速开始

### 方法 1: 使用 VS Code + PlatformIO 扩展 (推荐)

1. **安装 VS Code**
   - 下载并安装 [Visual Studio Code](https://code.visualstudio.com/)

2. **安装 PlatformIO IDE 扩展**
   - 打开 VS Code
   - 点击左侧扩展图标 (或按 `Ctrl+Shift+X`)
   - 搜索 "PlatformIO IDE"
   - 点击安装

3. **打开项目**
   - 在 VS Code 中打开项目文件夹
   - PlatformIO 会自动检测项目

4. **编译和上传**
   - 点击底部状态栏的 ✓ 图标编译
   - 点击 → 图标上传到 ESP32
   - 点击 🔌 图标监控串口输出

### 方法 2: 使用命令行 PlatformIO

```bash
# 安装 PlatformIO Core
pip install platformio

# 编译项目
pio run

# 上传到设备
pio run --target upload

# 监控串口输出
pio run --target monitor
```

### 方法 3: 使用 ESP-IDF (原生)

```bash
# 安装 ESP-IDF (参考官方文档)
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

# 配置项目
idf.py menuconfig

# 编译项目
idf.py build

# 烧录到设备
idf.py flash

# 监控串口
idf.py monitor
```

## 📁 项目结构

```
esp32-sk6812grbw/
├── platformio.ini              # PlatformIO 配置文件
├── CMakeLists.txt              # ESP-IDF 项目配置
├── src/                        # 主程序源码
│   ├── main.c                 # 高级 API 演示程序
│   └── CMakeLists.txt         
├── components/sk6812/          # 自定义 SK6812 组件
│   ├── include/sk6812.h       # 头文件
│   ├── sk6812.c               # 驱动实现
│   └── CMakeLists.txt         
├── examples/                   # 示例代码
│   ├── compatible_example.c   # 兼容官方 API 风格
│   ├── legacy_style_example.c # 传统底层 RMT 风格
│   └── adafruit_style_example.c # Adafruit_NeoPixel 风格
├── README.md                  # 本文件
├── setup_instructions.md      # 详细环境设置
└── .gitignore                # Git 忽略文件
```

## 💡 代码示例

### 高级 API 使用 (推荐)

```c
#include "sk6812.h"

sk6812_config_t config = {
    .gpio_num = 18,
    .led_count = 30, 
    .resolution_hz = 10 * 1000 * 1000,
};

sk6812_handle_t led_strip;
sk6812_new(&config, &led_strip);
sk6812_enable(led_strip);

// 设置颜色 (G, R, B, W)
sk6812_set_pixel_grbw(led_strip, 0, 0, 0, 100, 200);  // 蓝色+白色
sk6812_refresh(led_strip);
```

### 兼容官方 API 风格

参考 `examples/compatible_example.c`：
```c
// 和 ESP-IDF led_strip 组件一样的调用方式
led_strip_set_pixel_grbw_compat(i, 0x00, 0x00, 0x10, 0x20);
led_strip_refresh_compat();
```

### 底层 RMT 控制风格

参考 `examples/legacy_style_example.c`：
- 手动构建 RMT 符号数组
- 直接控制时序参数
- 完全自定义数据格式

### Adafruit_NeoPixel 风格

参考 `examples/adafruit_style_example.c`：
```c
// 熟悉的 Adafruit API 风格
strip_begin();
strip_setBrightness(64);
strip_setPixelColor(0, strip_Color(255, 0, 0, 100)); // RGBW
strip_show();

// 串口交互控制：
// 1-5: 切换效果  +/-: 调速度
```

## ⚙️ 配置修改

在 `platformio.ini` 中修改参数：

```ini
build_flags = 
    -DLED_STRIP_RMT_GPIO=18      # 修改数据引脚
    -DLED_STRIP_LED_NUM=30       # 修改LED数量
```

支持的 GPIO 引脚：任何空闲的数字 IO 引脚

## 🎨 内置效果

### 主程序 (`src/main.c`) 包含：
1. **GRBW 基础测试** - 分别测试绿、红、蓝、白四个通道
2. **彩虹效果** - 流动的彩虹色彩
3. **白色呼吸灯** - 纯白光的渐变效果
4. **彩色流水灯** - 多彩追逐效果
5. **随机闪烁** - 星空般的随机闪烁

### Adafruit 风格示例 (`examples/adafruit_style_example.c`) 包含：
1. **彩虹流水效果** - 平滑的彩虹色流动
2. **闪烁效果** - 随机颜色全屏闪烁
3. **跑马灯效果** - 绿色+白色单点追逐
4. **呼吸灯效果** - 暖白色渐变呼吸
5. **彩色追逐效果** - 多色连续追逐

**串口控制命令**：
- `1-5`: 切换效果
- `+`: 加速 (最快20ms)
- `-`: 减速 (最慢500ms)

### 示例程序功能：
- `compatible_example.c`: 展示如何从现有代码迁移
- `legacy_style_example.c`: 底层 RMT 控制，适合需要精确控制的场景  
- `adafruit_style_example.c`: **[推荐]** Adafruit_NeoPixel 风格，支持串口交互控制

## 🔧 时序参数

本项目针对 SK6812 进行了优化：
- **T0H**: 300ns (0码高电平)
- **T0L**: 900ns (0码低电平)
- **T1H**: 600ns (1码高电平)
- **T1L**: 600ns (1码低电平)
- **Reset**: 80μs (复位时间)
- **时钟频率**: 10MHz (0.1μs 分辨率)

## 📝 API 参考

### 主要函数

```c
// 创建和销毁
esp_err_t sk6812_new(const sk6812_config_t *config, sk6812_handle_t *handle);
esp_err_t sk6812_del(sk6812_handle_t handle);

// 控制函数
esp_err_t sk6812_enable(sk6812_handle_t handle);
esp_err_t sk6812_disable(sk6812_handle_t handle);

// 设置像素
esp_err_t sk6812_set_pixel_grbw(sk6812_handle_t handle, uint16_t index, 
                                uint8_t g, uint8_t r, uint8_t b, uint8_t w);
esp_err_t sk6812_clear(sk6812_handle_t handle);
esp_err_t sk6812_refresh(sk6812_handle_t handle);
```

## 🏠 硬件连接

```
ESP32 DevKit     SK6812 灯带      电源
-------------    -----------      -----
GPIO 18     ---> DIN             
3.3V        ---> VCC (短灯带)     5V (长灯带)
GND         ---> GND         ---> GND
                              ---> VCC (外部电源)
```

**重要提示**：
- 短灯带 (≤10个LED) 可以用 ESP32 的 3.3V 供电
- 长灯带需要外部 5V 电源，并确保共地连接
- 大功率应用建议使用电平转换器将 3.3V 信号转换为 5V

## 🐛 故障排除

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| 灯带不亮 | 电源/连接问题 | 检查电源和接线，确认 VCC、GND、DIN 连接正确 |
| 颜色错误 | 数据格式不匹配 | 确认使用 GRBW 格式，不是 RGB 或 RGBW |
| 闪烁抖动 | 电源不稳定 | 使用稳定的电源，添加滤波电容 |
| 编译错误 | 环境配置问题 | 参考 `setup_instructions.md` |
| 部分LED不响应 | 数量配置错误 | 检查 `LED_STRIP_LED_NUM` 配置 |

## 📚 参考资料

- [SK6812-RGBW-ESP32 GitHub 项目](https://github.com/gitpeut/SK6812-RGBW-ESP32)
- [ESP32 RMT 外设文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/rmt.html)
- [SK6812 数据手册](https://www.adafruit.com/product/2757)
- [PlatformIO ESP32 开发指南](https://docs.platformio.org/en/latest/platforms/espressif32.html)

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

本项目基于 Apache-2.0 许可证开源。 