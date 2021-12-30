cmake_minimum_required(VERSION 3.14)
project(SG2_Software C)

set(CMAKE_C_STANDARD 11)

add_executable(SG2_Software
        lcd05.c
        sg2_201_32.c)
