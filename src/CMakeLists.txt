# src/CMakeLists.txt

# 收集所有源代碼檔案
file(GLOB_RECURSE MTS_SOURCES 
    "*.cpp"
    "*.cc" 
    "*.cxx"
)

file(GLOB_RECURSE MTS_HEADERS
    "*.h"
    "*.hpp"
    "*.hxx"
)

# 如果有源代碼，建立主要的 MTS 函式庫
if(MTS_SOURCES)
    add_library(mts_lib ${MTS_SOURCES} ${MTS_HEADERS})
    
    target_include_directories(mts_lib 
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(mts_lib 
        PUBLIC 
            Boost::system 
            Boost::thread
            nlohmann_json::nlohmann_json
    )
    
    # C++ 標準設定
    target_compile_features(mts_lib PUBLIC cxx_std_17)
    
    # Windows 特定設定
    if(WIN32)
        target_compile_definitions(mts_lib PRIVATE
            _WIN32_WINNT=0x0601
            NOMINMAX
        )
    endif()
else()
    message(STATUS "No source files found in src directory")
endif()

# 如果需要建立可執行檔（例如有 main.cpp）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    add_executable(mts_app main.cpp)
    
    if(TARGET mts_lib)
        target_link_libraries(mts_app PRIVATE mts_lib)
    else()
        target_link_libraries(mts_app PRIVATE
            Boost::system 
            Boost::thread
            nlohmann_json::nlohmann_json
        )
    endif()
    
    target_compile_features(mts_app PRIVATE cxx_std_17)
    
    if(WIN32)
        target_compile_definitions(mts_app PRIVATE
            _WIN32_WINNT=0x0601
            NOMINMAX
        )
    endif()
endif()