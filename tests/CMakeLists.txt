# tests/CMakeLists.txt - 改進版本

# 啟用測試
enable_testing()

# 收集測試檔案
file(GLOB_RECURSE TEST_SOURCES 
    "*test*.cpp"
    "*_test.cpp"
    "test_*.cpp"
)

# 設定測試通用配置
function(setup_test_target test_name test_file)
    # 建立測試可執行檔
    add_executable(${test_name} ${test_file})
    
    # 連結必要的函式庫
    target_link_libraries(${test_name} PRIVATE
        GTest::gtest 
        GTest::gtest_main
        GTest::gmock 
        GTest::gmock_main
    )
    
    # 如果有主要函式庫，也要連結
    if(TARGET mts_lib)
        target_link_libraries(${test_name} PRIVATE mts_lib)
    else()
        target_link_libraries(${test_name} PRIVATE
            Boost::system 
            Boost::thread
            nlohmann_json::nlohmann_json
        )
    endif()
    
    # 設定標頭檔路徑
    target_include_directories(${test_name} PRIVATE 
        ${CMAKE_SOURCE_DIR}/src
    )
    
    target_compile_features(${test_name} PRIVATE cxx_std_17)
    
    # Windows 特定設定
    if(WIN32)
        target_compile_definitions(${test_name} PRIVATE
            _WIN32_WINNT=0x0601
            NOMINMAX
        )
    endif()
    
    # 🎯 註冊測試並設定屬性
    # Windows Visual Studio 需要特殊處理路徑
    if(WIN32 AND CMAKE_GENERATOR MATCHES "Visual Studio")
        # Visual Studio 會將執行檔放在 Debug/Release 子目錄
        add_test(NAME ${test_name} 
                 COMMAND $<TARGET_FILE:${test_name}>)
    else()
        add_test(NAME ${test_name} COMMAND ${test_name})
    endif()
    
    # 設定測試屬性
    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 60                    # 60秒超時
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    
    # 🎯 新增：根據測試類型設定標籤
    if(${test_name} MATCHES ".*performance.*" OR ${test_name} MATCHES ".*stress.*")
        set_tests_properties(${test_name} PROPERTIES LABELS "performance")
    elseif(${test_name} MATCHES ".*integration.*")
        set_tests_properties(${test_name} PROPERTIES LABELS "integration") 
    else()
        set_tests_properties(${test_name} PROPERTIES LABELS "unit")
    endif()
endfunction()

# 如果有測試檔案，建立測試可執行檔
if(TEST_SOURCES)
    foreach(test_file ${TEST_SOURCES})
        # 取得檔案名稱（無擴展名）作為測試名稱
        get_filename_component(test_name ${test_file} NAME_WE)
        
        # 使用通用配置函式
        setup_test_target(${test_name} ${test_file})
        
        message(STATUS "Added test: ${test_name}")
    endforeach()
    
    # 🎯 新增：建立測試目標群組
    add_custom_target(run_unit_tests
        COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
        DEPENDS ${TEST_SOURCES}
        COMMENT "Running unit tests"
    )
    
    add_custom_target(run_all_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS ${TEST_SOURCES} 
        COMMENT "Running all tests"
    )
    
else()
    message(STATUS "No test files found in tests directory")
endif()

# 🎯 新增：測試覆蓋率支援（可選）
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
    
    if(ENABLE_COVERAGE)
        find_program(GCOV_PATH gcov)
        find_program(LCOV_PATH lcov)
        find_program(GENHTML_PATH genhtml)
        
        if(GCOV_PATH AND LCOV_PATH AND GENHTML_PATH)
            target_compile_options(mts_lib PRIVATE --coverage)
            target_link_libraries(mts_lib PRIVATE --coverage)
            
            add_custom_target(coverage
                COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage.info
                COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' --output-file coverage.info.cleaned
                COMMAND ${GENHTML_PATH} -o coverage coverage.info.cleaned
                COMMAND ${CMAKE_COMMAND} -E remove coverage.info coverage.info.cleaned
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating coverage report"
            )
        endif()
    endif()
endif()