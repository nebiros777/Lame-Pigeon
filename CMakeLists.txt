# Source file name: CMakeLists.txt
# Author: Igor Matiushin
# Brief description: Builds the standalone Dovecote and Carrier targets against the local ENet dependency.

cmake_minimum_required(VERSION 3.20)
project(LamePigeon CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- ENet ----
set(ENET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/enet)

add_library(enet STATIC
    ${ENET_DIR}/callbacks.c
    ${ENET_DIR}/compress.c
    ${ENET_DIR}/host.c
    ${ENET_DIR}/list.c
    ${ENET_DIR}/packet.c
    ${ENET_DIR}/peer.c
    ${ENET_DIR}/protocol.c
    ${ENET_DIR}/unix.c
    ${ENET_DIR}/win32.c
)
target_include_directories(enet PUBLIC ${ENET_DIR}/include)
if(WIN32)
    target_link_libraries(enet PUBLIC ws2_32 winmm)
endif()

# ---- Dovecote (server) ----
add_executable(Dovecote Dovecote/Main.cpp
        Dovecote/public/Squab.h
        Dovecote/public/Dovecote.h
        Dovecote/private/Dovecote.cpp
        Dovecote/private/SpaceGrid.cpp
        Dovecote/public/SpaceGrid.h
        Dovecote/private/Birdhouse.cpp
        Dovecote/public/Birdhouse.h)
target_link_libraries(Dovecote PRIVATE enet)

# ---- Carrier (client) ----
add_executable(Carrier Carrier/Main.cpp
        Dovecote/public/Squab.h
        Carrier/private/Carrier.cpp
        Carrier/public/Carrier.h)
target_include_directories(Carrier PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Carrier/public)
target_link_libraries(Carrier PRIVATE enet)

# ---- Flock (debug: many squabs to Dovecote) ----
add_executable(Flock Flock/Main.cpp
        Carrier/private/Carrier.cpp
        Carrier/public/Carrier.h)
target_include_directories(Flock PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Flock
        ${CMAKE_CURRENT_SOURCE_DIR}/Carrier/public)
target_link_libraries(Flock PRIVATE enet)

if(MINGW)
    target_link_options(Dovecote PRIVATE -static)
    target_link_options(Carrier  PRIVATE -static)
    target_link_options(Flock    PRIVATE -static)
endif()