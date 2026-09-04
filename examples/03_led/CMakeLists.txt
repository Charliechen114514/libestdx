add_executable(led_example
    main.cpp
    stm32f1xx_it.c # 中断服务程序:哪些 handler 存在由这份固件决定
    syscalls.c # newlib 桩
)

target_link_libraries(led_example PRIVATE hal)

target_link_options(led_example PRIVATE
    -T${CMAKE_SOURCE_DIR}/cmake/arch/stm32f103c8t6.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/led_example.map
)

add_custom_command(TARGET led_example POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:led_example> led_example.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:led_example>
    COMMENT "objcopy -> bin, report size"
)

add_custom_target(sim_led_example
    DEPENDS led_example
    COMMAND renode --console --disable-xwt -e "include @${CMAKE_SOURCE_DIR}/examples/03_led/renode.resc"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)
