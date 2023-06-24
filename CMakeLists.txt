cmake_minimum_required(VERSION 3.14)
project(Bootloader CXX C ASM)

include("${CMAKE_CURRENT_LIST_DIR}/ufsel/cmake/QueryRepositoryState.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ufsel/cmake/BitOperations.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ufsel/cmake/Units.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ufsel/cmake/Assert.cmake")

add_definitions(-DECU_NAME=${ECU_NAME})
add_definitions(-DHSE_FREQ=${HSE_FREQ})
add_definitions(-DCAN1_FREQ=${CAN1_FREQ})
add_definitions(-DREMAP_CAN2=${REMAP_CAN2})
message(STATUS "Generating build files for ${ECU_NAME} running on stm32${MCU} with ${HSE_FREQ} MHz HSE. CAN2 remap active ${REMAP_CAN2}, CAN1 baudrate ${CAN1_FREQ} kbitps")

set(SRC
        API/BLdriver.cpp
        Bootloader/main.cpp
        Bootloader/bootloader.cpp
        Bootloader/canmanager.cpp
        Bootloader/flash.cpp
        BSP/entry_point.cpp
        BSP/isr_vector.cpp
        BSP/can.cpp
        BSP/fdcan.cpp
        BSP/gpio.cpp
        BSP/timer.cpp

        Drivers/core_cm3.h
        Drivers/core_cm4.h
        Drivers/core_cm7.h
        Drivers/core_cm3.c
        Drivers/stm32f10x.h
        Drivers/stm32f4xx.h
        Drivers/stm32f767xx.h
        
        CANdb/tx2_can.c
        CANdb/tx2_ringbuf.c
        CANdb/can_Bootloader.c
        )

add_executable(${PROJECT_NAME}.elf ${SRC} ${CAN_SRC})

include_directories(SYSTEM Drivers)
include_directories(SYSTEM CANdb)
include_directories(SYSTEM ufsel/include)
include_directories(SYSTEM .)

add_definitions(-DUFSEL_USING_UNITS -DUFSEL_USING_UNIT_LITERALS -DUFSEL_USING_ASSERT)


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

add_custom_command(
      POST_BUILD
      TARGET ${PROJECT_NAME}.elf
      WORKING_DIRECTORY ../..
      COMMAND bash ./disassemble.sh
)

