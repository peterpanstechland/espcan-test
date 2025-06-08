#!/usr/bin/env python3
"""
ESP32 木鱼灯光系统整合测试脚本
用于快速验证 espcan-master-muyu 和 espcan-light-12V-sk6812grbw 的同步控制功能
"""

import serial
import time
import sys

class WoodenFishLightTester:
    def __init__(self, port='COM3', baudrate=115200):
        """
        初始化测试器
        
        Args:
            port: ESP32主控端的串口号
            baudrate: 波特率 (默认115200)
        """
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        
    def connect(self):
        """连接串口"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=2)
            time.sleep(2)  # 等待ESP32初始化
            print(f"✅ 成功连接到 {self.port}")
            return True
        except Exception as e:
            print(f"❌ 连接失败: {e}")
            return False
    
    def send_command(self, command):
        """发送命令并接收响应"""
        if not self.ser:
            print("❌ 串口未连接")
            return False
            
        try:
            # 发送命令
            cmd_with_newline = command + '\n'
            self.ser.write(cmd_with_newline.encode())
            print(f"📤 发送命令: {command}")
            
            # 读取响应
            time.sleep(0.5)
            while self.ser.in_waiting:
                response = self.ser.readline().decode().strip()
                if response:
                    print(f"📥 响应: {response}")
            
            return True
        except Exception as e:
            print(f"❌ 发送命令失败: {e}")
            return False
    
    def test_basic_led_control(self):
        """测试基础LED控制"""
        print("\n🔧 测试基础LED控制...")
        
        # 开启LED
        self.send_command("LED:1")
        time.sleep(1)
        
        # 关闭LED
        self.send_command("LED:0")
        time.sleep(1)
        
        print("✅ 基础LED控制测试完成")
    
    def test_emotion_effects(self):
        """测试所有情绪灯光效果"""
        print("\n🎭 测试情绪灯光效果...")
        
        emotions = [
            ("开心 (彩虹效果)", "EMOTION:1"),
            ("伤心 (闪电效果)", "EMOTION:2"), 
            ("惊讶 (紫色追逐)", "EMOTION:3"),
            ("中性 (呼吸灯切换颜色)", "EMOTION:4")
        ]
        
        for name, command in emotions:
            print(f"\n🎨 测试 {name}")
            self.send_command(command)
            time.sleep(5)  # 观察效果5秒
            
        # 关闭所有效果
        print(f"\n⚫ 关闭所有效果")
        self.send_command("EMOTION:0")
        time.sleep(2)
        
        print("✅ 情绪灯光效果测试完成")
    
    def test_touchdesigner_expressions(self):
        """测试TouchDesigner表情命令"""
        print("\n🎭 测试TouchDesigner表情命令...")
        
        expressions = [
            ("开心表情", "EXPRESSION:HAPPY"),
            ("伤心表情", "EXPRESSION:SAD"),
            ("惊讶表情", "EXPRESSION:SURPRISE"),
            ("中性表情", "EXPRESSION:NEUTRAL")
        ]
        
        for name, command in expressions:
            print(f"\n✨ 测试 {name}")
            self.send_command(command)
            time.sleep(3)  # 观察效果3秒
            
        print("✅ TouchDesigner表情命令测试完成")
    
    def test_wooden_fish_simulation(self):
        """测试木鱼敲击模拟"""
        print("\n🥢 测试木鱼敲击模拟...")
        
        for i in range(3):
            print(f"\n🎯 模拟第 {i+1} 次木鱼敲击")
            self.send_command("WOODFISH_TEST")
            time.sleep(2)
            
        print("✅ 木鱼敲击模拟测试完成")
    
    def test_random_effects(self):
        """测试随机效果"""
        print("\n🎲 测试随机效果...")
        
        # 启动随机效果
        self.send_command("RANDOM:1:150:200")
        time.sleep(5)
        
        # 停止随机效果
        self.send_command("RANDOM:0")
        time.sleep(2)
        
        print("✅ 随机效果测试完成")
    
    def run_full_test(self):
        """运行完整测试流程"""
        print("🚀 开始木鱼灯光系统完整测试\n")
        
        if not self.connect():
            return False
        
        try:
            # 等待系统初始化
            print("⏳ 等待系统初始化...")
            time.sleep(3)
            
            # 执行各项测试
            self.test_basic_led_control()
            self.test_emotion_effects()
            self.test_touchdesigner_expressions()
            self.test_wooden_fish_simulation()
            self.test_random_effects()
            
            print("\n🎉 所有测试完成！")
            print("\n📋 测试结果验证清单:")
            print("- [ ] 板载LED正常开关")
            print("- [ ] 彩虹效果：流动的彩虹色带")
            print("- [ ] 闪电效果：随机蓝白色闪电")
            print("- [ ] 紫色追逐：紫色光带移动")
            print("- [ ] 呼吸灯：渐变切换颜色")
            print("- [ ] TouchDesigner命令正常响应")
            print("- [ ] 木鱼敲击事件正常发送")
            print("- [ ] 随机效果正常工作")
            
            return True
            
        except KeyboardInterrupt:
            print("\n⏹️  测试被用户中断")
            return False
        except Exception as e:
            print(f"\n❌ 测试过程中出现错误: {e}")
            return False
        finally:
            if self.ser:
                self.ser.close()
                print("🔌 串口连接已关闭")
    
    def interactive_mode(self):
        """交互模式 - 手动发送命令"""
        print("🎮 进入交互模式")
        print("输入命令 (输入 'quit' 退出):")
        print("示例命令: EMOTION:1, LED:1, WOODFISH_TEST")
        
        if not self.connect():
            return
        
        try:
            while True:
                command = input("\n> ").strip()
                
                if command.lower() in ['quit', 'exit', 'q']:
                    break
                
                if command:
                    self.send_command(command)
                    
        except KeyboardInterrupt:
            print("\n⏹️  交互模式退出")
        finally:
            if self.ser:
                self.ser.close()
                print("🔌 串口连接已关闭")

def main():
    """主函数"""
    print("🥢 ESP32 木鱼灯光系统测试工具")
    print("=" * 50)
    
    # 获取串口号
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = input("请输入ESP32主控端串口号 (例如: COM3 或 /dev/ttyUSB0): ").strip()
        if not port:
            port = "COM3"  # 默认值
    
    tester = WoodenFishLightTester(port)
    
    # 选择测试模式
    print(f"\n使用串口: {port}")
    print("选择测试模式:")
    print("1. 自动完整测试")
    print("2. 交互模式")
    
    try:
        choice = input("\n请选择 (1 或 2): ").strip()
        
        if choice == "1":
            tester.run_full_test()
        elif choice == "2":
            tester.interactive_mode()
        else:
            print("❌ 无效选择")
            
    except KeyboardInterrupt:
        print("\n👋 程序退出")

if __name__ == "__main__":
    main() 