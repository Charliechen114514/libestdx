add_executable(tim_heartbeat
    main.cpp
    stm32f1xx_it.c
    syscalls.c
)

target_link_libraries(tim_heartbeat PRIVATE hal)

target_link_options(tim_heartbeat PRIVATE
    -T${CMAKE_SOURCE_DIR}/cmake/arch/stm32f103c8t6.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/tim_heartbeat.map
)

add_custom_command(TARGET tim_heartbeat POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:tim_heartbeat> tim_heartbeat.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:tim_heartbeat>
    COMMENT "objcopy -> bin, report size"
)

add_custom_target(sim_tim_heartbeat
    DEPENDS tim_heartbeat
    COMMAND ${CMAKE_COMMAND} -E env XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/renode-config
            renode --console --disable-xwt
            -e "\\$$bin=@$<TARGET_FILE:tim_heartbeat>"
            -e "include @${CMAKE_SOURCE_DIR}/examples/09_tim_heartbeat/renode.resc"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(check_tim_heartbeat
    DEPENDS tim_heartbeat
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/check_renode.py
            $<TARGET_FILE:tim_heartbeat> ${CMAKE_CURRENT_SOURCE_DIR}/renode.resc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

add_custom_target(flash_tim_heartbeat
    DEPENDS tim_heartbeat
    COMMAND openocd
            -f interface/stlink.cfg
            -f target/stm32f1x.cfg
            -c "program $<TARGET_FILE:tim_heartbeat> verify reset exit"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
)
