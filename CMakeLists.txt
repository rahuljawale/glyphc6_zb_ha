# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# ESP32-C6 AC Monitor - Version 1.5.0 - Clean ESP-IDF Solution
# Set custom partition table
set(PARTITION_TABLE_CSV_PATH ${CMAKE_CURRENT_SOURCE_DIR}/partitions.csv)

# With proper sdkconfig.defaults containing CONFIG_ZB_ENABLED=y,
# ESP-IDF Component Manager should handle all dependencies automatically.
# No manual EXTRA_COMPONENT_DIRS manipulation needed.

# Debug information for different build environments
if(DEFINED ENV{GITHUB_ACTIONS})
    message(STATUS "GitHub Actions Docker build - using ESP-IDF managed components")
else()
    message(STATUS "Local development build - using ESP-IDF managed components")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(glyph_c6_monitor)
