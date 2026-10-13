cmake_minimum_required(VERSION 3.21)
project(blxshell_blob LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Quick ShaderTools)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_qml_module(blxshell_blob
    URI BlxShell.Blob
    VERSION 1.0
    PLUGIN_TARGET blxshell_blobplugin
    SOURCES blob.cpp blob.hpp
)
target_link_libraries(blxshell_blob PRIVATE Qt6::Quick)

qt_add_shaders(blxshell_blob blob_shaders
    BATCHABLE
    PREFIX "/"
    GLSL "300es,330"
    FILES shaders/blob.vert shaders/blob.frag
)

install(TARGETS blxshell_blob blxshell_blobplugin LIBRARY DESTINATION lib/qml/BlxShell/Blob)
install(FILES qmldir DESTINATION lib/qml/BlxShell/Blob)
