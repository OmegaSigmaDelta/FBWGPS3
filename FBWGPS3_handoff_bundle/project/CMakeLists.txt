cmake_minimum_required(VERSION 3.16)
project(fbw_ps3_port LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fbw_core
	src/level.cpp
	src/ground.cpp
)
target_include_directories(fbw_core PUBLIC src)

if(MSVC)
	target_compile_options(fbw_core PRIVATE /W4)
else()
	target_compile_options(fbw_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(test_input tests/test_input.cpp)
target_include_directories(test_input PRIVATE src)

add_executable(test_level tests/test_level.cpp)
target_link_libraries(test_level PRIVATE fbw_core)

add_executable(dump_ground tests/dump_ground.cpp)
target_link_libraries(dump_ground PRIVATE fbw_core)

add_executable(test_atlas tests/test_atlas.cpp)
target_include_directories(test_atlas PRIVATE src)

add_executable(test_bitmap_font tests/test_bitmap_font.cpp)
target_include_directories(test_bitmap_font PRIVATE src)

add_executable(test_render_queue tests/test_render_queue.cpp)
target_link_libraries(test_render_queue PRIVATE fbw_core)

add_executable(test_world_render tests/test_world_render.cpp)
target_link_libraries(test_world_render PRIVATE fbw_core)

add_executable(test_character_animation tests/test_character_animation.cpp)
target_include_directories(test_character_animation PRIVATE src)

add_executable(test_character_controller tests/test_character_controller.cpp)
target_include_directories(test_character_controller PRIVATE src)

add_executable(test_progression tests/test_progression.cpp)
target_include_directories(test_progression PRIVATE src)

add_executable(test_progression_catalog tests/test_progression_catalog.cpp)
target_include_directories(test_progression_catalog PRIVATE src)
target_link_libraries(test_progression_catalog PRIVATE fbw_core)

add_executable(test_menu_navigation tests/test_menu_navigation.cpp)
target_include_directories(test_menu_navigation PRIVATE src)

add_executable(test_gameplay_runtime tests/test_gameplay_runtime.cpp)
target_link_libraries(test_gameplay_runtime PRIVATE fbw_core)

add_executable(test_level_session tests/test_level_session.cpp)
target_link_libraries(test_level_session PRIVATE fbw_core)

add_executable(test_physics_world tests/test_physics_world.cpp)
target_include_directories(test_physics_world PRIVATE src)

add_executable(test_game_world tests/test_game_world.cpp)
target_link_libraries(test_game_world PRIVATE fbw_core)

enable_testing()
add_test(NAME input_layer COMMAND test_input)
add_test(NAME all_level_maps COMMAND test_level "${CMAKE_SOURCE_DIR}/game")
add_test(NAME all_texture_atlases COMMAND test_atlas "${CMAKE_SOURCE_DIR}/game")
add_test(NAME bitmap_font_parser COMMAND test_bitmap_font)
add_test(NAME engine_neutral_render_queue COMMAND test_render_queue "${CMAKE_SOURCE_DIR}/game")
add_test(NAME world_object_render_plan COMMAND test_world_render "${CMAKE_SOURCE_DIR}/game")
add_test(NAME character_animation COMMAND test_character_animation "${CMAKE_SOURCE_DIR}/game")
add_test(NAME character_controller COMMAND test_character_controller)
add_test(NAME progression_rules COMMAND test_progression)
add_test(NAME progression_catalog_and_save COMMAND test_progression_catalog
	"${CMAKE_SOURCE_DIR}/game" "${CMAKE_BINARY_DIR}/progress_test_save.json")
add_test(NAME menu_focus_navigation COMMAND test_menu_navigation)
add_test(NAME gameplay_contact_rules COMMAND test_gameplay_runtime "${CMAKE_SOURCE_DIR}/game")
add_test(NAME level_session_integration COMMAND test_level_session "${CMAKE_SOURCE_DIR}/game")
add_test(NAME fallback_physics_world COMMAND test_physics_world)
add_test(NAME integrated_game_world COMMAND test_game_world "${CMAKE_SOURCE_DIR}/game")

find_package(PNG QUIET)
if(PNG_FOUND)
	add_library(fbw_software_renderer src/software_renderer.cpp)
	target_include_directories(fbw_software_renderer PUBLIC src)
	target_link_libraries(fbw_software_renderer PUBLIC fbw_core PNG::PNG)
	if(MSVC)
		target_compile_options(fbw_software_renderer PRIVATE /W4)
	else()
		target_compile_options(fbw_software_renderer PRIVATE -Wall -Wextra -Wpedantic)
	endif()
	add_executable(render_level_preview tests/render_level_preview.cpp)
	target_link_libraries(render_level_preview PRIVATE fbw_software_renderer)
	add_executable(test_software_renderer tests/test_software_renderer.cpp)
	target_link_libraries(test_software_renderer PRIVATE fbw_software_renderer)
	add_test(NAME software_renderer_pixels COMMAND test_software_renderer)
	add_test(NAME software_render_preview COMMAND render_level_preview
		"${CMAKE_SOURCE_DIR}/game" "tutorials/levels/forest_01.json"
		"${CMAKE_BINARY_DIR}/forest_01_preview.png")
endif()

# Optional SDL2 desktop runtime. SDL headers are not required by the portable
# core; the tiny ABI declaration in src/sdl_abi.hpp keeps the backend buildable
# on minimal systems that have the SDL2 runtime library but no development headers.
if(PNG_FOUND)
    find_library(FBW_SDL2_LIBRARY NAMES SDL2 SDL2-2.0 SDL2-2.0.so.0)
    if(NOT FBW_SDL2_LIBRARY)
        find_file(FBW_SDL2_LIBRARY NAMES libSDL2-2.0.so.0 PATHS /lib /usr/lib /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu)
    endif()
    if(FBW_SDL2_LIBRARY)
        add_executable(fbw_game src/game_main.cpp)
        target_include_directories(fbw_game PRIVATE src)
        target_link_libraries(fbw_game PRIVATE fbw_software_renderer ${FBW_SDL2_LIBRARY})
        add_test(NAME desktop_runtime_startup COMMAND fbw_game "${CMAKE_SOURCE_DIR}/game" --validate)
        add_test(NAME desktop_runtime_all_levels COMMAND fbw_game "${CMAKE_SOURCE_DIR}/game" --validate-all)
        if(MSVC)
            target_compile_options(fbw_game PRIVATE /W4)
        else()
            target_compile_options(fbw_game PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endif()
endif()
