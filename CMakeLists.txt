cmake_minimum_required(VERSION 3.20)

project(
	ChocolatePoise
	VERSION 1.1.0
	LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(XSEPlugin)

target_compile_options(ChocolatePoise PRIVATE
    /wd4305
)

add_compile_definitions(
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    UNICODE
    _UNICODE
)

add_custom_command(TARGET ChocolatePoise POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "$<TARGET_FILE:ChocolatePoise>"
    "E:/ModLists/Skyrim Modding Setup/mods/Chocolate Poise - Reforged/skse/plugins/"

    COMMAND ${CMAKE_COMMAND} -E echo "Build finished"
)