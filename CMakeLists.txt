cmake_minimum_required(VERSION 2.8)
project(crashcatch C)

include_directories("${crashcatch_SOURCE_DIR}/include" "${crashcatch_BINARY_DIR}")

if(CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(CRASHCATCHER_NAME crashcatcher64)
else(CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(CRASHCATCHER_NAME crashcatcher32)
endif(CMAKE_SIZEOF_VOID_P MATCHES "8")

configure_file("${crashcatch_SOURCE_DIR}/config.h.cmake"
               "${crashcatch_BINARY_DIR}/config.h")

add_definitions("-Wall -Wextra")

add_executable(${CRASHCATCHER_NAME} main.c)

add_library(crash SHARED libcrash/handler.c)
set_target_properties(crash PROPERTIES VERSION 0.0.1
                                       SOVERSION 0)

install(TARGETS ${CRASHCATCH_NAME} crash
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)
install(FILES include/crashcatcher.h
        DESTINATION include)
