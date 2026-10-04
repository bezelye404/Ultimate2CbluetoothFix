cmake_minimum_required(VERSION 3.20)
project(u2c_ui_model LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt-free: everything the window decides. Tested without a display.
add_library(u2c_ui_model STATIC src/display.cpp src/text_state.cpp src/policy.cpp src/view_model.cpp src/demo.cpp)
target_include_directories(u2c_ui_model PUBLIC include)
target_link_libraries(u2c_ui_model PUBLIC u2c_core u2c_service)
target_compile_options(u2c_ui_model PRIVATE -Wall -Wextra -Wpedantic)

add_executable(u2c_ui_model_tests tests/test_ui_model.cpp)
target_link_libraries(u2c_ui_model_tests PRIVATE u2c_ui_model)
target_compile_options(u2c_ui_model_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME ui_model COMMAND u2c_ui_model_tests)
set_tests_properties(ui_model PROPERTIES ENVIRONMENT "U2C_RESOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../../core/resources")
