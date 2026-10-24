cmake_minimum_required(VERSION 3.20)
project(Lab408 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -g)
# 依赖关卡的公共库会经由多条路径传递进来（如 test.h 所在关卡），重复链接无害，只关掉这条提示
if(APPLE)
    add_link_options(-Wl,-no_warn_duplicate_libraries)
endif()

enable_testing()
include(cmake/lab.cmake)

# 依赖关卡可能在后面的科目里，但目标名在生成阶段才解析，所以收录顺序不影响链接
foreach(subject lib co os net)
    file(GLOB levels LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/${subject}/[0-9][0-9]-*")
    foreach(level IN LISTS levels)
        if(EXISTS "${level}/CMakeLists.txt")
            add_subdirectory("${level}")
        endif()
    endforeach()
endforeach()
