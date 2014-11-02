cmake_minimum_required(VERSION 2.8)
project(crashcatch C)

include_directories("${crashcatch_SOURCE_DIR}/include" "${crashcatch_BINARY_DIR}")

if(CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(CRASHCATCH_NAME crashcatch64)
else(CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(CRASHCATCH_NAME crashcatch32)
endif(CMAKE_SIZEOF_VOID_P MATCHES "8")

configure_file("${crashcatch_SOURCE_DIR}/config.h.cmake"
               "${crashcatch_BINARY_DIR}/config.h")

add_definitions("-Wall -Wextra")

add_executable(${CRASHCATCH_NAME} main.c)

add_library(crash SHARED libcrash/handler.c)

install(TARGETS ${CRASHCATCH_NAME} crash
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)
