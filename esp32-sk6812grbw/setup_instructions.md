# 开发环境设置指南

## 安装 PlatformIO

### 方法 1: 通过 VS Code 扩展安装
1. 安装 Visual Studio Code
2. 在 VS Code 扩展市场搜索并安装 "PlatformIO IDE"
3. 重启 VS Code

### 方法 2: 通过命令行安装
```bash
# 安装 Python (如果没有的话)
# 下载并安装 Python 3.8+ 从 https://python.org

# 安装 PlatformIO Core
pip install platformio

# 验证安装
pio --version
```

## 安装 ESP-IDF (可选，推荐)

### Windows 安装方法：
1. 下载 ESP-IDF Windows Installer
2. 从 [ESP-IDF 官网](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html) 下载
3. 按照安装向导完成安装

### 使用 VS Code + ESP-IDF 扩展：
1. 在 VS Code 中安装 "ESP-IDF" 扩展
2. 按 Ctrl+Shift+P，搜索 "ESP-IDF: Configure ESP-IDF Extension"
3. 选择安装方式并完成配置

## 编译和运行项目

### 使用 PlatformIO：
```bash
# 进入项目目录
cd esp32-sk6812grbw

# 编译项目
pio run

# 上传到设备
pio run --target upload

# 监控串口输出
pio run --target monitor
```

### 使用 ESP-IDF：
```bash
# 进入项目目录
cd esp32-sk6812grbw

# 配置项目
idf.py menuconfig

# 编译项目
idf.py build

# 烧录到设备
idf.py flash

# 监控串口
idf.py monitor
```

## 硬件连接

确保正确连接硬件：

1. **ESP32 连接**：
   - 使用 USB 数据线连接 ESP32 到电脑
   - 确保驱动程序已正确安装

2. **SK6812 灯带连接**：
   ```
   ESP32          SK6812 灯带
   -----          -----------
   GPIO 18   ---> DIN (数据输入)
   3.3V/5V   ---> VCC (电源正极)
   GND       ---> GND (电源负极)
   ```

3. **电源注意事项**：
   - 对于多个 LED，建议使用外部 5V 电源
   - 确保共地连接
   - 大功率灯带需要独立供电

## 配置修改

在 `platformio.ini` 中修改以下参数：

```ini
build_flags = 
    -DLED_STRIP_RMT_GPIO=18      # 修改数据引脚
    -DLED_STRIP_LED_NUM=30       # 修改LED数量
```

## 故障排除

1. **编译错误**：
   - 确保 ESP-IDF 版本兼容 (推荐 v5.0+)
   - 检查依赖库是否正确安装

2. **上传失败**：
   - 检查串口权限
   - 确认 ESP32 连接正常
   - 尝试按住 BOOT 按钮再上传

3. **灯带不亮**：
   - 检查电源连接
   - 确认引脚配置正确
   - 验证灯带型号 (必须是 SK6812 GRBW)

## 进一步开发

项目结构清晰，可以在以下基础上扩展：

- `src/main.c`: 修改主程序逻辑
- `components/sk6812/`: 修改驱动实现
- 添加新的效果函数
- 集成网络控制 (WiFi/蓝牙)
- 添加传感器输入响应 