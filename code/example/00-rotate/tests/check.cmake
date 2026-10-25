# 用本文件夹里的 .in / .out 测试项目的程序，由 lab.sh 生成，不用手改。
#
# 项目的 CMakeLists.txt 最后一行 include 本文件后：
#   - 与项目同名的主程序：每个 <用例>.in 喂给它，输出必须与 <用例>.out 一致
#   - <项目名>_test 单元测试：退出码为 0 即通过
# 项目被单独拷走、找不到本文件时，那行 include 带 OPTIONAL 会直接跳过，项目照常编译运行。
#
# 本文件有两种用法：被 include 时登记测试；被 CTest 以 cmake -P 调用时执行一次比对。

if(NOT CMAKE_SCRIPT_MODE_FILE)
    enable_testing()
    get_filename_component(lab_tests_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
    if(TARGET ${PROJECT_NAME}_test)
        add_test(NAME ${PROJECT_NAME}_test COMMAND ${PROJECT_NAME}_test)
    endif()
    file(GLOB lab_cases CONFIGURE_DEPENDS "${lab_tests_dir}/*.in")
    foreach(lab_in IN LISTS lab_cases)
        get_filename_component(lab_case "${lab_in}" NAME_WLE)
        add_test(NAME "${PROJECT_NAME}::${lab_case}"
                 COMMAND "${CMAKE_COMMAND}" "-DPROG=$<TARGET_FILE:${PROJECT_NAME}>"
                         "-DIN=${lab_in}" "-DEXPECTED=${lab_tests_dir}/${lab_case}.out"
                         -P "${CMAKE_CURRENT_LIST_FILE}")
    endforeach()
    return()
endif()

# deps/prompt.h 看到这个变量就不打印输入提示，否则提示文字会混进输出、比对失败
set(ENV{LAB_TEST} 1)

# 比较前统一换行符、去掉每行行尾空白和首尾空行，避免因多一个空格而误判
execute_process(
    COMMAND "${PROG}"
    INPUT_FILE "${IN}"
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE stderr
    RESULT_VARIABLE rc
    TIMEOUT 20
)
if(NOT EXISTS "${EXPECTED}")
    message(FATAL_ERROR "缺少标准答案文件：${EXPECTED}")
endif()
file(READ "${EXPECTED}" expected)

function(normalize var)
    set(text "${${var}}")
    string(REPLACE "\r\n" "\n" text "${text}")
    string(REGEX REPLACE "[ \t]+\n" "\n" text "${text}")
    string(STRIP "${text}" text)
    set(${var} "${text}" PARENT_SCOPE)
endfunction()

normalize(actual)
normalize(expected)

if(NOT rc STREQUAL "0")
    message(FATAL_ERROR "程序异常退出（退出码 ${rc}）\n${stderr}")
endif()
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "输出与标准答案不一致\n----- 期望 -----\n${expected}\n----- 实际 -----\n${actual}\n----------------")
endif()
