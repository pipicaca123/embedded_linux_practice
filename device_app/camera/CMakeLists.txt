cmake_minimum_required(VERSION 3.10)
project(CameraApp C CXX) # support both C & C++

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 11)  # 或 17、20 取決於你所需的版本
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")

find_package(OpenCV REQUIRED)
# message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
# message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")

# 指定源文件
set(SOURCES
    src/main.c
    src/show.cpp
)

# 生成可执行文件
add_executable(CameraApp ${SOURCES})

# 指定头文件目录
target_include_directories(CameraApp PRIVATE include ${OpenCV_INCLUDE_DIRS})

# 链接 OpenCV 库
target_link_libraries(CameraApp PRIVATE ${OpenCV_LIBS})
