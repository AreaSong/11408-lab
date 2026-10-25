# 由 lab.sh 生成，不用手改。新增、删除 .c 文件或给文件加上 main 后，在 CLion 里重新加载 CMake 项目即可。
# 这是一个独立项目：用 CLion 打开本文件夹就能编译、运行、跑测试，拷给别人也一样。
#
# 规则：
#   - 本文件夹里含 main 的 .c 各自成为一个程序，程序名为 <LAB_UNIT>_<文件名>
#   - 其余 .c 和 deps/ 下的依赖代码合成公共库，供这些程序调用
#   - 文件名以 _test 结尾的程序：退出码为 0 即通过
#   - tests/<文件名>/<用例>.in 与 .out：把 .in 喂给程序，输出必须与 .out 一致
cmake_minimum_required(VERSION 3.20)

set(LAB_UNIT    "example00_2026-10-25-first")
set(LAB_SUBJECT "example")
set(LAB_LEVEL   "example00")
set(LAB_KIND    "mine")

project(${LAB_UNIT} C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -g)
if(APPLE)
    # 空模板的 .c 只有 #include、没有符号，打进静态库时 ranlib 会报警告
    set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif()
enable_testing()

set(unit_dir "${CMAKE_CURRENT_SOURCE_DIR}")

# 只扫本文件夹和 deps/*/ 这一层：单独打开时生成的 cmake-build-*/ 里有编译器自带的含 main 的 .c，不能被扫进来
file(GLOB own_srcs CONFIGURE_DEPENDS "${unit_dir}/*.c")
file(GLOB dep_srcs CONFIGURE_DEPENDS "${unit_dir}/deps/*/*.c")
file(GLOB dep_entries LIST_DIRECTORIES true "${unit_dir}/deps/*")
set(dep_dirs "")
foreach(d IN LISTS dep_entries)
    if(IS_DIRECTORY "${d}")
        list(APPEND dep_dirs "${d}")
    endif()
endforeach()

# 文件里新增或删掉 main 时需要重新划分，所以把源文件也列为配置依赖
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${own_srcs})
set(mains "")
set(others ${dep_srcs})
foreach(f IN LISTS own_srcs)
    file(STRINGS "${f}" hit REGEX "^[ \t]*int[ \t]+main[ \t]*\\(")
    if(hit)
        list(APPEND mains "${f}")
    else()
        list(APPEND others "${f}")
    endif()
endforeach()

set(core "${LAB_UNIT}_core")
if(others)
    add_library(${core} STATIC ${others})
    target_include_directories(${core} PUBLIC "${unit_dir}" ${dep_dirs})
endif()

set(labels "${LAB_SUBJECT};${LAB_LEVEL};${LAB_KIND}")
foreach(m IN LISTS mains)
    get_filename_component(stem "${m}" NAME_WE)
    set(target "${LAB_UNIT}_${stem}")
    add_executable(${target} "${m}")
    if(others)
        target_link_libraries(${target} PRIVATE ${core})
    else()
        target_include_directories(${target} PRIVATE "${unit_dir}" ${dep_dirs})
    endif()

    if(stem MATCHES "_test$")
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES LABELS "${labels}" TIMEOUT 30)
    endif()
    file(GLOB cases CONFIGURE_DEPENDS "${unit_dir}/tests/${stem}/*.in")
    foreach(in IN LISTS cases)
        get_filename_component(case "${in}" NAME_WLE)
        set(expected "${unit_dir}/tests/${stem}/${case}.out")
        if(NOT EXISTS "${expected}")
            message(WARNING "${in} 缺少对应的标准答案 ${case}.out，已跳过")
            continue()
        endif()
        add_test(NAME "${target}::${case}"
                 COMMAND "${CMAKE_COMMAND}" "-DPROG=$<TARGET_FILE:${target}>"
                         "-DIN=${in}" "-DEXPECTED=${expected}"
                         -P "${unit_dir}/cmake/run_io_test.cmake")
        set_tests_properties("${target}::${case}" PROPERTIES LABELS "${labels}" TIMEOUT 30)
    endforeach()
endforeach()
