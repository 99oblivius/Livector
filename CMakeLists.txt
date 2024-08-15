cmake_minimum_required(VERSION 3.5.0)
project(Livector VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

add_executable(Livector WIN32 livector.c)

target_link_libraries(Livector ole32.lib winmm.lib)