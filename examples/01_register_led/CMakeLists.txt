add_executable(register_led
    main.cpp
    stm32f1xx_it.c
    syscalls.c
)

target_link_libraries(register_led PRIVATE hal)

target_link_options(register_led PRIVATE
    -T${CMAKE_SOURCE_DIR}/cmake/arch/stm32f103c8t6.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/register_led.map
)

add_custom_command(TARGET register_led POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:register_led> register_led.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:register_led>
    COMMENT "objcopy -> bin, report size"
)

add_custom_target(sim_register_led
    DEPENDS register_led
    COMMAND ${CMAKE_COMMAND} -E env XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/renode-config
            renode --console --disable-xwt
            -e "\\$$bin=@$<TARGET_FILE:register_led>"
            -e "include @${CMAKE_SOURCE_DIR}/examples/01_register_led/renode.resc"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(flash_register_led
    DEPENDS register_led
    COMMAND openocd
            -f interface/stlink.cfg
            -f target/stm32f1x.cfg
            -c "program $<TARGET_FILE:register_led> verify reset exit"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
)
