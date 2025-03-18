cmake_minimum_required(VERSION 3.10)
project(BasicNetwork C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O2")

# 指定源文件
set(SOURCES
    src/main.c
)

# 生成可执行文件
add_executable(BasicNetwork ${SOURCES})

# 指定头文件目录
target_include_directories(BasicNetwork PRIVATE include)