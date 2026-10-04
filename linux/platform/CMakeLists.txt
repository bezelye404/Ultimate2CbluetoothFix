cmake_minimum_required(VERSION 3.20)
project(u2c_platform_linux LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt-free Linux platform code (files, autostart, single instance now; device, virtual pad and battery in later steps).
add_library(u2c_platform_linux STATIC
  src/paths.cpp src/settings_store.cpp src/autostart.cpp src/single_instance.cpp
  src/evdev_logic.cpp src/linux_service.cpp src/linux_backend.cpp
  src/bluez_logic.cpp src/battery_monitor.cpp src/bluez_battery.cpp)
target_include_directories(u2c_platform_linux PUBLIC include)
target_link_libraries(u2c_platform_linux PUBLIC u2c_core u2c_service)
target_compile_options(u2c_platform_linux PRIVATE -Wall -Wextra -Wpedantic)

# The battery reader talks to BlueZ through sd-bus (libsystemd). Without it the program still builds, without battery.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SYSTEMD IMPORTED_TARGET libsystemd)
endif()
if(TARGET PkgConfig::SYSTEMD)
  target_compile_definitions(u2c_platform_linux PRIVATE U2C_HAVE_SDBUS)
  target_link_libraries(u2c_platform_linux PUBLIC PkgConfig::SYSTEMD)
  message(STATUS "libsystemd found: battery reading enabled")
else()
  message(WARNING "libsystemd not found: building without battery reading")
endif()

find_package(Threads REQUIRED)
add_executable(u2c_platform_tests tests/test_platform.cpp tests/test_evdev_logic.cpp tests/test_linux_service.cpp tests/test_battery.cpp)
target_link_libraries(u2c_platform_tests PRIVATE u2c_platform_linux Threads::Threads)
target_compile_options(u2c_platform_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME platform COMMAND u2c_platform_tests)
