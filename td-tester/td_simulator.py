#!/usr/bin/env python3
"""
TouchDesigner 模拟器 - 用于向ESP32发送控制命令
此程序模拟TouchDesigner向ESP32发送CAN控制信号的功能
"""

import serial
import time
import tkinter as tk
from tkinter import ttk, messagebox, StringVar, IntVar
import threading
import sys
import glob
import os
import re

# 自动检测可用串口
def list_serial_ports():
    """列出所有可用的串口"""
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # 排除USB转串口的/dev/ttyS*
        ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*') + glob.glob('/dev/cu.*')
    else:
        raise EnvironmentError('不支持的平台')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result

# 主应用类
class TouchDesignerSimulator:
    def __init__(self, root):
        self.root = root
        self.root.title("TouchDesigner 模拟器")
        self.root.geometry("600x800")  # 增加高度以适应木鱼状态区域
        self.root.resizable(True, True)
        
        # 设置主题颜色
        self.root.configure(bg="#2a2a2a")
        
        # 串口状态
        self.serial_connection = None
        self.is_connected = False
        
        # 随机效果参数
        self.random_speed = IntVar(value=128)  # 默认中速
        self.random_brightness = IntVar(value=200)  # 默认高亮度
        
        # 电机控制参数
        self.motor_pwm = IntVar(value=128)  # 默认中速
        self.motor_state = IntVar(value=0)  # 默认关闭
        
        # 雾化器状态
        self.fogger_state = IntVar(value=0)  # 默认关闭
        
        # 木鱼敲击状态
        self.woodfish_hit = IntVar(value=0)  # 默认未敲击
        self.last_hit_time = 0  # 最后一次敲击时间
        
        # 创建菜单
        self.create_menu()
        
        # 创建界面布局
        self.create_widgets()
        
        # 创建控制台区域
        self.create_console()
        
        # 更新串口列表
        self.update_port_list()
        
        # 接收线程
        self.receiver_thread = None
        self.thread_running = False
        
    def create_menu(self):
        """创建菜单栏"""
        menu_bar = tk.Menu(self.root)
        
        # 文件菜单
        file_menu = tk.Menu(menu_bar, tearoff=0)
        file_menu.add_command(label="刷新串口", command=self.update_port_list)
        file_menu.add_separator()
        file_menu.add_command(label="退出", command=self.root.quit)
        menu_bar.add_cascade(label="文件", menu=file_menu)
        
        # 帮助菜单
        help_menu = tk.Menu(menu_bar, tearoff=0)
        help_menu.add_command(label="关于", command=self.show_about)
        menu_bar.add_cascade(label="帮助", menu=help_menu)
        
        self.root.config(menu=menu_bar)
    
    def create_widgets(self):
        """创建主界面控件"""
        # 创建一个主框架
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # 串口设置区域
        port_frame = ttk.LabelFrame(main_frame, text="串口设置", padding="10")
        port_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # 串口选择
        ttk.Label(port_frame, text="端口:").grid(row=0, column=0, sticky=tk.W, padx=5, pady=5)
        self.port_var = StringVar()
        self.port_combo = ttk.Combobox(port_frame, textvariable=self.port_var, width=15)
        self.port_combo.grid(row=0, column=1, sticky=tk.W, padx=5, pady=5)
        
        # 波特率选择
        ttk.Label(port_frame, text="波特率:").grid(row=0, column=2, sticky=tk.W, padx=5, pady=5)
        self.baud_var = StringVar(value="115200")
        baud_combo = ttk.Combobox(port_frame, textvariable=self.baud_var, width=10)
        baud_combo['values'] = ('9600', '19200', '38400', '57600', '115200')
        baud_combo.grid(row=0, column=3, sticky=tk.W, padx=5, pady=5)
        
        # 连接按钮
        self.connect_button = ttk.Button(port_frame, text="连接", command=self.toggle_connection)
        self.connect_button.grid(row=0, column=4, sticky=tk.W, padx=5, pady=5)
        
        # 刷新按钮
        refresh_button = ttk.Button(port_frame, text="刷新", command=self.update_port_list)
        refresh_button.grid(row=0, column=5, sticky=tk.W, padx=5, pady=5)
        
        # 控制命令区域
        control_frame = ttk.LabelFrame(main_frame, text="情绪控制", padding="10")
        control_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # 情绪按钮 - 使用图标或彩色按钮
        self.create_emotion_button(control_frame, "开心 (彩虹)", "EMOTION:1", 0, 0, "#FFD700")  # 金色
        self.create_emotion_button(control_frame, "伤心 (闪电)", "EMOTION:2", 0, 1, "#1E90FF")  # 蓝色
        self.create_emotion_button(control_frame, "惊讶 (追逐)", "EMOTION:3", 0, 2, "#9932CC")  # 紫色
        self.create_emotion_button(control_frame, "随机效果", "EMOTION:4", 0, 3, "#FF6347")     # 橙红色
        
        # 随机效果控制区域
        random_frame = ttk.LabelFrame(main_frame, text="随机效果控制", padding="10")
        random_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # 随机效果启动/停止按钮
        ttk.Button(random_frame, text="启动随机效果", 
                  command=lambda: self.send_random_command(1)).grid(row=0, column=0, padx=10, pady=5)
        ttk.Button(random_frame, text="停止随机效果", 
                  command=lambda: self.send_random_command(0)).grid(row=0, column=1, padx=10, pady=5)
        
        # 速度滑块
        ttk.Label(random_frame, text="速度:").grid(row=1, column=0, sticky=tk.W, padx=5, pady=5)
        speed_scale = ttk.Scale(random_frame, from_=0, to=255, variable=self.random_speed, 
                               orient=tk.HORIZONTAL, length=200)
        speed_scale.grid(row=1, column=1, columnspan=2, padx=5, pady=5, sticky=tk.W+tk.E)
        
        # 亮度滑块
        ttk.Label(random_frame, text="亮度:").grid(row=2, column=0, sticky=tk.W, padx=5, pady=5)
        brightness_scale = ttk.Scale(random_frame, from_=0, to=255, variable=self.random_brightness, 
                                    orient=tk.HORIZONTAL, length=200)
        brightness_scale.grid(row=2, column=1, columnspan=2, padx=5, pady=5, sticky=tk.W+tk.E)
        
        # 应用参数按钮
        ttk.Button(random_frame, text="应用参数", 
                  command=self.apply_random_params).grid(row=3, column=1, padx=10, pady=5)
        
        # LED控制区域
        led_frame = ttk.LabelFrame(main_frame, text="LED控制", padding="10")
        led_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # LED开关按钮
        ttk.Button(led_frame, text="LED开", command=lambda: self.send_command("LED:1")).grid(row=0, column=0, padx=10, pady=5)
        ttk.Button(led_frame, text="LED关", command=lambda: self.send_command("LED:0")).grid(row=0, column=1, padx=10, pady=5)
        
        # 电机控制区域
        motor_frame = ttk.LabelFrame(main_frame, text="电机控制", padding="10")
        motor_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # PWM占空比滑块
        ttk.Label(motor_frame, text="占空比:").grid(row=0, column=0, sticky=tk.W, padx=5, pady=5)
        pwm_scale = ttk.Scale(motor_frame, from_=0, to=255, variable=self.motor_pwm, 
                               orient=tk.HORIZONTAL, length=200)
        pwm_scale.grid(row=0, column=1, columnspan=2, padx=5, pady=5, sticky=tk.W+tk.E)
        
        # 电机启停控制
        ttk.Label(motor_frame, text="电机控制:").grid(row=1, column=0, sticky=tk.W, padx=5, pady=5)
        ttk.Radiobutton(motor_frame, text="停止", variable=self.motor_state, value=0).grid(row=1, column=1, padx=5, pady=5, sticky=tk.W)
        ttk.Radiobutton(motor_frame, text="启动", variable=self.motor_state, value=1).grid(row=1, column=2, padx=5, pady=5, sticky=tk.W)
        
        # 应用电机参数按钮
        ttk.Button(motor_frame, text="应用电机参数", 
                  command=self.apply_motor_params).grid(row=2, column=1, padx=10, pady=5)
        
        # 雾化器控制区域
        fogger_frame = ttk.LabelFrame(main_frame, text="雾化器控制", padding="10")
        fogger_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # 雾化器状态图标 - 使用彩色方块表示当前状态
        self.fogger_status_canvas = tk.Canvas(fogger_frame, width=20, height=20, 
                                             bg="#FF0000", highlightthickness=0)
        self.fogger_status_canvas.grid(row=0, column=0, padx=5, pady=5)
        
        # 状态标签
        self.fogger_status_label = ttk.Label(fogger_frame, text="当前状态: 关闭")
        self.fogger_status_label.grid(row=0, column=1, padx=5, pady=5, sticky=tk.W)
        
        # 雾化器控制按钮
        ttk.Button(fogger_frame, text="开启雾化器", 
                  command=lambda: self.control_fogger(1)).grid(row=1, column=0, padx=10, pady=5)
        ttk.Button(fogger_frame, text="关闭雾化器", 
                  command=lambda: self.control_fogger(0)).grid(row=1, column=1, padx=10, pady=5)
        
        # 木鱼状态区域
        woodfish_frame = ttk.LabelFrame(main_frame, text="木鱼状态", padding="10")
        woodfish_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # 木鱼状态图标 - 使用彩色方块表示当前状态
        self.woodfish_status_canvas = tk.Canvas(woodfish_frame, width=20, height=20, 
                                               bg="#888888", highlightthickness=0)
        self.woodfish_status_canvas.grid(row=0, column=0, padx=5, pady=5)
        
        # 状态标签
        self.woodfish_status_label = ttk.Label(woodfish_frame, text="当前状态: 未敲击")
        self.woodfish_status_label.grid(row=0, column=1, padx=5, pady=5, sticky=tk.W)
        
        # 上次敲击时间标签
        self.woodfish_time_label = ttk.Label(woodfish_frame, text="上次敲击时间: --")
        self.woodfish_time_label.grid(row=1, column=0, columnspan=2, padx=5, pady=5, sticky=tk.W)
        
        # 清除敲击状态按钮
        ttk.Button(woodfish_frame, text="清除状态", 
                  command=self.clear_woodfish_status).grid(row=2, column=0, padx=10, pady=5)
        
        # 自定义命令区域
        custom_frame = ttk.LabelFrame(main_frame, text="自定义命令", padding="10")
        custom_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # 自定义命令输入框
        self.command_var = StringVar()
        command_entry = ttk.Entry(custom_frame, textvariable=self.command_var, width=40)
        command_entry.grid(row=0, column=0, padx=5, pady=5)
        command_entry.bind("<Return>", lambda event: self.send_custom_command())
        
        # 发送按钮
        ttk.Button(custom_frame, text="发送", command=self.send_custom_command).grid(row=0, column=1, padx=5, pady=5)
    
    def create_emotion_button(self, parent, text, command, row, col, color):
        """创建情绪按钮"""
        btn_frame = ttk.Frame(parent)
        btn_frame.grid(row=row, column=col, padx=10, pady=10)
        
        # 创建彩色按钮效果
        color_canvas = tk.Canvas(btn_frame, width=20, height=20, bg=color, highlightthickness=0)
        color_canvas.pack(side=tk.LEFT, padx=5)
        
        # 创建按钮
        ttk.Button(btn_frame, text=text, command=lambda: self.send_command(command)).pack(side=tk.LEFT)
    
    def create_console(self):
        """创建控制台输出区域"""
        console_frame = ttk.LabelFrame(self.root, text="控制台", padding="10")
        console_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # 滚动条
        scrollbar = ttk.Scrollbar(console_frame)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # 文本框
        self.console = tk.Text(console_frame, wrap=tk.WORD, yscrollcommand=scrollbar.set, bg="#000000", fg="#00FF00")
        self.console.pack(fill=tk.BOTH, expand=True)
        scrollbar.config(command=self.console.yview)
        
        # 禁止编辑
        self.console.config(state=tk.DISABLED)
    
    def update_port_list(self):
        """更新可用串口列表"""
        ports = list_serial_ports()
        self.port_combo['values'] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])
        
        if not ports:
            self.log_message("未检测到可用串口")
    
    def toggle_connection(self):
        """切换连接状态"""
        if not self.is_connected:
            self.connect_serial()
        else:
            self.disconnect_serial()
    
    def connect_serial(self):
        """连接串口"""
        port = self.port_var.get()
        baud = int(self.baud_var.get())
        
        if not port:
            messagebox.showerror("错误", "请选择串口")
            return
        
        try:
            self.serial_connection = serial.Serial(port, baud, timeout=1)
            self.is_connected = True
            self.connect_button.config(text="断开")
            self.log_message(f"已连接到 {port} (波特率: {baud})")
            
            # 启动接收线程
            self.thread_running = True
            self.receiver_thread = threading.Thread(target=self.receive_data)
            self.receiver_thread.daemon = True
            self.receiver_thread.start()
            
        except Exception as e:
            messagebox.showerror("连接错误", str(e))
            self.log_message(f"连接错误: {str(e)}")
    
    def disconnect_serial(self):
        """断开串口连接"""
        if self.serial_connection:
            self.thread_running = False
            time.sleep(0.2)  # 给线程一点时间退出
            
            self.serial_connection.close()
            self.serial_connection = None
            self.is_connected = False
            self.connect_button.config(text="连接")
            self.log_message("已断开连接")
    
    def receive_data(self):
        """接收串口数据的线程"""
        while self.thread_running and self.serial_connection:
            try:
                if self.serial_connection.in_waiting > 0:
                    data = self.serial_connection.readline().decode('utf-8', errors='ignore').strip()
                    if data:
                        self.log_message(f"接收: {data}")
                        
                        # 增加调试信息
                        if "WOODEN_FISH_HIT" in data or "木鱼" in data or "敲击" in data:
                            self.log_message(f"⭐ 检测到木鱼敲击信息: {data}")
                        
                        # 检测木鱼敲击信息
                        self.check_woodfish_hit(data)
            except Exception as e:
                self.log_message(f"接收错误: {str(e)}")
                break
            time.sleep(0.01)
    
    def send_command(self, command):
        """发送命令到ESP32"""
        if not self.is_connected:
            messagebox.showwarning("警告", "请先连接串口")
            return
        
        try:
            # 添加换行符
            if not command.endswith('\n'):
                command += '\n'
            
            self.serial_connection.write(command.encode('utf-8'))
            self.log_message(f"发送: {command.strip()}")
        except Exception as e:
            messagebox.showerror("发送错误", str(e))
            self.log_message(f"发送错误: {str(e)}")
    
    def send_random_command(self, state):
        """发送随机效果命令"""
        speed = self.random_speed.get()
        brightness = self.random_brightness.get()
        command = f"RANDOM:{state}:{speed}:{brightness}"
        self.send_command(command)
    
    def apply_random_params(self):
        """应用当前随机效果参数"""
        self.send_random_command(1)  # 1表示启动随机效果
        self.log_message(f"应用随机效果参数 - 速度: {self.random_speed.get()}, 亮度: {self.random_brightness.get()}")
    
    def apply_motor_params(self):
        """应用当前电机参数"""
        pwm = self.motor_pwm.get()
        state = self.motor_state.get()
        command = f"MOTOR:{pwm}:{state}"
        self.send_command(command)
        self.log_message(f"应用电机参数 - 占空比: {pwm}, 状态: {'启动' if state else '停止'}")
    
    def control_fogger(self, state):
        """控制雾化器"""
        self.fogger_state.set(state)
        command = f"FOGGER:{state}"
        self.send_command(command)
        
        # 更新UI显示
        if state:
            self.fogger_status_canvas.configure(bg="#00FF00")  # 绿色表示开启
            self.fogger_status_label.configure(text="当前状态: 开启")
        else:
            self.fogger_status_canvas.configure(bg="#FF0000")  # 红色表示关闭
            self.fogger_status_label.configure(text="当前状态: 关闭")
        
        self.log_message(f"雾化器状态: {'开启' if state else '关闭'}")
    
    def send_custom_command(self):
        """发送自定义命令"""
        command = self.command_var.get()
        if command:
            self.send_command(command)
            self.command_var.set("")  # 清空输入框
    
    def log_message(self, message):
        """向控制台输出消息"""
        self.console.config(state=tk.NORMAL)
        self.console.insert(tk.END, message + "\n")
        self.console.see(tk.END)  # 滚动到底部
        self.console.config(state=tk.DISABLED)
    
    def show_about(self):
        """显示关于对话框"""
        messagebox.showinfo("关于", "TouchDesigner 模拟器\n\n"
                                  "用于模拟 TouchDesigner 向 ESP32 发送控制命令\n"
                                  "以实现 CAN 总线控制 LED 灯带、电机和雾化器\n"
                                  "支持木鱼敲击状态监测\n\n"
                                  "支持的命令:\n"
                                  "EMOTION:1 - 开心(彩虹效果)\n"
                                  "EMOTION:2 - 伤心(蓝色闪电)\n"
                                  "EMOTION:3 - 惊讶(紫色追逐)\n"
                                  "EMOTION:4 - 随机效果\n"
                                  "RANDOM:1:速度:亮度 - 启动随机效果\n"
                                  "RANDOM:0 - 停止随机效果\n"
                                  "LED:1 - 开启LED\n"
                                  "LED:0 - 关闭LED\n"
                                  "MOTOR:pwm:state - 控制电机\n"
                                  "FOGGER:1 - 开启雾化器\n"
                                  "FOGGER:0 - 关闭雾化器")
    
    def on_closing(self):
        """关闭窗口时的处理"""
        if self.is_connected:
            self.disconnect_serial()
        self.root.destroy()
    
    def check_woodfish_hit(self, data):
        """检查接收数据中是否包含木鱼敲击信息"""
        # 直接检查关键字匹配，简化匹配逻辑
        woodfish_keywords = ["WOODEN_FISH_HIT", "木鱼被敲击", "敲击检测", "KNOCK_DETECTED"]
        
        # 直接字符串匹配，不使用正则表达式
        for keyword in woodfish_keywords:
            if keyword in data:
                self.log_message(f"匹配到木鱼关键词: {keyword}")
                self.update_woodfish_status(True)
                return
        
        # 保留原有的正则表达式匹配作为备用
        woodfish_patterns = [
            r"WOODFISH_HIT",
            r"木鱼被敲击",
            r"敲击检测到",
            r"KNOCK_DETECTED"
        ]
        
        for pattern in woodfish_patterns:
            if re.search(pattern, data, re.IGNORECASE):
                self.log_message(f"通过正则匹配到木鱼信息: {pattern}")
                self.update_woodfish_status(True)
                return
    
    def update_woodfish_status(self, hit_status):
        """更新木鱼敲击状态显示"""
        if hit_status:
            self.woodfish_hit.set(1)
            self.woodfish_status_canvas.configure(bg="#FF0000")  # 红色表示被敲击
            self.woodfish_status_label.configure(text="当前状态: 已敲击!")
            
            # 更新敲击时间
            current_time = time.strftime("%H:%M:%S", time.localtime())
            self.last_hit_time = current_time
            self.woodfish_time_label.configure(text=f"上次敲击时间: {current_time}")
            
            # 播放提示音效 (如果有需要)
            self.root.bell()
    
    def clear_woodfish_status(self):
        """清除木鱼敲击状态"""
        self.woodfish_hit.set(0)
        self.woodfish_status_canvas.configure(bg="#888888")  # 灰色表示未敲击
        self.woodfish_status_label.configure(text="当前状态: 未敲击")

# 主函数
def main():
    root = tk.Tk()
    app = TouchDesignerSimulator(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    
    # 设置应用图标
    try:
        # 在 Windows 上设置图标
        if sys.platform.startswith('win'):
            root.iconbitmap(default='icon.ico')
    except:
        pass
    
    root.mainloop()

if __name__ == "__main__":
    main() 