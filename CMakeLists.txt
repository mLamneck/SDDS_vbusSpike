cmake_minimum_required(VERSION 3.16)
project(sdds_vbusSpike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sdds_vbusSpike INTERFACE)

target_include_directories(sdds_vbusSpike INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)


