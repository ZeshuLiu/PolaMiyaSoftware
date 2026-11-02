# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(PM_Controller)

# Preserve exactly the ELF that subsequent image generation and flashing use.
# This is a copy after linking, not an extra build or a hardware operation.
idf_build_get_property(archive_python PYTHON)
add_custom_command(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
    COMMAND "${archive_python}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/archive_firmware.py"
        --elf "$<TARGET_FILE:${CMAKE_PROJECT_NAME}.elf>"
        --map "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map"
        --output "${CMAKE_CURRENT_SOURCE_DIR}/.firmware_archive"
    COMMENT "Archiving firmware ELF and MAP by SHA256"
    VERBATIM
)
