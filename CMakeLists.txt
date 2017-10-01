cmake_minimum_required(VERSION 3.2)
project(LSMP CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#-----------------------------------------------------------------------------
# Crypto++

list(APPEND CMAKE_MODULE_PATH ${LSMP_SOURCE_DIR}/cmake)

find_package(sodium REQUIRED)
add_library(libodium INTERFACE)

#-----------------------------------------------------------------------------
# ASIO

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${LSMP_SOURCE_DIR}/libs/asio/asio/include)
target_compile_definitions(asio INTERFACE -DASIO_STANDALONE)

#-----------------------------------------------------------------------------
# nlohmann/json

add_library(json INTERFACE)
target_include_directories(json INTERFACE ${LSMP_SOURCE_DIR}/libs/json)

#-----------------------------------------------------------------------------
# LSMP

file(GLOB_RECURSE LSMP_CXX_SOURCES ${LSMP_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE LSMP_C_SOURCES   ${LSMP_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE LSMP_CXX_HEADERS ${LSMP_SOURCE_DIR}/src/*.hpp)
file(GLOB_RECURSE LSMP_C_HEADERS   ${LSMP_SOURCE_DIR}/src/*.h)

set(LSMP_HEADERS ${LSMP_CXX_HEADERS} ${LSMP_C_HEADERS})
set(LSMP_SOURCES ${LSMP_CXX_SOURCES} ${LSMP_C_SOURCES})

add_library(lsmp STATIC ${LSMP_HEADERS} ${LSMP_SOURCES})
target_include_directories(lsmp PUBLIC ${LSMP_SOURCE_DIR}/src ${sodium_INCLUDE_DIR})
target_link_libraries(lsmp json asio sodium)

#-----------------------------------------------------------------------------
# C++ standard

set_property(TARGET lsmp PROPERTY CXX_STANDARD 11)
set_property(TARGET lsmp PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET lsmp PROPERTY CXX_STANDARD_EXTENSIONS OFF)
