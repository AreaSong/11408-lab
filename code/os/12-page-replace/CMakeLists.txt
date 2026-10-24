# 关卡子配置：不能单独加载，请在 CLion 中加载 code/ 下的 CMakeLists.txt
if(NOT COMMAND lab_level)
    message(FATAL_ERROR "这是关卡的子配置。请在 CLion 中加载 code/CMakeLists.txt（code 目录最外层那份）")
endif()
lab_level(DEPS lib/02-linked-list lib/16-hash)
