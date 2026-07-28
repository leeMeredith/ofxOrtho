# ortho-kernel — shared C kernel for the ortho language.
#
# Consumed as a git submodule by ortho-max and ortho-of:
#
#   add_subdirectory(ortho-kernel)
#   target_link_libraries(my_target PRIVATE ortho_kernel)
#
# Plain C99, no dependencies, no allocation, no host types — links cleanly into
# a Max external or an openFrameworks addon.

cmake_minimum_required(VERSION 3.15)
project(ortho_kernel C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(ortho_kernel STATIC
    src/ortho.c
    src/prng.c
)

target_include_directories(ortho_kernel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Only build the oracle when this is the top-level project — hosts embedding the
# kernel as a submodule don't want the test binary.
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_executable(ortho_oracle tests/oracle.c)
    target_link_libraries(ortho_oracle PRIVATE ortho_kernel)
endif()
