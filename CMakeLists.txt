cmake_minimum_required(VERSION 3.20)

project(text_ui LANGUAGES CXX)

# Deliberately omit the WIN32 keyword: this is a console-subsystem program
# whose main() function creates and runs a native Win32 window.
add_executable(text_ui main.cpp)

target_compile_features(text_ui PRIVATE cxx_std_17)
target_compile_definitions(text_ui PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if(MSVC)
    target_compile_options(text_ui PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(text_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()
