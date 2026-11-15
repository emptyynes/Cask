cmake_minimum_required(VERSION 3.16)
project(Cask VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_executable(cask
    src/main.cpp
    src/core/Config.h        src/core/Config.cpp
    src/core/Component.h     src/core/Component.cpp
    src/core/Runner.h        src/core/Runner.cpp
    src/core/Prefix.h        src/core/Prefix.cpp
    src/core/PrefixManager.h src/core/PrefixManager.cpp
    src/ui/MainWindow.h      src/ui/MainWindow.cpp
    src/ui/NewPrefixDialog.h
    src/ui/SettingsDialog.h
)

target_include_directories(cask PRIVATE src)
target_link_libraries(cask PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Портативная раскладка: Cask/{cask, lib/, data/, prefixes/}
# Бинарник ищет зависимости в ./lib рядом с собой.
set_target_properties(cask PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Cask
    BUILD_RPATH   "$ORIGIN/lib"
    INSTALL_RPATH "$ORIGIN/lib"
)

set(CASK_LAYOUT_DIRS
    lib
    data/runners
    data/dxvk
    data/d7vk
    data/vkd3d-proton
    prefixes
)

foreach(d ${CASK_LAYOUT_DIRS})
    add_custom_command(TARGET cask POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:cask>/${d}")
    install(DIRECTORY DESTINATION ${d})
endforeach()

install(TARGETS cask RUNTIME DESTINATION .)
