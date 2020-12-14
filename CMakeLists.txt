cmake_minimum_required(VERSION 3.0)

option(BUILD_TESTS "whether or not to build tests" OFF)

project(linky)


set(SOURCES
    src/allocator.c
    src/config.c
    src/hashtable.c
    src/linky.c
    src/logging.c
    src/listener.c)

add_executable(linky ${SOURCES})
target_include_directories(linky PUBLIC src)
target_link_options(linky PUBLIC -static)
set_property(TARGET linky PROPERTY C_STANDARD 11)

if(MSVC)
  target_compile_options(linky PRIVATE /W4 /WX)
else()
  target_compile_options(linky PRIVATE -Wall -Wextra -Wno-unused-parameter -pedantic -Werror)
endif()



