cmake_minimum_required(VERSION 3.20)
project(gameoflife C)

set(CMAKE_C_STANDARD 17)

add_executable(gameoflife
        main.c
        axvector.c
        axqueue.c
        axstack.c
        gameoflife.c
        sdl_viewport.c
        square2_png.c)

target_include_directories(gameoflife PRIVATE /usr/include/SDL2)

target_link_libraries(gameoflife PRIVATE SDL2 SDL2_image m)

target_compile_options(gameoflife PRIVATE -Wall -Wextra -Wpedantic -O3)
