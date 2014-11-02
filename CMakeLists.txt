cmake_minimum_required(VERSION 2.6)
project(crashcatch C)

add_executable(crashcatch main.c)

install(TARGETS crashcatch RUNTIME DESTINATION bin)
