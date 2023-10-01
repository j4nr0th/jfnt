cmake_minimum_required(VERSION 3.26)
project(jfnt C)

set(CMAKE_C_STANDARD 99)

find_package(Freetype CONFIG)
find_package(Freetype REQUIRED)
find_package(Fontconfig REQUIRED)

add_library(jfnt
        source/jfnt_font.c
        include/jfnt_font.h
        source/jfnt_error.c
        include/jfnt_error.h
        include/jfnt.h
)
if (CMAKE_C_COMPILER_ID STREQUAL GNU)
    target_compile_options(jfnt PRIVATE -Wall -Wextra -Werror)
endif ()

target_link_libraries(jfnt PRIVATE freetype fontconfig)
target_include_directories(jfnt PRIVATE "${FREETYPE_INCLUDE_DIR_ft2build}")

list(APPEND TEST_FILES tests/test_common.c tests/test_common.h)
enable_testing()
find_package(PNG REQUIRED)
add_executable(raster_test
        tests/raster_test.c
        ${TEST_FILES})
target_link_libraries(raster_test PRIVATE jfnt png16)

