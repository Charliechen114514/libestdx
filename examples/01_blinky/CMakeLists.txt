# 运行时桩(SysTick handler、newlib _init/_fini)直接编进固件:
# 中断向量表和 newlib 对这些符号的引用在链接后期才出现,放库里会因
# "没人引用就不拉"被丢,owning 它们的是固件工程,不是库。
add_executable(blinky
    main.cpp
    stm32f1xx_it.c # 中断服务程序:哪些 handler 存在由这份固件决定
    syscalls.c # newlib 桩
)

target_link_libraries(blinky PRIVATE hal)

target_link_options(blinky PRIVATE
    -T${CMAKE_SOURCE_DIR}/cmake/arch/stm32f103c8t6.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/blinky.map
)

add_custom_command(TARGET blinky POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:blinky> blinky.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:blinky>
    COMMENT "objcopy -> bin, report size"
)

add_custom_target(sim
    DEPENDS blinky
    COMMAND renode --console --disable-xwt -e "include @${CMAKE_SOURCE_DIR}/examples/01_blinky/renode.resc"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)

# 真机烧录(抄 TAMCPP:openocd + ST-Link;WSL2 下先 usbipd attach + chmod 666 设备节点)。
add_custom_target(flash
    COMMAND openocd
            -f interface/stlink.cfg
            -f target/stm32f1x.cfg
            -c "program $<TARGET_FILE_DIR:blinky>/blinky.bin verify reset exit 0x08000000"
    DEPENDS blinky
    COMMENT "Flashing blinky.bin via OpenOCD (ST-Link)"
)

# 整片擦除(换固件 / 排查用)。
add_custom_target(erase
    COMMAND openocd
            -f interface/stlink.cfg
            -f target/stm32f1x.cfg
            -c "init; halt; stm32f1x mass_erase 0; exit"
    COMMENT "Mass-erasing STM32F1 flash via OpenOCD"
)
