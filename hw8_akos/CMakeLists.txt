cmake_minimum_required(VERSION 3.20)
project(hw8_akos)

set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_POSIX_C_SOURCE=200809L")

add_executable(server server.c)
target_link_libraries(server pthread)


add_executable(client client.c)
target_link_libraries(client pthread)

# Версия для макос