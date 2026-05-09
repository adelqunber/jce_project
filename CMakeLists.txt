cmake_minimum_required(VERSION 4.1)
project(Dijkstra C)

set(CMAKE_C_STANDARD 11)

find_package(raylib REQUIRED)

add_executable(dijkstra
        main.c
        graph.c
        dijkstra.c
        graph.h
        dijkstra.h
)

target_compile_definitions(dijkstra PRIVATE MILESTONE1)

add_executable(sim
        main.c
        graph.c
        dijkstra.c
        GUI.c
        graph.h
        dijkstra.h
        GUI.h
)

target_link_libraries(sim raylib m)