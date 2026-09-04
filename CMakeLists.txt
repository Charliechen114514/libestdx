cmake_minimum_required(VERSION 3.22)

# 工具链在配置时传:
# cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arch/stm32f103c8t6.cmake
# cmake --build build
project(libestdx C CXX ASM)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(include/libestdx/boards/stm32f1)
add_subdirectory(examples)
