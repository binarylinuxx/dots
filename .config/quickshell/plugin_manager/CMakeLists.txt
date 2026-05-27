cmake_minimum_required(VERSION 3.21)
project(qs_plugin_manager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Qml)

qt_standard_project_setup(REQUIRES 6.5)

# Install into a real-filesystem QML import path so dlopen of the backing
# plugin .so works. Prefer the blxshell runtime root over legacy quickshell
# config paths. Override QS_CONFIG_DIR for packaging or custom installs.
if(DEFINED ENV{BLXSHELL_PATH} AND NOT "$ENV{BLXSHELL_PATH}" STREQUAL "")
    set(DEFAULT_QS_CONFIG_DIR "$ENV{BLXSHELL_PATH}")
else()
    set(DEFAULT_QS_CONFIG_DIR "$ENV{HOME}/.local/blxshell")
endif()
set(QS_CONFIG_DIR "${DEFAULT_QS_CONFIG_DIR}" CACHE PATH "blxshell runtime root")
set(QS_MODULE_DIR "${QS_CONFIG_DIR}/qml/PluginManager")

qt_add_qml_module(qs_plugin_manager
    URI PluginManager
    VERSION 1.0
    OUTPUT_DIRECTORY "${QS_MODULE_DIR}"
    SOURCES
        src/plugin_registry.cpp
        src/plugin_registry.h
)

target_include_directories(qs_plugin_manager PRIVATE src)
target_link_libraries(qs_plugin_manager PRIVATE Qt6::Core Qt6::Qml)

set_target_properties(qs_plugin_manager PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${QS_MODULE_DIR}"
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)
