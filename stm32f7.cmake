include (CMakeForceCompiler)
SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# specify the cross compiler
set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY      arm-none-eabi-objcopy)
set(CMAKE_SIZE         arm-none-eabi-size)

SET(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/stm32f7.ld)

SET(OPTIMIZATION_LEVEL "s")

set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 20)

#TODO make things like MCU configurable
SET(DEVICE_FLAGS "-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -mabi=aapcs")
SET(OPTIMIZATIONS_FLAGS "-g3 -O${OPTIMIZATION_LEVEL} -finline-functions-called-once -funsigned-char -funsigned-bitfields -ffunction-sections -fdata-sections -fdiagnostics-color=always -fno-stack-protector -finline-small-functions -findirect-inlining -fstack-usage")

SET(DEFINES "-DBUILDING_BOOTLOADER -DBOOT_STM32F7 -DEVICE=STM32F767 -DTX_WITH_CANDB=1 -D__weak='__attribute__((weak))' -D__packed='__attribute__((__packed__))'")

SET(VALIDATION_FLAGS "-Werror=switch -Werror=return-type -Werror=stringop-overflow -Werror=parentheses -Wfatal-errors -Wall -Wextra -Werror=undef -Wduplicated-cond -Wduplicated-branches -Wint-to-pointer-cast -Wlogical-op -Wnull-dereference -Wcast-align -Wvla -Wmissing-format-attribute -Wuninitialized -Winit-self -Wdouble-promotion -Wstrict-aliasing -Wno-unused-local-typedefs -Wno-unused-function -Wno-unused-parameter -pedantic")

SET(CONFIG_FILES "-DUFSEL_CONFIGURATION_FILE=\"<ufsel-configuration.hpp>\"")

# merge all flags

SET(COMMON_FLAGS "${DEVICE_FLAGS} ${OPTIMIZATIONS_FLAGS} ${DEFINES} ${VALIDATION_FLAGS} ${CONFIG_FILES}")

SET(CMAKE_CXX_FLAGS_INIT "${COMMON_FLAGS} -fno-unwind-tables -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -Wno-volatile -fno-asynchronous-unwind-tables  -Wno-register")

SET(CMAKE_C_FLAGS_INIT "${COMMON_FLAGS} -Werror=implicit-function-declaration -Werror=int-conversion -Werror=incompatible-pointer-types")   

SET(CMAKE_EXE_LINKER_FLAGS_INIT " -O${OPTIMIZATION_LEVEL} -Xlinker -Map=output.map -lm --specs=nosys.specs --specs=nano.specs -ffreestanding -fdiagnostics-color=always -Wl,--gc-sections -ffunction-sections -fdata-sections -Wl,--wrap,printf -Wl,--wrap,snprintf -Wl,--wrap,vsnprintf  -T ${LINKER_SCRIPT}")

