cmake_minimum_required(VERSION 3.10)
project(sqlite_header_vfs C)

set(CMAKE_C_STANDARD 11)

add_library(headervfs SHARED headervfs.c)

set_target_properties(headervfs PROPERTIES
        OUTPUT_NAME "headervfs"
        PREFIX ""
        SUFFIX ".so"
)

# 通用的，还可以设置编译选项
target_compile_options(headervfs PRIVATE -fPIC)
