# tools/CMakeLists.txt

# 收集工具程式的源代碼
file(GLOB TOOL_SOURCES "*.cpp")

# 為每個工具程式建立可執行檔
foreach(tool_file ${TOOL_SOURCES})
    # 取得檔案名稱（無擴展名）作為工具名稱
    get_filename_component(tool_name ${tool_file} NAME_WE)
    
    # 建立工具可執行檔
    add_executable(${tool_name} ${tool_file})
    
    # 連結必要的函式庫
    if(TARGET mts_lib)
        target_link_libraries(${tool_name} PRIVATE mts_lib)
    else()
        target_link_libraries(${tool_name} PRIVATE
            Boost::system 
            Boost::thread
            nlohmann_json::nlohmann_json
        )
    endif()
    
    # 設定標頭檔路徑
    target_include_directories(${tool_name} PRIVATE 
        ${CMAKE_SOURCE_DIR}/src
    )
    
    target_compile_features(${tool_name} PRIVATE cxx_std_17)
    
    # Windows 特定設定
    if(WIN32)
        target_compile_definitions(${tool_name} PRIVATE
            _WIN32_WINNT=0x0601
            NOMINMAX
        )
    endif()
endforeach()

# 如果沒有工具檔案，顯示訊息
if(NOT TOOL_SOURCES)
    message(STATUS "No tool files found in tools directory")
endif()