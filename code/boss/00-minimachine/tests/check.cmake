# 用本文件夹里的 .in / .out 测试项目的程序，由 lab.sh 复制，不用手改。
#
# 项目的 CMakeLists.txt 最后一行 include 本文件后：
#   - tests/<用例>.in       喂给与项目同名的主程序，输出必须与 <用例>.out 一致
#   - tests/<名>/<用例>.in  喂给程序 <项目名>_<名>（一个项目有多个程序时用）
#   - <项目名>_test         单元测试，退出码为 0 即通过
#   - tests/stage-NN/       MiniMachine（boss）的阶段测试：*.c 和项目的 LAB_CORE_SRC 一起编成 <项目名>_sNN_test，
#                           *.in/.out 喂主程序（测试名 <项目名>::sNN/<用例>）；都带标签 stage-NN。
#                           项目目录里的 .stage（如 05）或 -DLAB_STAGE=05 决定只登记到第几阶段，没有就登记全部；
#                           有 .stage 的尝试里，阶段单元测试不进 Build All，由测试 <项目名>_sNN_test::build 按需编译
# 项目被单独拷走、找不到本文件时，那行 include 带 OPTIONAL 会直接跳过，项目照常编译运行。
#
# 本文件有两种用法：被 include 时登记测试；被 CTest 以 cmake -P 调用时执行一次比对。

if(NOT CMAKE_SCRIPT_MODE_FILE)
    enable_testing()
    # 函数体里的 CMAKE_CURRENT_LIST_FILE 指向调用方，所以先把本文件路径存下来
    set(lab_check_script "${CMAKE_CURRENT_LIST_FILE}")
    get_filename_component(lab_tests_dir "${lab_check_script}" DIRECTORY)

    # 可选的第 3、4 个参数：测试名前缀（如 s05/）和 ctest 标签（如 stage-05）
    function(lab_add_cases target dir)
        if(NOT TARGET ${target})
            return()
        endif()
        set(prefix "")
        set(label "")
        if(ARGC GREATER 3)
            set(prefix "${ARGV2}")
            set(label "${ARGV3}")
        endif()
        file(GLOB cases CONFIGURE_DEPENDS "${dir}/*.in")
        foreach(in IN LISTS cases)
            get_filename_component(case "${in}" NAME_WLE)
            add_test(NAME "${target}::${prefix}${case}"
                     COMMAND "${CMAKE_COMMAND}" "-DPROG=$<TARGET_FILE:${target}>"
                             "-DIN=${in}" "-DEXPECTED=${dir}/${case}.out"
                             -P "${lab_check_script}")
            if(label)
                set_tests_properties("${target}::${prefix}${case}" PROPERTIES LABELS "${label}")
            endif()
        endforeach()
    endfunction()

    # 本项目最多登记到第几阶段：-DLAB_STAGE 优先，其次项目目录的 .stage，都没有就是全部
    function(lab_stage_limit out)
        set(limit 99)
        set(stage_file "${CMAKE_CURRENT_SOURCE_DIR}/.stage")
        if(EXISTS "${stage_file}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${stage_file}")
            file(STRINGS "${stage_file}" limit LIMIT_COUNT 1)
            string(STRIP "${limit}" limit)
        endif()
        if(DEFINED LAB_STAGE AND NOT LAB_STAGE STREQUAL "")
            set(limit "${LAB_STAGE}")
        endif()
        if(NOT limit MATCHES "^[0-9]+$")
            message(WARNING "${stage_file} 应写两位阶段号（如 05），收到「${limit}」，按全部阶段登记")
            set(limit 99)
        endif()
        set(${out} ${limit} PARENT_SCOPE)
    endfunction()

    # 尝试里当前阶段没写完时，阶段单元测试会链接失败。为了不让它拖垮 Build All（会连带 code/ 下其他科目），
    # 尝试里的阶段测试不进 Build All，改由 ctest 先跑「<目标>::build」编译它，编译失败即「这一阶段还没写完」
    function(lab_build_on_demand target label)
        set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL ON)
        add_test(NAME ${target}::build
                 COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --config "$<CONFIG>" --target ${target})
        set_tests_properties(${target}::build PROPERTIES
                             LABELS "${label}" FIXTURES_SETUP ${target} RESOURCE_LOCK lab_build)
        set_tests_properties(${target} PROPERTIES FIXTURES_REQUIRED ${target})
    endfunction()

    # 阶段单元测试要链接项目的核心源码，所以项目的 CMakeLists.txt 须在 include 本文件前设好 LAB_CORE_SRC
    function(lab_add_stages)
        file(GLOB stage_dirs LIST_DIRECTORIES true "${lab_tests_dir}/stage-[0-9][0-9]")
        if(NOT stage_dirs)
            return()
        endif()
        lab_stage_limit(limit)
        foreach(dir IN LISTS stage_dirs)
            get_filename_component(stage "${dir}" NAME)
            string(SUBSTRING "${stage}" 6 2 nn)
            if(NOT IS_DIRECTORY "${dir}" OR nn GREATER limit)
                continue()
            endif()
            file(GLOB stage_srcs CONFIGURE_DEPENDS "${dir}/*.c")
            if(stage_srcs AND DEFINED LAB_CORE_SRC)
                set(target ${PROJECT_NAME}_s${nn}_test)
                add_executable(${target} ${stage_srcs} ${LAB_CORE_SRC})
                add_test(NAME ${target} COMMAND ${target})
                set_tests_properties(${target} PROPERTIES LABELS "${stage}")
                if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.stage")
                    lab_build_on_demand(${target} "${stage}")
                endif()
            endif()
            lab_add_cases(${PROJECT_NAME} "${dir}" "s${nn}/" "${stage}")
        endforeach()
    endfunction()

    if(TARGET ${PROJECT_NAME}_test)
        add_test(NAME ${PROJECT_NAME}_test COMMAND ${PROJECT_NAME}_test)
    endif()
    lab_add_cases(${PROJECT_NAME} "${lab_tests_dir}")
    file(GLOB lab_entries LIST_DIRECTORIES true "${lab_tests_dir}/*")
    foreach(entry IN LISTS lab_entries)
        get_filename_component(sub "${entry}" NAME)
        if(IS_DIRECTORY "${entry}" AND NOT sub MATCHES "^stage-[0-9][0-9]$")
            lab_add_cases(${PROJECT_NAME}_${sub} "${entry}")
        endif()
    endforeach()
    lab_add_stages()
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
