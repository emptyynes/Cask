cmake_minimum_required(VERSION 3.16)
project(Cask VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} QUIET OPTIONAL_COMPONENTS Test)

# Ядро (без GUI) и ресурсы с переводами: общие для программы и тестов.
set(CASK_CORE_SOURCES
    src/core/Config.h          src/core/Config.cpp
    src/core/Component.h       src/core/Component.cpp
    src/core/I18n.h            src/core/I18n.cpp
    src/core/LaunchSettings.h  src/core/LaunchSettings.cpp
    src/core/Runner.h          src/core/Runner.cpp
    src/core/Prefix.h          src/core/Prefix.cpp
    src/core/PrefixManager.h   src/core/PrefixManager.cpp
    src/core/Winetricks.h      src/core/Winetricks.cpp
    resources/cask.qrc
)

add_executable(cask
    src/main.cpp
    ${CASK_CORE_SOURCES}
    src/ui/MainWindow.h      src/ui/MainWindow.cpp
    src/ui/NewPrefixDialog.h
    src/ui/SettingsDialog.h
    src/ui/EnvDialog.h
    src/ui/WinetricksDialog.h
)

target_include_directories(cask PRIVATE src)
target_link_libraries(cask PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Портативная раскладка: Cask/{cask, lib/, lang/, data/, prefixes/}
# Бинарник ищет зависимости в ./lib рядом с собой.
set_target_properties(cask PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Cask
    BUILD_RPATH   "$ORIGIN/lib"
    INSTALL_RPATH "$ORIGIN/lib"
)

set(CASK_LAYOUT_DIRS
    lib
    lang
    data/runners
    data/dxvk
    data/d7vk
    data/vkd3d-proton
    data/winetricks
    prefixes
)

foreach(d ${CASK_LAYOUT_DIRS})
    add_custom_command(TARGET cask POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:cask>/${d}")
    install(DIRECTORY DESTINATION ${d})
endforeach()

install(TARGETS cask RUNTIME DESTINATION .)

# Тесты ядра (собираются, только если установлен Qt Test).
if(TARGET Qt${QT_VERSION_MAJOR}::Test)
    enable_testing()
    add_executable(cask_tests tests/test_core.cpp ${CASK_CORE_SOURCES})
    target_include_directories(cask_tests PRIVATE src)
    target_link_libraries(cask_tests PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Test)
    set_target_properties(cask_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    add_test(NAME core COMMAND cask_tests)
endif()
