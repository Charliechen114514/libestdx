add_executable(uart_ring
    main.cpp
    stm32f1xx_it.c
    syscalls.c
)

target_link_libraries(uart_ring PRIVATE hal)

target_link_options(uart_ring PRIVATE
    -T${CMAKE_SOURCE_DIR}/cmake/arch/stm32f103c8t6.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/uart_ring.map
)

add_custom_command(TARGET uart_ring POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:uart_ring> uart_ring.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:uart_ring>
    COMMENT "objcopy -> bin, report size"
)

add_custom_target(sim_ring_example
    DEPENDS uart_ring
    COMMAND ${CMAKE_COMMAND} -E env XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/renode-config
            renode --console --disable-xwt
            -e "\\$$bin=@$<TARGET_FILE:uart_ring>"
            -e "include @${CMAKE_SOURCE_DIR}/examples/08_uart_ring/renode.resc"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(check_ring_renode
    DEPENDS uart_ring
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/check_renode.py
            $<TARGET_FILE:uart_ring> ${CMAKE_CURRENT_SOURCE_DIR}/renode.resc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(flash_ring_example
    DEPENDS uart_ring
    COMMAND openocd
            -f interface/stlink.cfg
            -f target/stm32f1x.cfg
            -c "program $<TARGET_FILE:uart_ring> verify reset exit"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
)
