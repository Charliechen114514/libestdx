add_executable(gpio_example
    gpio_example.cpp
    stm32f1xx_it.c # 中断服务程序:哪些 handler 存在由这份固件决定
    syscalls.c # newlib 桩
)

target_link_libraries(gpio_example PRIVATE hal)

target_link_options(gpio_example PRIVATE
    -T${CMAKE_SOURCE_DIR}/cmake/arch/stm32f103c8t6.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/gpio_example.map
)

add_custom_command(TARGET gpio_example POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:gpio_example> gpio_example.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:gpio_example>
    COMMENT "objcopy -> bin, report size"
)

add_custom_target(sim_gpio_example
    DEPENDS gpio_example
    COMMAND renode --console -e "include @${CMAKE_SOURCE_DIR}/examples/02_gpio/renode.resc"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)
