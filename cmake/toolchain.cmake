# specify cross compilers and tools
set(GCC_PATH /home/hristo/develop/gcc-arm-none-eabi-9-2019-q4-major/bin)

set(CMAKE_C_COMPILER ${GCC_PATH}/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER ${GCC_PATH}/arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER ${GCC_PATH}/arm-none-eabi-gcc)
set(CMAKE_AR ${GCC_PATH}/arm-none-eabi-ar)
set(CMAKE_OBJCOPY ${GCC_PATH}/arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP ${GCC_PATH}/arm-none-eabi-objdump)
set(SIZE ${GCC_PATH}/arm-none-eabi-size)