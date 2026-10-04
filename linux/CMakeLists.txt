# Entry point for the Linux build: cmake -B build -S linux && cmake --build build && ctest --test-dir build
cmake_minimum_required(VERSION 3.20)
project(ultimate2cfixer_linux LANGUAGES CXX)
enable_testing()

# Binaries must not carry the location of the source or build folder (assert messages, __FILE__, debug info).
get_filename_component(U2C_REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)
add_compile_options(-ffile-prefix-map=${U2C_REPO_ROOT}=. -ffile-prefix-map=${CMAKE_BINARY_DIR}=build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core core)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../service service)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/platform platform)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ui-model ui-model)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ui-qt ui-qt)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/app app)
