add_executable(button_events
    main.cpp
    stm32f1xx_it.c
    syscalls.c
)

target_link_libraries(button_events PRIVATE hal)

target_link_options(button_events PRIVATE
    -T${CMAKE_SOURCE_DIR}/cmake/arch/stm32f103c8t6.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/button_events.map
)

add_custom_command(TARGET button_events POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:button_events> button_events.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:button_events>
    COMMENT "objcopy -> bin, report size"
)

add_custom_target(sim_button_events
    DEPENDS button_events
    COMMAND ${CMAKE_COMMAND} -E env XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/renode-config
            renode --console --disable-xwt
            -e "\\$$bin=@$<TARGET_FILE:button_events>"
            -e "include @${CMAKE_SOURCE_DIR}/examples/07_button_events/renode.resc"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(check_button_renode
    DEPENDS button_events
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/check_renode.py
            $<TARGET_FILE:button_events> ${CMAKE_CURRENT_SOURCE_DIR}/renode.resc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(flash_button_events
    DEPENDS button_events
    COMMAND openocd
            -f interface/stlink.cfg
            -f target/stm32f1x.cfg
            -c "program $<TARGET_FILE:button_events> verify reset exit"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
)
