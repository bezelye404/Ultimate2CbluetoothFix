cmake_minimum_required(VERSION 3.20)
project(8bitdofixer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive- /EHsc)
    # Enable C++/WinRT if available
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/await>)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(SOURCES
    src/main.cpp
    src/Remapper.cpp
    src/BatteryMonitor.cpp
    src/Localization.cpp
    src/resource.rc
)

add_executable(8bitdofixer WIN32 ${SOURCES})

target_link_libraries(8bitdofixer PRIVATE
    dinput8
    dxguid
    dwmapi
    comctl32
    shell32
)

if(MSVC)
    target_link_libraries(8bitdofixer PRIVATE windowsapp)
endif()
