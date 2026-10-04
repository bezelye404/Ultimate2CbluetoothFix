cmake_minimum_required(VERSION 3.20)
project(u2c_app LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
add_executable(ultimate2cfixer-headless headless_main.cpp)
target_link_libraries(ultimate2cfixer-headless PRIVATE u2c_platform_linux Threads::Threads)
target_compile_options(ultimate2cfixer-headless PRIVATE -Wall -Wextra -Wpedantic)

# The window: only when the Qt layer was built (Qt 6.4 or newer found).
if(TARGET u2c_ui_qt)
  add_executable(ultimate2cfixer main.cpp)
  target_link_libraries(ultimate2cfixer PRIVATE u2c_platform_linux u2c_ui_qt Threads::Threads)
  target_compile_options(ultimate2cfixer PRIVATE -Wall -Wextra)
endif()

# Data the programs look for at run time (see find_data_path): a "data" folder next to the programs in the build folder,
# and share/ultimate2cfixer in an installed prefix. No source or build folder path is stored in any binary.
set(U2C_LANG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../core/resources/lang)
set(U2C_ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../Assets/logo.ico)
add_custom_target(u2c_app_data ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${U2C_LANG_DIR} ${CMAKE_CURRENT_BINARY_DIR}/data/lang
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${U2C_ICON_FILE} ${CMAKE_CURRENT_BINARY_DIR}/data/logo.ico)
add_dependencies(ultimate2cfixer-headless u2c_app_data)

include(GNUInstallDirs)
install(TARGETS ultimate2cfixer-headless RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TARGET ultimate2cfixer)
  install(TARGETS ultimate2cfixer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY ${U2C_LANG_DIR} DESTINATION ${CMAKE_INSTALL_DATADIR}/ultimate2cfixer)
install(FILES ${U2C_ICON_FILE} DESTINATION ${CMAKE_INSTALL_DATADIR}/ultimate2cfixer)
