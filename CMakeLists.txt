cmake_minimum_required(VERSION 3.20)

# 設定 CMake 政策
cmake_policy(SET CMP0167 NEW)  # 使用新的 Boost 查找方式

project(MTS VERSION 1.0.0 LANGUAGES CXX)

# C++ 標準
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 特定設定
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 以上
    add_definitions(-DNOMINMAX)  # 避免 Windows.h 的 min/max 巨集
endif()

# 尋找套件
find_package(GTest CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(nlohmann_json CONFIG REQUIRED)

# 包含目錄
include_directories(${PROJECT_SOURCE_DIR}/src)

# 子目錄
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(tools)