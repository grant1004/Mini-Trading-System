cmake_minimum_required(VERSION 3.20)
if(WIN32)
    add_compile_options(/utf-8)
endif()
# 設定 CMake 政策
cmake_policy(SET CMP0167 NEW)  # 使用新的 Boost 查找方式

project(MTS VERSION 1.0.0 LANGUAGES CXX)

# C++ 標準
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 🔧 新增：除錯模式設定
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# 🔧 新增：FIX 除錯選項
option(ENABLE_FIX_DEBUG "Enable FIX protocol debug output" ON)
option(ENABLE_FIX_CHECKSUM_DEBUG "Enable FIX checksum debug output" ON)
option(ENABLE_FIX_PARSE_DEBUG "Enable FIX parsing debug output" ON)
option(ENABLE_FIX_SERIALIZE_DEBUG "Enable FIX serialization debug output" ON)
option(ENABLE_FIX_VALIDATION_DEBUG "Enable FIX validation debug output" ON)
option(ENABLE_FIX_FACTORY_DEBUG "Enable FIX factory debug output" ON)
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

# 🔧 新增：根據選項設定編譯定義
if(ENABLE_FIX_DEBUG OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "🔍 FIX Debug mode enabled")
    
    if(ENABLE_FIX_CHECKSUM_DEBUG)
        add_definitions(-DDEBUG_FIX_CHECKSUM)
        message(STATUS "  - Checksum debug: ON")
    endif()
    
    if(ENABLE_FIX_PARSE_DEBUG)
        add_definitions(-DDEBUG_FIX_PARSE)
        message(STATUS "  - Parse debug: ON")
    endif()
    
    if(ENABLE_FIX_SERIALIZE_DEBUG)
        add_definitions(-DDEBUG_FIX_SERIALIZE)
        message(STATUS "  - Serialize debug: ON")
    endif()
    
    if(ENABLE_FIX_VALIDATION_DEBUG)
        add_definitions(-DDEBUG_FIX_VALIDATION)
        message(STATUS "  - Validation debug: ON")
    endif()
    
    if(ENABLE_FIX_FACTORY_DEBUG)
        add_definitions(-DDEBUG_FIX_FACTORY)
        message(STATUS "  - Factory debug: ON")
    endif()
else()
    message(STATUS "🚀 FIX Production mode (debug disabled)")
endif()

# 子目錄
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(tools)