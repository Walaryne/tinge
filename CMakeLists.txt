cmake_minimum_required(VERSION 3.15)
project(tinge C)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)

pkg_check_modules(HIDAPI REQUIRED hidapi-hidraw)

add_executable(tinge main.c main.h)

target_link_libraries(tinge ${HIDAPI_LIBRARIES} z)
target_include_directories(tinge PUBLIC ${HIDAPI_INCLUDE_DIRS})
target_compile_options(tinge PUBLIC ${HIDAPI_CFLAGS_OTHER})