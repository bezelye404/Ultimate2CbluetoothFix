cmake_minimum_required(VERSION 3.20)
project(u2c_service LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only interface library; it needs u2c_core (types and settings). Build it through linux/CMakeLists.txt.
if(NOT TARGET u2c_core)
  message(FATAL_ERROR "Configure through linux/CMakeLists.txt (it adds core/ first)")
endif()
add_library(u2c_service INTERFACE)
target_include_directories(u2c_service INTERFACE include)
target_link_libraries(u2c_service INTERFACE u2c_core)

add_executable(u2c_service_tests tests/test_service.cpp)
target_link_libraries(u2c_service_tests PRIVATE u2c_service)
target_compile_options(u2c_service_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME service COMMAND u2c_service_tests)
set_tests_properties(service PROPERTIES ENVIRONMENT "U2C_RESOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../core/resources")
