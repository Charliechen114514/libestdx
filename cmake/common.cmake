# cmake/common.cmake —— 编译器绑定(前缀 → 工具),被各 arch toolchain 文件 include。
# 注意:toolchain 文件里必须用 CMAKE_CURRENT_LIST_DIR 定位,不能用相对路径。

set(CMAKE_C_COMPILER   ${CROSS_COMPILE}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)
set(CMAKE_ASM_COMPILER ${CROSS_COMPILE}gcc)
set(CMAKE_OBJCOPY      ${CROSS_COMPILE}objcopy CACHE FILEPATH "")
set(CMAKE_SIZE         ${CROSS_COMPILE}size CACHE FILEPATH "")
set(CMAKE_READELF      ${CROSS_COMPILE}readelf CACHE FILEPATH "")
