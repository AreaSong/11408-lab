# 用本文件夹里的 .in / .out 测试项目的程序，由 lab.sh 复制，不用手改。
#
# 项目的 CMakeLists.txt 最后一行 include 本文件后：
#   - tests/<用例>.in       喂给与项目同名的主程序，输出必须与 <用例>.out 一致
#   - tests/<名>/<用例>.in  喂给程序 <项目名>_<名>（一个项目有多个程序时用）
#   - <项目名>_test         单元测试，退出码为 0 即通过
# 项目被单独拷走、找不到本文件时，那行 include 带 OPTIONAL 会直接跳过，项目照常编译运行。
#
# 本文件有两种用法：被 include 时登记测试；被 CTest 以 cmake -P 调用时执行一次比对。

if(NOT CMAKE_SCRIPT_MODE_FILE)
    enable_testing()
    # 函数体里的 CMAKE_CURRENT_LIST_FILE 指向调用方，所以先把本文件路径存下来
    set(lab_check_script "${CMAKE_CURRENT_LIST_FILE}")
    get_filename_component(lab_tests_dir "${lab_check_script}" DIRECTORY)

    function(lab_add_cases target dir)
        if(NOT TARGET ${target})
            return()
        endif()
        file(GLOB cases CONFIGURE_DEPENDS "${dir}/*.in")
        foreach(in IN LISTS cases)
            get_filename_component(case "${in}" NAME_WLE)
            add_test(NAME "${target}::${case}"
                     COMMAND "${CMAKE_COMMAND}" "-DPROG=$<TARGET_FILE:${target}>"
                             "-DIN=${in}" "-DEXPECTED=${dir}/${case}.out"
                             -P "${lab_check_script}")
        endforeach()
    endfunction()

    if(TARGET ${PROJECT_NAME}_test)
        add_test(NAME ${PROJECT_NAME}_test COMMAND ${PROJECT_NAME}_test)
    endif()
    lab_add_cases(${PROJECT_NAME} "${lab_tests_dir}")
    file(GLOB lab_entries LIST_DIRECTORIES true "${lab_tests_dir}/*")
    foreach(entry IN LISTS lab_entries)
        if(IS_DIRECTORY "${entry}")
            get_filename_component(sub "${entry}" NAME)
            lab_add_cases(${PROJECT_NAME}_${sub} "${entry}")
        endif()
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
