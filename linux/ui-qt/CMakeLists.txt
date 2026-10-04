cmake_minimum_required(VERSION 3.20)
project(u2c_ui_qt LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The Qt layer is optional: without Qt 6.4 or newer everything else still builds.
find_package(Qt6 6.4 QUIET COMPONENTS Widgets)
if(NOT Qt6_FOUND)
  message(WARNING "Qt 6 (6.4 or newer) not found: the window (ultimate2cfixer) is not built")
  return()
endif()
message(STATUS "Qt ${Qt6_VERSION} found: building the window")

set(CMAKE_AUTOMOC OFF)   # no class here needs the meta object compiler (only lambdas and Qt's own signals are used)
add_library(u2c_ui_qt STATIC src/flat_button.cpp src/main_window.cpp src/service_bridge.cpp src/tray_controller.cpp src/notice_dialog.cpp)
target_include_directories(u2c_ui_qt PUBLIC include)
target_link_libraries(u2c_ui_qt PUBLIC u2c_ui_model Qt6::Widgets)
target_compile_options(u2c_ui_qt PRIVATE -Wall -Wextra)
