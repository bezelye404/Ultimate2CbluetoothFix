cmake_minimum_required(VERSION 3.20)
project(Ultimate2CFixer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive- /EHsc /wd4828)
    # Enable C++/WinRT if available
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/await>)
endif()

add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vigem
)

set(SOURCES
    src/main.cpp
    src/Remapper.cpp
    src/BatteryMonitor.cpp
    src/Localization.cpp
    src/DriverInstaller.cpp
    src/vigem/ViGEmClient.cpp
    src/resource.rc
)

add_executable(Ultimate2CFixer WIN32 ${SOURCES})

target_link_libraries(Ultimate2CFixer PRIVATE
    dinput8
    dxguid
    dwmapi
    comctl32
    shell32
    uxtheme
    urlmon
    setupapi
)

if(MSVC)
    target_link_libraries(Ultimate2CFixer PRIVATE windowsapp)
endif()
