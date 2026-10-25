# 总入口：用 CLion 打开 code/，一个窗口看到全部关卡。
# 每关目录下的 CMakeLists.txt 由 ./lab.sh 生成，收录该关的 answer/ 与 mine/ 下的每个项目；
# 关卡和项目也都能单独用 CLion 打开。
cmake_minimum_required(VERSION 3.20)
project(Lab408 C)

enable_testing()

foreach(subject lib co os net example)
    file(GLOB levels LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/${subject}/[0-9][0-9]-*")
    foreach(level IN LISTS levels)
        if(EXISTS "${level}/CMakeLists.txt")
            add_subdirectory("${level}")
        endif()
    endforeach()
endforeach()
