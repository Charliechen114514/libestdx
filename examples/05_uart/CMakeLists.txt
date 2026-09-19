add_executable(uart_example
    main.cpp
    stm32f1xx_it.c
    syscalls.c
)

target_link_libraries(uart_example PRIVATE hal)

target_link_options(uart_example PRIVATE
    -T${CMAKE_SOURCE_DIR}/cmake/arch/stm32f103c8t6.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/uart_example.map
)

add_custom_command(TARGET uart_example POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:uart_example> uart_example.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:uart_example>
    COMMENT "objcopy -> bin, report size"
)

add_custom_target(sim_uart_example
    DEPENDS uart_example
    COMMAND ${CMAKE_COMMAND} -E env XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/renode-config
            renode --console --disable-xwt
            -e "\\$$bin=@$<TARGET_FILE:uart_example>"
            -e "include @${CMAKE_SOURCE_DIR}/examples/05_uart/renode.resc"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(check_uart_renode
    DEPENDS uart_example
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/check_renode.py
            $<TARGET_FILE:uart_example> ${CMAKE_CURRENT_SOURCE_DIR}/renode.resc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(flash_uart_example
    DEPENDS uart_example
    COMMAND openocd
            -f interface/stlink.cfg
            -f target/stm32f1x.cfg
            -c "program $<TARGET_FILE:uart_example> verify reset exit"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
)
