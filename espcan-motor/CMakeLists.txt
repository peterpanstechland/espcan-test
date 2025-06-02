cmake_minimum_required(VERSION 3.5)

# 设置项目名称和简短描述
set(PROJECT_NAME "espcan-motor")
set(PROJECT_VER "1.0.0")
set(PROJECT_DESCRIPTION "ESP32 CAN控制电机项目")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(${PROJECT_NAME}) 