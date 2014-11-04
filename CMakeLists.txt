cmake_minimum_required(VERSION 2.8)
project(crashcatcher C)

include_directories("${crashcatcher_SOURCE_DIR}/include" "${crashcatcher_BINARY_DIR}")

if(CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(CRASHCATCHER_NAME crashcatcher64)
else(CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(CRASHCATCHER_NAME crashcatcher32)
endif(CMAKE_SIZEOF_VOID_P MATCHES "8")

configure_file("${crashcatcher_SOURCE_DIR}/config.h.cmake"
               "${crashcatcher_BINARY_DIR}/config.h")

add_definitions("-Wall -Wextra")

add_library(crash SHARED libcrash/handler.c)
set_target_properties(crash PROPERTIES VERSION 0.0.1
                                       SOVERSION 0)

add_executable(${CRASHCATCHER_NAME} main.c)
target_link_libraries(${CRASHCATCHER_NAME} crash)

install(TARGETS ${CRASHCATCHER_NAME} crash
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib${LIB_SUFFIX})
install(FILES include/crashcatcher.h
        DESTINATION include)
