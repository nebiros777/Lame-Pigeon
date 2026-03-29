# Source file name: CMakeLists.txt
# Author: Igor Matiushin
# Brief description: Builds the standalone Flock stress client against Carrier and local ENet sources.

cmake_minimum_required(VERSION 3.10)
project(Flock LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

set(ENET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/enet")
set(CARRIER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Carrier")

set(ENET_SOURCES
    ${ENET_DIR}/callbacks.c
    ${ENET_DIR}/compress.c
    ${ENET_DIR}/host.c
    ${ENET_DIR}/list.c
    ${ENET_DIR}/packet.c
    ${ENET_DIR}/peer.c
    ${ENET_DIR}/protocol.c
)
if(WIN32)
    list(APPEND ENET_SOURCES ${ENET_DIR}/win32.c)
else()
    list(APPEND ENET_SOURCES ${ENET_DIR}/unix.c)
endif()

add_executable(Flock
        Main.cpp
    ${CARRIER_DIR}/private/Carrier.cpp
    ${ENET_SOURCES}
)

target_include_directories(Flock PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CARRIER_DIR}/public
    ${ENET_DIR}/include
)

if(WIN32)
    target_link_libraries(Flock PRIVATE ws2_32 winmm)
endif()
