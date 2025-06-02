@echo off
:: TouchDesigner模拟器启动脚本 (Windows版)

echo 检查Python安装...
where python >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo 错误: 未找到Python。请安装Python 3.6或更高版本。
    pause
    exit /b 1
)

:: 检查依赖是否已安装
python -c "import serial" >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo 正在安装依赖...
    pip install -r requirements.txt
)

:: 启动模拟器
echo 启动TouchDesigner模拟器...
python td_simulator.py

pause 