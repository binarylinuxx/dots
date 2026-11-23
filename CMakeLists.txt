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
  set(VW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  add_subdirectory("${VW_SOURCE_DIR}" vw EXCLUDE_FROM_ALL)
else()
  find_package(vw 0.1 REQUIRED)
endif()

find_program(GLSLC glslc REQUIRED)
# Compiled to vw's .vws format: vw then knows these shaders never sample the
# backdrop and skips the full-surface copy it would otherwise make per frame.
set(BLXSHELL_SHADERS)
foreach(shader blob shape)
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
target_link_libraries(blxshell-core PUBLIC vw::vw PRIVATE PkgConfig::ZSTD)
add_dependencies(blxshell-core blxshell-shaders)
# Shaders are found in the build tree while developing, or in
# <prefix>/share/blxshell when installed.
target_compile_definitions(blxshell-core PRIVATE
  BLXSHELL_BUILD_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}"
  BLXSHELL_VERSION="${PROJECT_VERSION}"
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

install(TARGETS blxshell blxshell-greeter RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES ${BLXSHELL_SHADERS} DESTINATION "${CMAKE_INSTALL_DATADIR}/blxshell")
# The default wallpaper, for the shell and the greeter alike.
install(FILES wallpapers/runix-m3.png DESTINATION "${CMAKE_INSTALL_DATADIR}/blxshell/wallpapers")
install(FILES icons/runix-logo.svg DESTINATION "${CMAKE_INSTALL_DATADIR}/blxshell/icons")
install(DIRECTORY translations/ DESTINATION "${CMAKE_INSTALL_DATADIR}/blxshell/translations")

# The built-in themes, found through $XDG_DATA_DIRS/blxshell/themes, and the
# manifest schema for package authors.
install(DIRECTORY packages/themes/ DESTINATION "${CMAKE_INSTALL_DATADIR}/blxshell/themes")
install(FILES schemas/manifest.schema.json DESTINATION "${CMAKE_INSTALL_DATADIR}/blxshell/schemas")
