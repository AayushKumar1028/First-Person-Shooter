cmake_minimum_required(VERSION 3.16)
project(fps_shooter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# --- dependencies ------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

# zlib is optional: it only enables PNG texture loading.
find_package(ZLIB)

# --- sources -----------------------------------------------------------------
add_executable(fps_shooter
  src/main.cpp
  src/framebuffer.cpp
  src/font.cpp
  src/image.cpp
  src/assets.cpp
  src/level.cpp
  src/levels.cpp
  src/render.cpp
  src/hud.cpp
  src/entities.cpp
  src/audio.cpp
  src/game.cpp
)

target_include_directories(fps_shooter PRIVATE src)
target_link_libraries(fps_shooter PRIVATE PkgConfig::SDL2)

if(ZLIB_FOUND)
  target_link_libraries(fps_shooter PRIVATE ZLIB::ZLIB)
  target_compile_definitions(fps_shooter PRIVATE FPS_HAVE_ZLIB=1)
endif()

if(MSVC)
  target_compile_options(fps_shooter PRIVATE /W4 /permissive-)
else()
  target_compile_options(fps_shooter PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Where the "assets/" folder lives at build time, so custom textures can be
# swapped in without rebuilding anything.
target_compile_definitions(fps_shooter PRIVATE
  FPS_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# --- headless developer tools ------------------------------------------------
# render_check : renders known scenes and prints them as terminal art
# level_check  : validates campaign map geometry, reachability and spawns
add_executable(render_check EXCLUDE_FROM_ALL
  tools/render_check.cpp
  src/framebuffer.cpp src/font.cpp src/image.cpp src/assets.cpp
  src/level.cpp src/levels.cpp src/render.cpp)
target_include_directories(render_check PRIVATE src)
if(ZLIB_FOUND)
  target_link_libraries(render_check PRIVATE ZLIB::ZLIB)
  target_compile_definitions(render_check PRIVATE FPS_HAVE_ZLIB=1)
endif()

add_executable(level_check EXCLUDE_FROM_ALL
  tools/level_check.cpp
  src/framebuffer.cpp src/font.cpp src/image.cpp src/assets.cpp
  src/level.cpp src/levels.cpp)
target_include_directories(level_check PRIVATE src)
if(ZLIB_FOUND)
  target_link_libraries(level_check PRIVATE ZLIB::ZLIB)
  target_compile_definitions(level_check PRIVATE FPS_HAVE_ZLIB=1)
endif()

# Keep a copy of assets next to the executable so `./fps_shooter` just works.
add_custom_command(TARGET fps_shooter POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/assets"
          "$<TARGET_FILE_DIR:fps_shooter>/assets"
  COMMENT "Copying assets next to the executable")
