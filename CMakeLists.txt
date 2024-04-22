cmake_minimum_required(VERSION 3.6)

#set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

#add_compile_options("-I/usr/local/include -I/usr/local/include/libdrm -L/usr/local/lib/aarch64-linux-gnu -ldrm")
set(CMAKE_C_FLAGS "-Wall -O0 -g -I/usr/local/include -I/usr/local/include/libdrm -L/usr/local/lib/aarch64-linux-gnu -ldrm")

project(v4l2_cap)
set(CMAKE_CXX_STANDARD C14)

aux_source_directory(. SRC)
include_directories(./ )
add_executable(v4l2_cap ${SRC})
