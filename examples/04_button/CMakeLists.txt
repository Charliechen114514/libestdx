add_executable(button_example
    main.cpp
    stm32f1xx_it.c # 中断服务程序:哪些 handler 存在由这份固件决定
    syscalls.c # newlib 桩
)

target_link_libraries(button_example PRIVATE hal)

target_link_options(button_example PRIVATE
    -T${CMAKE_SOURCE_DIR}/cmake/arch/stm32f103c8t6.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/button_example.map
)

add_custom_command(TARGET button_example POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:button_example> button_example.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:button_example>
    COMMENT "objcopy -> bin, report size"
)

add_custom_target(sim_button_example
    DEPENDS button_example
    # --disable-xwt:Renode 1.16 默认起 xwt UI,WSL2 无 X 下卡死在 GUI
    # 初始化(零输出假死);console 模式加它才落地。
    COMMAND renode --console --disable-xwt -e "include @${CMAKE_SOURCE_DIR}/examples/04_button/renode.resc"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)
