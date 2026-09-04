set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m3)

set(CROSS_COMPILE arm-none-eabi-)
include("${CMAKE_CURRENT_LIST_DIR}/../common.cmake")

set(MCU_FLAGS "-mcpu=cortex-m3 -mthumb")

set(CMAKE_C_FLAGS_INIT "${MCU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${MCU_FLAGS} -fno-exceptions -fno-rtti -fno-threadsafe-statics")
set(CMAKE_ASM_FLAGS_INIT "${MCU_FLAGS} -x assembler-with-cpp")

set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${MCU_FLAGS} -nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 给 clangd / IDE 用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
