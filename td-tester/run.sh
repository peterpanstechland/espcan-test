#!/bin/bash
# TouchDesigner模拟器启动脚本

# 检查Python是否安装
if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到Python 3。请安装Python 3.6或更高版本。"
    exit 1
fi

# 检查依赖是否已安装
if ! python3 -c "import serial" &> /dev/null; then
    echo "正在安装依赖..."
    pip3 install -r requirements.txt
fi

# 启动模拟器
echo "启动TouchDesigner模拟器..."
python3 td_simulator.py 