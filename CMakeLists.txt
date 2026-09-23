cmake_minimum_required(VERSION 3.22)

# 工具链在配置时传:
# cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arch/stm32f103c8t6.cmake
# cmake --build build
project(libestdx C CXX ASM)

# 零开销抽象的"零"以开优化为前提——库的默认产物必须是优化构建。
# 没显式指定 build type 时默认 Release(-O3 -DNDEBUG),别让人 clone 下来
# 一条 cmake 敲出个无优化固件(2026-09-11 why-cpp 开篇实测踩到)。
get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _is_multi_config AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Diagnostics must not bake absolute build paths into the firmware
# (source_location / __FILE__ / reproducible builds). Genex keeps the flag
# away from ASM.
add_compile_options(
    "$<$<COMPILE_LANGUAGE:C,CXX>:-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.>")

add_subdirectory(include/libestdx/boards/stm32f1)
add_subdirectory(examples)
