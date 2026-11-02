# 总入口：用 CLion 打开 code/，一个窗口看到全部关卡。
# 默认只收录「已建过 mine/ 尝试的关卡」（以及 example/），避免未学关卡的 200+ 个答案目标塞满下拉框；
# 可用 ./lab.sh focus mine | all | <科目> <关卡号> 切换加载范围（见 .focus）。
cmake_minimum_required(VERSION 3.20)
project(Lab408 C)

enable_testing()

set(lab_focus "mine")
if(EXISTS "${CMAKE_SOURCE_DIR}/.focus")
    file(STRINGS "${CMAKE_SOURCE_DIR}/.focus" lab_focus_lines LIMIT_COUNT 1)
    if(lab_focus_lines)
        list(GET lab_focus_lines 0 lab_focus)
        string(STRIP "${lab_focus}" lab_focus)
    endif()
endif()

if(NOT lab_focus STREQUAL "mine" AND NOT lab_focus STREQUAL "all")
    if(EXISTS "${CMAKE_SOURCE_DIR}/${lab_focus}/CMakeLists.txt")
        add_subdirectory("${CMAKE_SOURCE_DIR}/${lab_focus}")
    endif()
else()
    foreach(subject lib co os net example)
        file(GLOB levels LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/${subject}/[0-9][0-9]-*")
        foreach(level IN LISTS levels)
            if(EXISTS "${level}/CMakeLists.txt")
                file(GLOB attempts "${level}/mine/*/CMakeLists.txt")
                if(lab_focus STREQUAL "all" OR attempts OR subject STREQUAL "example")
                    add_subdirectory("${level}")
                endif()
            endif()
        endforeach()
    endforeach()
endif()
