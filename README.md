# 木鱼互动装置 - 基于ESP32的分布式CAN总线控制系统

这个项目是一个基于ESP32的分布式控制系统，使用CAN总线进行通信，实现了一套完整的木鱼互动装置。该装置通过木鱼敲击触发不同的声光电效果，展示不同的情绪状态反馈。

## 系统架构

系统由以下几个模块组成，每个模块都是一个独立的ESP32开发板：

- **espcan-master-muyu**: 木鱼主控模块，负责检测木鱼敲击事件并发送控制命令到其他模块
- **espcan-light**: 灯光控制模块，负责根据情绪状态显示不同的灯光效果
- **espcan-sound**: 声音控制模块，播放不同情绪状态对应的音效
- **espcan-motor**: 电机控制模块，控制电机速度和模式
- **espcan-fogger**: 雾化器控制模块，根据情绪状态控制雾化效果
- **td-tester**: 测试模拟器，用于模拟木鱼敲击和情绪状态控制

## 情绪状态映射

系统支持以下情绪状态：

1. **开心(EMOTION_HAPPY=1)**:
   - 灯光: 彩虹灯效
   - 声音: 开心音效(GPIO18)
   - 电机: 中速渐变
   
2. **伤心(EMOTION_SAD=2)**:
   - 灯光: 紫色追逐
   - 声音: 小雨点音效(GPIO22)
   - 雾化器: 开启
   
3. **惊讶(EMOTION_SURPRISE=3)**:
   - 灯光: 蓝色闪电
   - 声音: 打雷闪电音效(GPIO23)
   - 电机: 高速
   
4. **随机(EMOTION_RANDOM=4)**:
   - 灯光: 呼吸灯效果
   - 声音: 随机音效(GPIO17)
   - 电机: 渐变速度
   
5. **木鱼敲击(WOODFISH_HIT=5)**:
   - 声音: 木鱼敲击音效(GPIO19)

## 通信协议

所有模块通过CAN总线通信，使用以下消息ID：

- 0x123 (WOODEN_FISH_HIT_ID): 木鱼敲击事件
- 0x456 (LED_CMD_ID): LED控制命令
- 0x789 (EMOTION_CMD_ID): 情绪状态命令
- 0xABC (RANDOM_CMD_ID): 随机效果命令
- 0x301 (MOTOR_CMD_ID): 电机控制命令
- 0x321 (FOGGER_CMD_ID): 雾化器控制命令

## 开发环境

- ESP32开发板
- ESP-IDF框架
- PlatformIO开发环境

## 硬件配置

- CAN总线通信使用TJA1050或SN65HVD230等CAN收发器
- 每个模块使用独立的ESP32连接相应的外设
- 木鱼主控使用振动传感器和声音传感器检测敲击

## 开发者

- Peter Pan's Tech Land
- 项目地址: [GitHub](https://github.com/peterpanstechland/espcan-test)

## 许可证

MIT License 