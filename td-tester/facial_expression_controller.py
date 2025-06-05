#!/usr/bin/env python3
"""
TouchDesigner 表情识别交互控制器
木鱼敲击 → 表情识别 → 设备效果演示 → 循环
"""

import serial
import time
import cv2
import mediapipe as mp
import numpy as np
import threading
from enum import Enum
import queue

class SystemState(Enum):
    WAITING = "waiting"           # 等待木鱼敲击
    DETECTING = "detecting"       # 表情识别阶段
    PERFORMING = "performing"     # 设备演示阶段
    COOLDOWN = "cooldown"         # 冷却阶段

class ExpressionType(Enum):
    HAPPY = 1      # 开心 → 彩虹效果
    SAD = 2        # 伤心 → 蓝色闪电  
    SURPRISE = 3   # 惊讶 → 紫色追逐

class FacialExpressionController:
    def __init__(self, serial_port='COM18', baud_rate=115200):
        # 系统状态
        self.current_state = SystemState.WAITING
        self.state_start_time = time.time()
        
        # 配置参数
        self.DETECTION_TIME = 3.0      # 表情识别时间（秒）
        self.PERFORMANCE_TIME = 10.0   # 设备演示时间（秒）
        self.COOLDOWN_TIME = 2.0       # 冷却时间（秒）
        
        # 串口通信
        self.serial_connection = None
        self.serial_port = serial_port
        self.baud_rate = baud_rate
        self.message_queue = queue.Queue()
        
        # MediaPipe 初始化
        self.mp_face_mesh = mp.solutions.face_mesh
        self.mp_drawing = mp.solutions.drawing_utils
        self.face_mesh = self.mp_face_mesh.FaceMesh(
            static_image_mode=False,
            max_num_faces=1,
            refine_landmarks=True,
            min_detection_confidence=0.5,
            min_tracking_confidence=0.5
        )
        
        # 摄像头
        self.cap = None
        self.camera_active = False
        
        # 表情识别结果
        self.detected_expressions = []
        self.final_expression = None
        
        print("表情识别控制器初始化完成")
    
    def connect_serial(self):
        """连接串口"""
        try:
            self.serial_connection = serial.Serial(
                port=self.serial_port,
                baudrate=self.baud_rate,
                timeout=1
            )
            print(f"串口连接成功: {self.serial_port}")
            
            # 启动串口监听线程
            threading.Thread(target=self.serial_listener, daemon=True).start()
            return True
        except Exception as e:
            print(f"串口连接失败: {e}")
            return False
    
    def serial_listener(self):
        """监听串口消息"""
        while self.serial_connection and self.serial_connection.is_open:
            try:
                if self.serial_connection.in_waiting > 0:
                    data = self.serial_connection.readline().decode('utf-8', errors='ignore').strip()
                    if data:
                        self.message_queue.put(data)
                        print(f"收到消息: {data}")
            except Exception as e:
                print(f"串口读取错误: {e}")
                break
            time.sleep(0.01)
    
    def send_command(self, command):
        """发送命令到ESP32"""
        if self.serial_connection and self.serial_connection.is_open:
            try:
                cmd = command + '\n'
                self.serial_connection.write(cmd.encode('utf-8'))
                print(f"发送命令: {command}")
            except Exception as e:
                print(f"发送命令失败: {e}")
    
    def check_wooden_fish_hit(self):
        """检查是否有木鱼敲击事件"""
        while not self.message_queue.empty():
            message = self.message_queue.get()
            # 检测木鱼敲击关键词
            if any(keyword in message for keyword in [
                "WOODEN_FISH_HIT", "木鱼被敲击", "敲击检测", "KNOCK_DETECTED"
            ]):
                print("🥢 检测到木鱼敲击！启动交互流程")
                return True
        return False
    
    def start_camera(self):
        """启动摄像头"""
        if not self.camera_active:
            self.cap = cv2.VideoCapture(0)
            if self.cap.isOpened():
                self.camera_active = True
                print("📷 摄像头启动成功")
                return True
            else:
                print("❌ 摄像头启动失败")
                return False
        return True
    
    def stop_camera(self):
        """关闭摄像头"""
        if self.camera_active and self.cap:
            self.cap.release()
            self.camera_active = False
            print("📷 摄像头已关闭")
    
    def detect_expression(self, frame):
        """使用MediaPipe检测表情"""
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results = self.face_mesh.process(rgb_frame)
        
        if results.multi_face_landmarks:
            face_landmarks = results.multi_face_landmarks[0]
            
            # 提取关键点坐标
            h, w = frame.shape[:2]
            landmarks = []
            for lm in face_landmarks.landmark:
                landmarks.append([int(lm.x * w), int(lm.y * h)])
            
            # 分析表情特征
            expression = self.analyze_facial_features(landmarks)
            return expression
        
        return None
    
    def analyze_facial_features(self, landmarks):
        """分析面部特征判断表情"""
        # MediaPipe面部关键点索引
        LEFT_EYE_INDICES = [33, 7, 163, 144, 145, 153, 154, 155, 133, 173, 157, 158, 159, 160, 161, 246]
        RIGHT_EYE_INDICES = [362, 382, 381, 380, 374, 373, 390, 249, 263, 466, 388, 387, 386, 385, 384, 398]
        MOUTH_INDICES = [61, 84, 17, 314, 405, 321, 308, 324, 318]
        
        # 计算眼部开合度
        left_eye_height = self.calculate_eye_aspect_ratio(landmarks, LEFT_EYE_INDICES[:6])
        right_eye_height = self.calculate_eye_aspect_ratio(landmarks, RIGHT_EYE_INDICES[:6])
        eye_ratio = (left_eye_height + right_eye_height) / 2
        
        # 计算嘴部特征
        mouth_ratio = self.calculate_mouth_aspect_ratio(landmarks, MOUTH_INDICES)
        
        # 根据特征判断表情
        if mouth_ratio > 0.05 and eye_ratio > 0.2:
            return ExpressionType.HAPPY
        elif eye_ratio < 0.15:
            return ExpressionType.SAD
        elif eye_ratio > 0.25 and mouth_ratio > 0.03:
            return ExpressionType.SURPRISE
        
        return None
    
    def calculate_eye_aspect_ratio(self, landmarks, eye_indices):
        """计算眼部长宽比"""
        if len(eye_indices) < 6:
            return 0
        
        # 垂直距离
        v1 = np.linalg.norm(np.array(landmarks[eye_indices[1]]) - np.array(landmarks[eye_indices[5]]))
        v2 = np.linalg.norm(np.array(landmarks[eye_indices[2]]) - np.array(landmarks[eye_indices[4]]))
        
        # 水平距离
        h = np.linalg.norm(np.array(landmarks[eye_indices[0]]) - np.array(landmarks[eye_indices[3]]))
        
        if h > 0:
            return (v1 + v2) / (2.0 * h)
        return 0
    
    def calculate_mouth_aspect_ratio(self, landmarks, mouth_indices):
        """计算嘴部长宽比"""
        if len(mouth_indices) < 6:
            return 0
        
        # 垂直距离（上唇到下唇）
        v = np.linalg.norm(np.array(landmarks[mouth_indices[2]]) - np.array(landmarks[mouth_indices[6]]))
        
        # 水平距离（嘴角到嘴角）
        h = np.linalg.norm(np.array(landmarks[mouth_indices[0]]) - np.array(landmarks[mouth_indices[4]]))
        
        if h > 0:
            return v / h
        return 0
    
    def trigger_device_effect(self, expression):
        """根据表情触发对应的设备效果"""
        print(f"🎭 触发表情效果: {expression.name}")
        
        # 发送对应的情绪命令
        self.send_command(f"EMOTION:{expression.value}")
        
        # 同时开启LED增强视觉效果
        self.send_command("LED:1")
        
        # 根据不同表情添加特殊效果
        if expression == ExpressionType.HAPPY:
            print("😊 开心模式 - 彩虹效果")
            # 可以添加额外的设备控制，如启动雾化器
            # self.send_command("FOGGER:1")
            
        elif expression == ExpressionType.SAD:
            print("😢 伤心模式 - 蓝色闪电")
            # 可以添加电机振动效果
            # self.send_command("MOTOR:100:1")
            
        elif expression == ExpressionType.SURPRISE:
            print("😮 惊讶模式 - 紫色追逐")
            # 可以添加随机效果
            # self.send_command("RANDOM:1:200:255")
    
    def stop_all_effects(self):
        """停止所有设备效果"""
        print("⏹️ 停止所有效果")
        self.send_command("LED:0")
        self.send_command("MOTOR:0:0")
        self.send_command("FOGGER:0")
        self.send_command("RANDOM:0:0:0")
    
    def run_detection_phase(self):
        """运行表情识别阶段"""
        print("🔍 开始表情识别...")
        self.detected_expressions = []
        
        if not self.start_camera():
            return None
        
        detection_start = time.time()
        
        while time.time() - detection_start < self.DETECTION_TIME:
            ret, frame = self.cap.read()
            if ret:
                # 检测表情
                expression = self.detect_expression(frame)
                if expression:
                    self.detected_expressions.append(expression)
                
                # 显示检测画面（可选）
                cv2.putText(frame, f"Detecting... {int(self.DETECTION_TIME - (time.time() - detection_start))}s", 
                           (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                
                if expression:
                    cv2.putText(frame, f"Expression: {expression.name}", 
                               (10, 70), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 0, 0), 2)
                
                cv2.imshow('Expression Detection', frame)
                
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
            
            time.sleep(0.03)  # ~30 FPS
        
        cv2.destroyAllWindows()
        self.stop_camera()
        
        # 分析检测结果，选择最频繁的表情
        if self.detected_expressions:
            from collections import Counter
            expression_counts = Counter(self.detected_expressions)
            final_expression = expression_counts.most_common(1)[0][0]
            print(f"🎯 最终识别表情: {final_expression.name} (出现{expression_counts[final_expression]}次)")
            return final_expression
        else:
            print("❌ 未检测到有效表情，使用默认开心表情")
            return ExpressionType.HAPPY
    
    def run_main_loop(self):
        """运行主循环"""
        print("🚀 表情识别交互系统启动")
        print("等待木鱼敲击来开始交互...")
        
        while True:
            current_time = time.time()
            elapsed_time = current_time - self.state_start_time
            
            if self.current_state == SystemState.WAITING:
                # 等待木鱼敲击
                if self.check_wooden_fish_hit():
                    self.current_state = SystemState.DETECTING
                    self.state_start_time = current_time
                    print("🎭 进入表情识别阶段")
            
            elif self.current_state == SystemState.DETECTING:
                # 表情识别阶段
                expression = self.run_detection_phase()
                if expression:
                    self.final_expression = expression
                    self.trigger_device_effect(expression)
                    self.current_state = SystemState.PERFORMING
                    self.state_start_time = current_time
                    print(f"🎪 开始设备演示阶段 ({self.PERFORMANCE_TIME}秒)")
                else:
                    # 识别失败，回到等待状态
                    self.current_state = SystemState.WAITING
                    self.state_start_time = current_time
            
            elif self.current_state == SystemState.PERFORMING:
                # 设备演示阶段
                if elapsed_time >= self.PERFORMANCE_TIME:
                    self.stop_all_effects()
                    self.current_state = SystemState.COOLDOWN
                    self.state_start_time = current_time
                    print("😴 进入冷却阶段")
                else:
                    # 显示剩余时间
                    remaining = int(self.PERFORMANCE_TIME - elapsed_time)
                    if remaining % 2 == 0:  # 每2秒打印一次
                        print(f"⏰ 演示中... 剩余 {remaining} 秒")
            
            elif self.current_state == SystemState.COOLDOWN:
                # 冷却阶段
                if elapsed_time >= self.COOLDOWN_TIME:
                    self.current_state = SystemState.WAITING
                    self.state_start_time = current_time
                    print("✅ 系统就绪，等待下一次木鱼敲击...")
            
            time.sleep(0.1)  # 主循环延时
    
    def cleanup(self):
        """清理资源"""
        print("🧹 清理系统资源...")
        self.stop_all_effects()
        self.stop_camera()
        if self.serial_connection and self.serial_connection.is_open:
            self.serial_connection.close()
        print("✅ 清理完成")

def main():
    # 创建控制器实例
    controller = FacialExpressionController(serial_port='COM3', baud_rate=115200)
    
    try:
        # 连接串口
        if controller.connect_serial():
            # 运行主循环
            controller.run_main_loop()
        else:
            print("❌ 无法连接串口，请检查设备连接")
    
    except KeyboardInterrupt:
        print("\n🛑 用户中断程序")
    
    finally:
        controller.cleanup()

if __name__ == "__main__":
    main() 