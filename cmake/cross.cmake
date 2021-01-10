set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)

set(STM32CubeProgrammer /home/$ENV{USER}/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI)
message(STATUS "STM32CubeProgrammer -> " ${STM32CubeProgrammer})
set(STM32CubeLoader /home/$ENV{USER}/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/ExternalLoader/N25Q128A_STM32469I-DISCO.stldr)
message(STATUS "STM32CubeLoader -> " ${STM32CubeLoader})

include(cmake/toolchain.cmake)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(Script "STM32F469NIHx_FLASH.ld")

#Uncomment for hardware floating point
add_compile_definitions(ARM_MATH_CM4;ARM_MATH_MATRIX_CHECK;ARM_MATH_ROUNDING)
add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
add_link_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)

#Uncomment for software floating point
#add_compile_options(-mfloat-abi=soft)

add_compile_options(-mcpu=cortex-m4 -mthumb)

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -c -x assembler-with-cpp --specs=nano.specs")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections -Wall -fstack-usage --specs=nano.specs")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -Wall -femit-class-debug-always -fstack-usage")

# uncomment to mitigate c++17 absolute addresses warnings
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-register")

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Maximum optimization for speed")
    add_compile_options(-Ofast)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    message(STATUS "Maximum optimization for speed, debug info included")
    add_compile_options(-Ofast -g)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    message(STATUS "Maximum optimization for size")
    add_compile_options(-Os)
else ()
    message(STATUS "Minimal optimization, debug info included")
    add_compile_options(-Og -g)
#    add_compile_options(-O0 -g)
endif ()

include_directories(Core/Inc
                    TouchGFX/App
                    TouchGFX/target/generated
                    TouchGFX/target
                    Drivers/STM32F4xx_HAL_Driver/Inc
                    Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
                    Middlewares/Third_Party/FreeRTOS/Source/include
                    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
                    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
                    Drivers/CMSIS/Device/ST/STM32F4xx/Include
                    Drivers/CMSIS/Include Drivers/BSP/Components/Common
                    Drivers/BSP/STM32469I-Discovery
                    Middlewares/ST/touchgfx/framework/include
                    TouchGFX/generated/fonts/include
                    TouchGFX/generated/gui_generated/include
                    TouchGFX/generated/images/include
                    TouchGFX/generated/texts/include
                    TouchGFX/gui/include
                    Middlewares/ST/touchgfx/framework/include
                    TouchGFX/generated/fonts/include
                    TouchGFX/generated/gui_generated/include
                    TouchGFX/generated/images/include
                    TouchGFX/generated/texts/include
                    TouchGFX/gui/include
                    FATFS/App
                    FATFS/Target
                    Middlewares/Third_Party/FatFs/src
                    USB_DEVICE/App
                    USB_DEVICE/Target
                    Middlewares/ST/STM32_USB_Device_Library/Class/MSC/Inc
                    Middlewares/ST/STM32_USB_Device_Library/Core/Inc
                    LWIP/App
                    LWIP/Target
                    Middlewares/Third_Party/LwIP/src/include
                    Middlewares/Third_Party/LwIP/system
                    Middlewares/Third_Party/wolfSSL_wolfSSL_wolfSSL/wolfssl
                    wolfSSL
                    HTTPSClient
                    HTTPSClient/data
                    HTTPSClient/Socket
                    PPPoS/App
                    PPPoS/Target
                    )

add_definitions(-DUSE_HAL_DRIVER -DSTM32F469xx -DDEBUG -DUSE_BPP=24 -DCORE_M4 -D__irq="")

file(GLOB_RECURSE SOURCES
     "Middlewares/ST/STM32_USB_Device_Library/*.*"
     "Middlewares/Third_Party/FatFs/*.*"
     "Middlewares/Third_Party/FreeRTOS/*.*"
     "Middlewares/Third_Party/LwIP/*.*"
     "Middlewares/Third_Party/wolfSSL_wolfSSL_wolfSSL/wolfssl/src/*.*"
     "Middlewares/Third_Party/wolfSSL_wolfSSL_wolfSSL/wolfssl/wolfcrypt/src/*.*"
     "Core/*.*"
     "TouchGFX/*.*"
     "Drivers/*.*"
     "PPPoS/*.*"
     "USB_DEVICE/*.*"
     "FATFS/*.*"
     "HTTPSClient/*.*"
     "gcc/startup_stm32f469xx.s"
     "wolfSSL/*.*"
     )

set (EXCLUDE_DIR
     "Middlewares/Third_Party/wolfSSL_wolfSSL_wolfSSL/wolfssl/wolfcrypt/src/port"
     "Middlewares/ST/touchgfx"
     "TouchGFX/generated/simulator"
     "TouchGFX/simulator"
     "TouchGFX/build"
     "Middlewares/Third_Party/FreeRTOS/Source/portable/IAR"
     "Middlewares/Third_Party/FreeRTOS/Source/portable/Keil"
     "Middlewares/Third_Party/FreeRTOS/Source/portable/RVDS"
     )

foreach (TMP_PATH ${SOURCES})
    foreach(TMP_DIR ${EXCLUDE_DIR})
        string (FIND ${TMP_PATH} ${TMP_DIR} EXCLUDE_DIR_FOUND)
        if (NOT ${EXCLUDE_DIR_FOUND} EQUAL -1)
            list (REMOVE_ITEM SOURCES ${TMP_PATH})
        endif ()
    endforeach(TMP_DIR)
endforeach(TMP_PATH)

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/gcc/${Script})

add_link_options(-Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
#add_link_options(-mcpu=cortex-m4 -mthumb -static --specs=nano.specs --specs=nosys.specs --specs=rdimon.specs)
add_link_options(-mcpu=cortex-m4 -mthumb -static --specs=nano.specs --specs=nosys.specs)
add_link_options(-T ${LINKER_SCRIPT})

add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT})

target_link_libraries(${PROJECT_NAME}.elf -L${CMAKE_SOURCE_DIR}/Middlewares/ST/touchgfx/lib/core/cortex_m4f/gcc)
target_link_libraries(${PROJECT_NAME}.elf -ltouchgfx-float-abi-hard)
#target_link_libraries(${PROJECT_NAME}.elf -Wl,--start-group -lc -lrdimon -lm -lstdc++ -lsupc++ -Wl,--end-group)
target_link_libraries(${PROJECT_NAME}.elf -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group)

set(HEX_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
                   COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
                   COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
#                   COMMAND ${CMAKE_OBJCOPY} --remove-section=ExtFlashSection $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/intflash.elf
#                   COMMAND ${CMAKE_OBJCOPY} -O ihex --remove-section=ExtFlashSection $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/intflash.hex
#                   COMMAND ${CMAKE_OBJCOPY} -O binary --only-section=ExtFlashSection $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/extflash.bin
#                   COMMAND ${CMAKE_OBJCOPY} -O ihex --only-section=ExtFlashSection $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_BINARY_DIR}/extflash.hex
                   COMMENT "Generate ${HEX_FILE}
                            Generate ${BIN_FILE}")

#add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
add_custom_target("Program-All-Flash-${PROJECT_NAME}.elf" DEPENDS ${PROJECT_NAME}.elf COMMAND ${STM32CubeProgrammer} -c port=SWD -d ${PROJECT_NAME}.hex -el ${STM32CubeLoader} -hardRst
                   COMMENT "Program ${HEX_FILE}")
