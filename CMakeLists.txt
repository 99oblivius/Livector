cmake_minimum_required(VERSION 3.5.0)
project(Liviscope VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

add_executable(Liviscope WIN32 liviscope.c)

target_link_libraries(Liviscope ole32.lib winmm.lib)