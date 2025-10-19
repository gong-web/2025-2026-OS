cmake_minimum_required(VERSION 3.10)
project(BuddySystem)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含头文件目录
include_directories(include)

# 添加可执行文件
add_executable(test test.cpp src/buddy_system.cpp)

# 设置编译选项
target_compile_options(test PRIVATE -g)
