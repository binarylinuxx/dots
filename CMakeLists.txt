cmake_minimum_required(VERSION 3.25)

project(blxshell VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

# Use an installed libvw by default; point VW_SOURCE_DIR at a vw checkout to
# build it alongside (the dev shell does this).
set(VW_SOURCE_DIR "$ENV{VW_SOURCE_DIR}" CACHE PATH "vw source checkout to build instead of find_package(vw)")
if(VW_SOURCE_DIR)
  set(VW_WITH_LUA OFF CACHE BOOL "" FORCE)
  # Facet's interpreter runs plugins' views (libvw-facet, vw::facet).
  set(VW_WITH_FACET ON CACHE BOOL "" FORCE)
  set(VW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  add_subdirectory("${VW_SOURCE_DIR}" vw EXCLUDE_FROM_ALL)
else()
  find_package(vw 0.1 REQUIRED)
endif()

find_program(GLSLC glslc REQUIRED)
# Compiled to vw's .vws format: vw then knows these shaders never sample the
# backdrop and skips the full-surface copy it would otherwise make per frame.
set(BLXSHELL_SHADERS)
foreach(shader blob shape wave)
  vw_compile_shader(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${shader}.vws"
                    SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}.comp")
  list(APPEND BLXSHELL_SHADERS "${CMAKE_CURRENT_BINARY_DIR}/${shader}.vws")
endforeach()
add_custom_target(blxshell-shaders ALL DEPENDS ${BLXSHELL_SHADERS})

# Everything but the entry points, shared by the shell and the greeter.
add_library(blxshell-core STATIC
  src/archive.cpp
  src/bar.cpp
  src/blob.cpp
  src/config.cpp
  src/dashboard.cpp
  src/desktop.cpp
  src/draw.cpp
  src/i18n.cpp
  src/halley.cpp
  src/ipc.cpp
  src/island.cpp
  src/launcher.cpp
  src/lock.cpp
  src/net.cpp
  src/notifications.cpp
  src/notify.cpp
  src/packages.cpp
  src/plugins.cpp
  src/popouts.cpp
  src/quickshare.cpp
  src/services.cpp
  src/settings.cpp
  src/shapes.cpp
  src/share_panel.cpp
  src/sidebar.cpp
  src/style.cpp
  src/surfaces.cpp
  src/system.cpp
  src/widgets.cpp)
# .blx packages are zstd-compressed tar archives.
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)
# Which project's packages this build takes: manifests and repositories name
# it as "project". Another shell reusing this plugin system sets its own, so
# the two ecosystems never install each other's packages.
set(BLXSHELL_PROJECT_ID "blxshell" CACHE STRING "Project id packages must name to be installed here")
if(NOT BLXSHELL_PROJECT_ID MATCHES "^[a-z][a-z0-9-]*$")
  message(FATAL_ERROR "BLXSHELL_PROJECT_ID must be lowercase letters, digits and -")
endif()

# C++ plugins: <blxshell/plugin.hpp>, stamped with this version (its ABI tag).
configure_file(include/blxshell/plugin.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/include/blxshell/plugin.hpp" @ONLY)
target_include_directories(blxshell-core PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include")
target_link_libraries(blxshell-core PUBLIC vw::vw vw::facet PRIVATE PkgConfig::ZSTD ${CMAKE_DL_LIBS})
add_dependencies(blxshell-core blxshell-shaders)
# Shaders are found in the build tree while developing, or in
# <prefix>/share/blxshell when installed.
target_compile_definitions(blxshell-core PRIVATE
  BLXSHELL_BUILD_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}"
  BLXSHELL_VERSION="${PROJECT_VERSION}"
  BLXSHELL_PROJECT_ID="${BLXSHELL_PROJECT_ID}"
  BLXSHELL_SOURCE_PACKAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/packages"
  BLXSHELL_SOURCE_WALLPAPER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/wallpapers"
  BLXSHELL_SOURCE_ICON_DIR="${CMAKE_CURRENT_SOURCE_DIR}/icons"
  BLXSHELL_SOURCE_TRANSLATION_DIR="${CMAKE_CURRENT_SOURCE_DIR}/translations")
# vw props are designated initializers that omit defaulted fields on purpose.
set(BLXSHELL_WARNINGS -Wall -Wextra -Wpedantic
  -Wno-missing-designated-field-initializers -Wno-missing-field-initializers)
target_compile_options(blxshell-core PRIVATE ${BLXSHELL_WARNINGS})

add_executable(blxshell src/main.cpp)
target_link_libraries(blxshell PRIVATE blxshell-core)
target_compile_options(blxshell PRIVATE ${BLXSHELL_WARNINGS})

# The greetd login screen, in the lock screen's style.
add_executable(blxshell-greeter src/greeter.cpp)
target_link_libraries(blxshell-greeter PRIVATE blxshell-core)
target_compile_options(blxshell-greeter PRIVATE ${BLXSHELL_WARNINGS})

# Draws the lock screen on an overlay without locking the session, apart from
# the shell, to look at it while working on it. Not installed.
add_executable(blxshell-preview EXCLUDE_FROM_ALL src/preview.cpp)
target_link_libraries(blxshell-preview PRIVATE blxshell-core)
target_compile_options(blxshell-preview PRIVATE ${BLXSHELL_WARNINGS})

# The plugin and theme store, a window of its own.
add_executable(blxshell-store src/store.cpp)
target_link_libraries(blxshell-store PRIVATE blxshell-core)
target_compile_options(blxshell-store PRIVATE ${BLXSHELL_WARNINGS})

install(TARGETS blxshell blxshell-greeter blxshell-store RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES share/blxshell-store.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES ${BLXSHELL_SHADERS} DESTINATION "${CMAKE_INSTALL_DATADIR}/blxshell")
# The default wallpaper, for the shell and the greeter alike.
install(FILES wallpapers/runix-m3.png DESTINATION "${CMAKE_INSTALL_DATADIR}/blxshell/wallpapers")
install(FILES icons/runix-logo.svg DESTINATION "${CMAKE_INSTALL_DATADIR}/blxshell/icons")
install(DIRECTORY translations/ DESTINATION "${CMAKE_INSTALL_DATADIR}/blxshell/translations")

# The built-in themes, found through $XDG_DATA_DIRS/blxshell/themes, and the
# manifest schema for package authors.
install(DIRECTORY packages/themes/ DESTINATION "${CMAKE_INSTALL_DATADIR}/blxshell/themes")
install(FILES schemas/manifest.schema.json DESTINATION "${CMAKE_INSTALL_DATADIR}/blxshell/schemas")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/blxshell/plugin.hpp" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/blxshell")

# The example C++ plugin (packages/plugins/dev.blxshell.native), built here so
# its ABI tag matches this blxshell: build/plugins/dev.blxshell.native/lib/plugin.so.
add_library(blxshell-native-example MODULE packages/plugins/dev.blxshell.native/src/plugin.cpp)
target_include_directories(blxshell-native-example PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")
target_link_libraries(blxshell-native-example PRIVATE vw::vw)
set_target_properties(blxshell-native-example PROPERTIES PREFIX "" OUTPUT_NAME plugin
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins/dev.blxshell.native/lib")
