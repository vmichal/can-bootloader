cmake_minimum_required(VERSION 3.14)
project(Bootloader CXX C ASM)

include("${CMAKE_CURRENT_LIST_DIR}/ufsel/cmake/QueryRepositoryState.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ufsel/cmake/BitOperations.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ufsel/cmake/Units.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ufsel/cmake/Itertools.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ufsel/cmake/Assert.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ufsel/cmake/Time.cmake")
string(APPEND CMAKE_CXX_FLAGS " -DUFSEL_USING_TIME")
add_definitions(-DUFSEL_USING_UNITS -DUFSEL_USING_UNIT_LITERALS -DUFSEL_USING_CHEAP_ASSERT)

add_definitions(-DECU_NAME=${ECU_NAME})
add_definitions(-DHSE_FREQ=${HSE_FREQ})

add_definitions(-DCAN1_BITRATE=${CAN1_BITRATE})
add_definitions(-DCAN1_PERIPHERAL=${CAN1_PERIPHERAL})
add_definitions(-DCAN1_RX_pin=${CAN1_RX_pin})
add_definitions(-DCAN1_TX_pin=${CAN1_TX_pin})
add_definitions(-DCAN1_used=${CAN1_used})

add_definitions(-DCAN2_BITRATE=${CAN2_BITRATE})
add_definitions(-DCAN2_PERIPHERAL=${CAN2_PERIPHERAL})
add_definitions(-DCAN2_RX_pin=${CAN2_RX_pin})
add_definitions(-DCAN2_TX_pin=${CAN2_TX_pin})
add_definitions(-DCAN2_used=${CAN2_used})
message(STATUS "Generating build files for ${ECU_NAME} running on stm32${MCU} with ${HSE_FREQ} MHz HSE. CAN1 baudrate ${CAN1_BITRATE}, CAN1 pins RX (${CAN1_RX_pin}), TX (${CAN1_TX_pin}), CAN2 baudrate ${CAN2_BITRATE}, pins RX (${CAN2_RX_pin}), TX (${CAN2_TX_pin})")

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

#add_custom_command(
#      POST_BUILD
#      TARGET ${PROJECT_NAME}.elf
#      WORKING_DIRECTORY ../..
#      COMMAND bash ./disassemble.sh
#)

