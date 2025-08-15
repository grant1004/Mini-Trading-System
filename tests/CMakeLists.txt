# tests/CMakeLists.txt

# 啟用測試
enable_testing()

# 收集測試檔案
file(GLOB_RECURSE TEST_SOURCES 
    "*test*.cpp"
    "*_test.cpp"
    "test_*.cpp"
)

# 如果有測試檔案，建立測試可執行檔
if(TEST_SOURCES)
    foreach(test_file ${TEST_SOURCES})
        # 取得檔案名稱（無擴展名）作為測試名稱
        get_filename_component(test_name ${test_file} NAME_WE)
        
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
        
        # 註冊測試
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
else()
    message(STATUS "No test files found in tests directory")
endif()

# 建立一個簡單的範例測試檔案（如果沒有任何測試檔案）
if(NOT TEST_SOURCES)
    set(EXAMPLE_TEST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/example_test.cpp")
    if(NOT EXISTS ${EXAMPLE_TEST_FILE})
        file(WRITE ${EXAMPLE_TEST_FILE}
"#include <gtest/gtest.h>

// 範例測試
TEST(ExampleTest, BasicAssertion) {
    EXPECT_EQ(2 + 2, 4);
}

// 如果不需要自定義 main，GTest 會提供預設的
// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
")
        message(STATUS "Created example test file: ${EXAMPLE_TEST_FILE}")
    endif()
    
    # 建立範例測試
    add_executable(example_test ${EXAMPLE_TEST_FILE})
    target_link_libraries(example_test PRIVATE
        GTest::gtest 
        GTest::gtest_main
    )
    target_compile_features(example_test PRIVATE cxx_std_17)
    
    if(WIN32)
        target_compile_definitions(example_test PRIVATE
            _WIN32_WINNT=0x0601
            NOMINMAX
        )
    endif()
    
    add_test(NAME example_test COMMAND example_test)
endif()