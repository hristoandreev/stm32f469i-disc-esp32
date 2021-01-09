set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)

include(cmake/toolchain.cmake)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(Script "STM32F469NIHx_FLASH.ld")

#Uncomment for hardware floating point
add_compile_definitions(ARM_MATH_CM4;ARM_MATH_MATRIX_CHECK;ARM_MATH_ROUNDING)
add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
add_link_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)

#Uncomment for software floating point
#add_compile_options(-mfloat-abi=soft)

add_compile_options(-mcpu=cortex-m4 -mthumb -fno-omit-frame-pointer)

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
endif ()

include_directories(Core/Inc TouchGFX/App TouchGFX/target/generated TouchGFX/target Drivers/STM32F4xx_HAL_Driver/Inc Drivers/STM32F4xx_HAL_Driver/Inc/Legacy Middlewares/Third_Party/FreeRTOS/Source/include Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F Drivers/CMSIS/Device/ST/STM32F4xx/Include Drivers/CMSIS/Include Drivers/BSP/Components/Common Drivers/BSP/STM32469I-Discovery Middlewares/ST/touchgfx/framework/include TouchGFX/generated/fonts/include TouchGFX/generated/gui_generated/include TouchGFX/generated/images/include TouchGFX/generated/texts/include TouchGFX/gui/include
                    Middlewares/ST/touchgfx/framework/include
                    TouchGFX/generated/fonts/include
                    TouchGFX/generated/gui_generated/include
                    TouchGFX/generated/images/include
                    TouchGFX/generated/texts/include
                    TouchGFX/gui/include)

add_definitions(-DUSE_HAL_DRIVER -DSTM32F469xx -DDEBUG -DUSE_BPP=24 -DCORE_M4 -D__irq="")

file(GLOB_RECURSE SOURCES
     "Middlewares/*.*"
     "Core/Startup/*.*"
     "TouchGFX/*.*"
     "Drivers/*.*"
     Core/Src/main.c
     Core/Src/freertos.c
     Core/Src/stm32f4xx_hal_msp.c
     Core/Src/stm32f4xx_hal_timebase_tim.c
     Core/Src/stm32f4xx_it.c
     Core/Src/sysmem.c
     )

set (EXCLUDE_DIR
     "Middlewares/ST"
     "TouchGFX/generated/simulator"
     "TouchGFX/simulator"
     "TouchGFX/build")

foreach (TMP_PATH ${SOURCES})
    foreach(TMP_DIR ${EXCLUDE_DIR})
        string (FIND ${TMP_PATH} ${TMP_DIR} EXCLUDE_DIR_FOUND)
        if (NOT ${EXCLUDE_DIR_FOUND} EQUAL -1)
            list (REMOVE_ITEM SOURCES ${TMP_PATH})
        endif ()
    endforeach(TMP_DIR)
endforeach(TMP_PATH)

set_source_files_properties(${SOURCES} PROPERTIES COMPILE_FLAGS -pg)

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/Profiling/${Script})

add_link_options(-Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}-Gprof.map)
add_link_options(-mcpu=cortex-m4 -mthumb -static --specs=rdimon.specs --specs=nosys.specs --specs=nano.specs)
add_link_options(-T ${LINKER_SCRIPT})

add_executable(${PROJECT_NAME}-Gprof.elf
               ${SOURCES}
               Core/Src/system_stm32f4xx.c
               Profiling/gmon.c
               Profiling/exit.c
               Profiling/profil.c
               Profiling/profiler.S
               ${LINKER_SCRIPT})

target_link_libraries(${PROJECT_NAME}-Gprof.elf -L${CMAKE_SOURCE_DIR}/Middlewares/ST/touchgfx/lib/core/cortex_m4f/gcc)
target_link_libraries(${PROJECT_NAME}-Gprof.elf -ltouchgfx-float-abi-hard)
#target_link_libraries(${PROJECT_NAME}-Gprof.elf -ltouchgfx)
target_link_libraries(${PROJECT_NAME}-Gprof.elf -Wl,--start-group -lc -lrdimon -lm -lstdc++ -lsupc++ -Wl,--end-group)

set(HEX_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-Gprof.hex)
set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-Gprof.bin)

add_custom_command(TARGET ${PROJECT_NAME}-Gprof.elf POST_BUILD
                   COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}-Gprof.elf> ${HEX_FILE}
                   COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}-Gprof.elf> ${BIN_FILE}
                   COMMENT "Building ${HEX_FILE}
Building ${BIN_FILE}")
