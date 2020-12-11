cmake_minimum_required(VERSION 3.0)

option(BUILD_TESTS "whether or not to build tests" OFF)

project(linky)


set(SOURCES
    src/config.c
    src/hashtable.c
    src/linky.c
    src/logging.c
    src/listener.c)

add_executable(linky ${SOURCES})
target_include_directories(linky PUBLIC src)
target_link_options(linky PUBLIC -static)
target_compile_options(linky PUBLIC )

if(MSVC)
  target_compile_options(linky PRIVATE /W4 /WX)
else()
  target_compile_options(linky PRIVATE -Wall -Wextra -Wno-unused-parameter -pedantic -Werror)
endif()



