#      _________   _____________________  ____  ______
#     / ____/   | / ___/_  __/ ____/ __ \/ __ \/ ____/
#    / /_  / /| | \__ \ / / / /   / / / / / / / __/
#   / __/ / ___ |___/ // / / /___/ /_/ / /_/ / /___
#  /_/   /_/  |_/____//_/  \____/\____/_____/_____/


cmake_minimum_required (VERSION 3.0.2)
set(CMAKE_VERBOSE_MAKEFILE ON)
project (FNC_XNORNET)#项目名称
include(CMakeLibs.cmake)#包含的cmake

# ================================================================================
# Reset the C flags 重置C编译选项
# ================================================================================
set(CMAKE_C_FLAGS           "")# 编译选项
set(CMAKE_C_FLAGS_RELEASE   "")# 两种编译模式
set(CMAKE_C_FLAGS_DEBUG     "")

# ================================================================================
# Build the executable
# ================================================================================

# Add the include directories
include_directories(include)#头文件

# ================================================================================
# C Global Flags
# ================================================================================

if (CMAKE_C_COMPILER_ID MATCHES "Clang")

    # clang - CC flags 编译选项 添加 -std=c99  变量可以延迟到使用之前再定义
    set(CMAKE_C_FLAGS "-O3 -mavx2 -mfma -ffp-contract=fast -ffast-math -funroll-loops -ftree-vectorize -std=c99 ")

elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")

    # GNU GCC - CC flags
    set(CMAKE_C_FLAGS "-O3 -mavx2 -mfma -ffp-contract=fast -ffast-math -funroll-loops -ftree-vectorize -std=c99 ")

endif()

# 源文件
set(SOURCE_FILES
    src/main.c
    src/main.h
    src/xnornet.c
    src/xnornet.h
    src/conv_layer.c
    src/conv_layer.h
    src/fully_con_layer.c
    src/fully_con_layer.h
    src/mnist_wrapper.c
    src/mnist_wrapper.h
    src/pool_layer.c
    src/pool_layer.h
    src/tensor.c
    src/tensor.h
    src/back_prop.c
    src/back_prop.h
)

add_executable(FNC_XNORNET ${SOURCE_FILES})# 可执行文件

target_link_libraries(FNC_XNORNET m)# 链接
target_link_libraries(FNC_XNORNET pcm)
if (DEFINED PCM_DEPENDENCIES)
    add_dependencies(FNC_XNORNET ${PCM_DEPENDENCIES})
endif(DEFINED PCM_DEPENDENCIES)
