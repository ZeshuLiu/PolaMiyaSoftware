cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME FocusUnit)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# 将四个按功能分目录的业务模块加入最终固件，CubeMX 生成清单不自动包含它们。
# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/FocusUnit/Adc/focus_adc.c
    Core/Src/FocusUnit/Encoder/focus_encoder.c
    Core/Src/FocusUnit/Motor/focus_motor.c
    Core/Src/FocusUnit/Scheduler/focus_scheduler.c
)

# Debug 强制 -Os，避免生成工具链默认 -O0 造成 16 KB Flash 超限；保留 -g3。
# Apply to the application and STM32_Drivers through their shared interface.
# Keep this override in the user-owned file: CubeMX regenerates the toolchain
# with -O0 for Debug, which overflows this device's 16 KB Flash.
# Debug still retains the toolchain's -g3 symbols.
target_compile_options(stm32cubemx INTERFACE
    $<$<CONFIG:Debug>:-Os>
)

# 应用和 HAL 同时启用 LTO，跨文件裁剪未使用分支；会影响单步和局部变量观察。
# Compile the HAL and application together at link time so unused generic HAL
# branches can be removed without replacing drivers or changing peripherals.
set_property(TARGET ${CMAKE_PROJECT_NAME} STM32_Drivers
    PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE
)
# 链接阶段也使用 -Os，使 LTO 按体积优化。
# LTO also runs optimization during linking. Override Debug's generated -O0
# there as well, rather than applying -Os only to individual source files.
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -Os)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
