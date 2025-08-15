cmake_minimum_required(VERSION 3.15)

# 为项目添加版本号和语言，这是规范的做法
project(sqlite_header_vfs LANGUAGES C)

# 如果没有指定构建类型（如 Debug, Release），则默认为 Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug, Release, RelWithDebInfo, MinSizeRel)")
endif()

# ----------------------------------------------------------------------------
# 定义库
# ----------------------------------------------------------------------------

# 创建一个共享库目标
add_library(headervfs SHARED headervfs.c)

# 1. 设置 C 语言标准 (推荐方式)
set_target_properties(headervfs PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
)

# 2. 自动处理位置无关代码 (PIC)
#    这是 -fPIC 的可移植替代方案。CMake 会在需要时自动为共享库添加 -fPIC 标志。
set_target_properties(headervfs PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

# --- 添加编译选项 ---

# 4. 添加通用的编译警告，有助于提高代码质量
#    这主要针对 GCC 和 Clang 编译器
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(headervfs PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()
