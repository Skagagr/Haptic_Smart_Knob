cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Haptic_Smart_Knob)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# ============================================================
# 自动识别用户源文件 (.c)
# CONFIGURE_DEPENDS: 每次构建前检查文件列表是否变化,
#                     若新增/删除了 .c 文件会自动触发重新 configure,
#                     避免"新加文件却没编译进去"的坑
# 注意: 这里只扫描自己新建的驱动目录,不扫描整个 Drivers,
#       避免误把 CMSIS / HAL 库的源文件重复扫入导致重复编译
# ============================================================
file(GLOB_RECURSE USER_SOURCES
        CONFIGURE_DEPENDS
        Drivers/BSP/Knob_Encoder/*.c
        Drivers/BSP/Knob_Motor/*.c
        Drivers/BSP/Knob_Buzzer/*.c
        App/Src/*.c
)

# ============================================================
# 将上面自动识别到的 .c 源文件加入编译目标
# 后续在 Drivers/I2C、OLED、SHT30 目录下新增 .c 文件
# 会被自动扫描到,无需手动修改此处
# ============================================================
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user sources here
        ${USER_SOURCES}
)

# ============================================================
# 添加自定义头文件搜索路径 (.h)
# 每新建一个驱动目录,需要在这里手动补一行,
# 否则对应目录下的 .h 文件 #include 时会找不到
# ============================================================
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined include paths
        Core/Inc
        Drivers/BSP/Knob_Encoder
        Drivers/BSP/Knob_Motor
        Drivers/BSP/Knob_Buzzer
        App/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
        -Wl,-Map=${CMAKE_PROJECT_NAME}.map
        -u _printf_float        # ← 添加这行，开启 sprintf 浮点支持
)
