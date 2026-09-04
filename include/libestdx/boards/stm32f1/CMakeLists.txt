# boards/stm32f1 —— F1 家族包的构建定义。
# 官方 HAL + CMSIS 设备层,C 静态库原样编入(只讲不造);
# F4 来了就在 boards/ 下平级加一个同构目录,根 CMake 只做编排。

set(STM32F1_ROOT ${PROJECT_SOURCE_DIR}/third_party/STM32F1/Drivers)
set(CMSIS_TMPL   ${STM32F1_ROOT}/CMSIS/Device/ST/STM32F1xx/Source/Templates)

# 源文件与 hal/stm32f1xx_hal_conf.h 引入的分片一一对应:
# hal_conf 加一个分片,这里加对应 .c,不要 glob 全量(会编出一堆空 obj)。
set(HAL_SRC
    ${STM32F1_ROOT}/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c        # kernel.hpp
    ${STM32F1_ROOT}/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c # cortex.hpp
    ${STM32F1_ROOT}/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c    # rcc.hpp
    ${STM32F1_ROOT}/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
    ${STM32F1_ROOT}/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c   # gpio.hpp
    ${STM32F1_ROOT}/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
)

add_library(hal STATIC
    ${CMSIS_TMPL}/gcc/startup_stm32f103xb.s
    ${CMSIS_TMPL}/system_stm32f1xx.c
    ${HAL_SRC}
)

target_include_directories(hal PUBLIC
    ${STM32F1_ROOT}/CMSIS/Include
    ${STM32F1_ROOT}/CMSIS/Device/ST/STM32F1xx/Include
    ${STM32F1_ROOT}/STM32F1xx_HAL_Driver/Inc
    ${PROJECT_SOURCE_DIR}/include   # libestdx/xxx.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hal # stm32f1xx_hal_conf.h 随分片住 hal/
)

target_compile_definitions(hal PUBLIC
    STM32F103xB
    USE_HAL_DRIVER
)
