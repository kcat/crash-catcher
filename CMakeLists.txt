cmake_minimum_required(VERSION 2.6)
project(crashcatch C)

include_directories("${crashcatch_SOURCE_DIR}/include")

add_definitions("-Wall -Wextra")

add_executable(crashcatch main.c)

add_library(crash SHARED libcrash/handler.c)

install(TARGETS crashcatch crash
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)
