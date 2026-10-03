add_executable(button_confirm
    main.cpp
    stm32f1xx_it.c
    syscalls.c
)

target_link_libraries(button_confirm PRIVATE hal)

target_link_options(button_confirm PRIVATE
    -T${CMAKE_SOURCE_DIR}/cmake/arch/stm32f103c8t6.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/button_confirm.map
)

add_custom_command(TARGET button_confirm POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:button_confirm> button_confirm.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:button_confirm>
    COMMENT "objcopy -> bin, report size"
)

add_custom_target(sim_button_confirm
    DEPENDS button_confirm
    COMMAND ${CMAKE_COMMAND} -E env XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/renode-config
            renode --console --disable-xwt
            -e "\\$$bin=@$<TARGET_FILE:button_confirm>"
            -e "include @${CMAKE_SOURCE_DIR}/examples/10_button_confirm/renode.resc"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(check_button_confirm
    DEPENDS button_confirm
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/check_renode.py
            $<TARGET_FILE:button_confirm> ${CMAKE_CURRENT_SOURCE_DIR}/renode.resc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(flash_button_confirm
    DEPENDS button_confirm
    COMMAND openocd
            -f interface/stlink.cfg
            -f target/stm32f1x.cfg
            -c "program $<TARGET_FILE:button_confirm> verify reset exit"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
)
