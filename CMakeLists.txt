cmake_minimum_required(VERSION 3.6)
project(Bootloader CXX C ASM)

include("${CMAKE_CURRENT_LIST_DIR}/ufsel/cmake/QueryRepositoryState.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ufsel/cmake/BitOperations.cmake")

set(SRC
        Bootloader/main.cpp
        Bootloader/bootloader.cpp
        Bootloader/canmanager.cpp
        Bootloader/flash.cpp
        BSP/entry_point.cpp
        BSP/can.cpp
        BSP/gpio.cpp
        BSP/timer.cpp
        stm32f10x_conf.h
        Drivers/Cmsis/core_cm3.h
        Drivers/Cmsis/core_cm3.c
        Drivers/Cmsis/stm32f10x.h
        Drivers/StdPeriph/Inc/misc.h
        Drivers/StdPeriph/Inc/stm32f10x_adc.h
        Drivers/StdPeriph/Inc/stm32f10x_bkp.h
        Drivers/StdPeriph/Inc/stm32f10x_can.h
        Drivers/StdPeriph/Inc/stm32f10x_cec.h
        Drivers/StdPeriph/Inc/stm32f10x_crc.h
        Drivers/StdPeriph/Inc/stm32f10x_dac.h
        Drivers/StdPeriph/Inc/stm32f10x_dbgmcu.h
        Drivers/StdPeriph/Inc/stm32f10x_dma.h
        Drivers/StdPeriph/Inc/stm32f10x_exti.h
        Drivers/StdPeriph/Inc/stm32f10x_flash.h
        Drivers/StdPeriph/Inc/stm32f10x_fsmc.h
        Drivers/StdPeriph/Inc/stm32f10x_gpio.h
        Drivers/StdPeriph/Inc/stm32f10x_i2c.h
        Drivers/StdPeriph/Inc/stm32f10x_iwdg.h
        Drivers/StdPeriph/Inc/stm32f10x_pwr.h
        Drivers/StdPeriph/Inc/stm32f10x_rcc.h
        Drivers/StdPeriph/Inc/stm32f10x_rtc.h
        Drivers/StdPeriph/Inc/stm32f10x_sdio.h
        Drivers/StdPeriph/Inc/stm32f10x_spi.h
        Drivers/StdPeriph/Inc/stm32f10x_tim.h
        Drivers/StdPeriph/Inc/stm32f10x_usart.h
        Drivers/StdPeriph/Inc/stm32f10x_wwdg.h
        Drivers/StdPeriph/Src/misc.c
        Drivers/StdPeriph/Src/stm32f10x_adc.c
        Drivers/StdPeriph/Src/stm32f10x_bkp.c
        Drivers/StdPeriph/Src/stm32f10x_can.c
        Drivers/StdPeriph/Src/stm32f10x_cec.c
        Drivers/StdPeriph/Src/stm32f10x_crc.c
        Drivers/StdPeriph/Src/stm32f10x_dac.c
        Drivers/StdPeriph/Src/stm32f10x_dbgmcu.c
        Drivers/StdPeriph/Src/stm32f10x_dma.c
        Drivers/StdPeriph/Src/stm32f10x_exti.c
        Drivers/StdPeriph/Src/stm32f10x_flash.c
        Drivers/StdPeriph/Src/stm32f10x_fsmc.c
        Drivers/StdPeriph/Src/stm32f10x_gpio.c
        Drivers/StdPeriph/Src/stm32f10x_i2c.c
        Drivers/StdPeriph/Src/stm32f10x_iwdg.c
        Drivers/StdPeriph/Src/stm32f10x_pwr.c
        Drivers/StdPeriph/Src/stm32f10x_rcc.c
        Drivers/StdPeriph/Src/stm32f10x_rtc.c
        Drivers/StdPeriph/Src/stm32f10x_sdio.c
        Drivers/StdPeriph/Src/stm32f10x_spi.c
        Drivers/StdPeriph/Src/stm32f10x_tim.c
        Drivers/StdPeriph/Src/stm32f10x_usart.c
        Drivers/StdPeriph/Src/stm32f10x_wwdg.c
        CANdb/tx2_can.c
        CANdb/tx2_ringbuf.c
        CANdb/can_Bootloader.c

        startup/startup_stm32f105xc.s
        )

add_executable(${PROJECT_NAME}.elf ${SRC} ${CAN_SRC})

target_include_directories(${PROJECT_NAME}.elf PRIVATE
        Drivers/Cmsis
        Drivers/StdPeriph/Inc
        CANdb
        .
        ufsel/include
        )



set(HEX_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin)

add_custom_command(
        POST_BUILD
        TARGET ${PROJECT_NAME}.elf
        COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
        # COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
        COMMENT "Building ${HEX_FILE} \nBuilding ${BIN_FILE}")

add_custom_command(
        POST_BUILD
        TARGET ${PROJECT_NAME}.elf
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
        )

