# 由 CTest 调用：把 IN 喂给 PROG，输出与 EXPECTED 比较。
# 比较前统一换行符、去掉每行行尾空白和首尾空行，避免因多一个空格而误判。

execute_process(
    COMMAND "${PROG}"
    INPUT_FILE "${IN}"
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE stderr
    RESULT_VARIABLE rc
    TIMEOUT 20
)
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
