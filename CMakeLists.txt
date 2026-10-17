# Go Editor on Linux. (Windows builds with FASTBuild: fastbuild/fbuild.bff.)
#
# Needs a C++20 compiler, CMake, and what GLFW needs for X11 and OpenGL -- on
# Debian or Ubuntu:
#   sudo apt install cmake pkg-config libx11-dev libxrandr-dev libxinerama-dev \
#                    libxcursor-dev libxi-dev libgl1-mesa-dev
# Then, from the repo root:
#   cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/linux -j
# and the program is build/linux/kovarex_go_editor.
cmake_minimum_required(VERSION 3.20)
project(kovarex_go_editor C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
# Agui keeps extra debug bookkeeping in its classes under DEBUG, so everything
# that includes its headers has to agree on it.
add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

#------------------------------------------------------------------------------
# What resources/ holds is compiled into the program, as fbuild.bff does it:
# tools/Embed.cpp turns each file into a .cpp of its bytes.
#------------------------------------------------------------------------------
add_executable(Embed tools/Embed.cpp)
add_executable(PrepareWood tools/PrepareWood.cpp)
target_include_directories(PrepareWood SYSTEM PRIVATE libraries/raylib/external)

set(GENERATED ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED})
set(EMBEDDED)
function(embed name input symbol)
  add_custom_command(
    OUTPUT ${GENERATED}/${name}.cpp
    COMMAND Embed ${input} ${GENERATED}/${name}.cpp ${symbol} ui
    DEPENDS Embed ${input}
    VERBATIM)
  set(EMBEDDED ${EMBEDDED} ${GENERATED}/${name}.cpp PARENT_SCOPE)
endfunction()

set(R ${CMAKE_SOURCE_DIR}/resources)
embed(GuiAtlas       ${R}/gui.png                          GUI_ATLAS_PNG)
embed(FontRegular    ${R}/fonts/TitilliumWeb-Regular.ttf   FONT_REGULAR_TTF)
embed(FontSemiBold   ${R}/fonts/TitilliumWeb-SemiBold.ttf  FONT_SEMIBOLD_TTF)
embed(FontBold       ${R}/fonts/TitilliumWeb-Bold.ttf      FONT_BOLD_TTF)
embed(IconInfo       ${R}/icons/info-blue.png              ICON_INFO_PNG)
embed(IconReset      ${R}/icons/reset.png                  ICON_RESET_PNG)
embed(IconResetWhite ${R}/icons/reset-white.png            ICON_RESET_WHITE_PNG)
embed(IconSearch     ${R}/icons/search-white.png           ICON_SEARCH_PNG)
embed(AppIcon        ${R}/icon/goeditor-256.png            APP_ICON_PNG)

# The board's wood, turned and graded by tools/PrepareWood.cpp first.
add_custom_command(
  OUTPUT ${GENERATED}/BoardWood.jpg
  COMMAND PrepareWood ${R}/wood/Wood048.jpg ${GENERATED}/BoardWood.jpg
  DEPENDS PrepareWood ${R}/wood/Wood048.jpg
  VERBATIM)
embed(BoardWood ${GENERATED}/BoardWood.jpg BOARD_WOOD_JPG)

#------------------------------------------------------------------------------
# raylib 5.5, vendored in libraries/raylib: desktop, OpenGL 3.3.
# Its config leaves JPG out; the board's wood is one.
#------------------------------------------------------------------------------
add_library(raylib STATIC
  libraries/raylib/rcore.c libraries/raylib/rshapes.c libraries/raylib/rtextures.c
  libraries/raylib/rtext.c libraries/raylib/rmodels.c libraries/raylib/raudio.c
  libraries/raylib/utils.c libraries/raylib/rglfw.c)
# GLFW on X11, which every Linux desktop has -- Wayland ones through XWayland.
target_compile_definitions(raylib PUBLIC PLATFORM_DESKTOP GRAPHICS_API_OPENGL_33 SUPPORT_FILEFORMAT_JPG=1 _GLFW_X11)
target_include_directories(raylib SYSTEM PUBLIC libraries/raylib libraries/raylib/external/glfw/include)
target_compile_options(raylib PRIVATE -w)
find_package(Threads REQUIRED)
target_link_libraries(raylib PUBLIC GL X11 m dl rt Threads::Threads)

#------------------------------------------------------------------------------
# Agui: the vendored GUI library, included as <Agui/...>.
#------------------------------------------------------------------------------
file(GLOB_RECURSE AGUI_SOURCES CONFIGURE_DEPENDS libraries/Agui/*.cpp)
add_library(agui STATIC ${AGUI_SOURCES})
target_include_directories(agui SYSTEM PUBLIC libraries)
target_compile_options(agui PRIVATE -w)

#------------------------------------------------------------------------------
# The editor.
#------------------------------------------------------------------------------
# All of it but main(), as a library the tests link too.
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(REMOVE_ITEM EDITOR_SOURCES ${CMAKE_SOURCE_DIR}/src/main.cpp)
add_library(editor STATIC ${EDITOR_SOURCES} ${EMBEDDED})
target_include_directories(editor PUBLIC src)
target_compile_options(editor PRIVATE -Wall)
target_link_libraries(editor PUBLIC agui raylib)

add_executable(kovarex_go_editor src/main.cpp)
target_compile_options(kovarex_go_editor PRIVATE -Wall)
target_link_libraries(kovarex_go_editor PRIVATE editor)
# The C++ runtime built in, so the program runs on distributions older than
# the compiler -- whose libstdc++ they wouldn't have.
target_link_options(kovarex_go_editor PRIVATE -static-libstdc++ -static-libgcc)

#------------------------------------------------------------------------------
# The tests: UnitTest++ as Factorio has it, in libraries/UnitTest++, and the
# scenarios in tests/ that play the editor through its own window. Run
# build/linux/kovarex_go_editor_tests -- under xvfb-run where there is no
# screen.
#------------------------------------------------------------------------------
file(GLOB UNITTEST_SOURCES CONFIGURE_DEPENDS libraries/UnitTest++/*.cpp libraries/UnitTest++/Posix/*.cpp)
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(kovarex_go_editor_tests ${TEST_SOURCES} ${UNITTEST_SOURCES})
target_include_directories(kovarex_go_editor_tests PRIVATE tests libraries/UnitTest++)
target_link_libraries(kovarex_go_editor_tests PRIVATE editor)

#------------------------------------------------------------------------------
# The relay: a server for sharing games through, needing none of the above.
# relay/build.sh builds it on its own, where there is no CMake (or X11).
#------------------------------------------------------------------------------
add_executable(go_relay relay/Relay.cpp src/net/Hub.cpp src/net/Socket.cpp src/net/WebSocket.cpp)
target_include_directories(go_relay PRIVATE src)
target_compile_options(go_relay PRIVATE -Wall)
target_link_libraries(go_relay PRIVATE Threads::Threads)
target_link_options(go_relay PRIVATE -static-libstdc++ -static-libgcc)
