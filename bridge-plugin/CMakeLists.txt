cmake_minimum_required(VERSION 3.20)
project(AviUtl2McpBridge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(AviUtl2McpBridge SHARED
    src/main.cpp
    src/globals.cpp
    src/string_utils.cpp
    src/object_utils.cpp
    src/commands_read.cpp
    src/commands_edit.cpp
    src/commands_rendering.cpp
    src/commands_fileio.cpp
    src/bridge_server.cpp
)

target_include_directories(AviUtl2McpBridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../sdk_mirror/include/aviutl2_sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# SDKのサンプルと同じオプション: stdint.hの強制include, ソースをUTF-8として解釈
target_compile_options(AviUtl2McpBridge PRIVATE /FIstdint.h /utf-8)
target_compile_definitions(AviUtl2McpBridge PRIVATE UNICODE _UNICODE)

target_link_libraries(AviUtl2McpBridge PRIVATE ws2_32)

# 汎用プラグインの拡張子は .aux2
set_target_properties(AviUtl2McpBridge PROPERTIES
    OUTPUT_NAME "AviUtl2McpBridge"
    SUFFIX ".aux2"
)
