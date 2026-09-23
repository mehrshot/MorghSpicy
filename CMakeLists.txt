cmake_minimum_required(VERSION 3.24)
project(MorghSpicy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- Packages ---
find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)
find_package(Eigen3 REQUIRED CONFIG)

add_executable(MorghSpicy
        main.cpp

        # Controller
        Controller/CommandParser.cpp
        Controller/SimulationRunner.cpp
        Controller/Signal.cpp

        # Model
        Model/Elements.cpp
        Model/Graph.cpp
        Model/MNASolver.cpp
        Model/NodeManager.cpp

        # View
        View/App.cpp
        View/CircuitGrid.cpp
        View/Plotter.cpp
)

target_include_directories(MorghSpicy PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(MorghSpicy PRIVATE
        SDL3::SDL3
        SDL3_ttf::SDL3_ttf
        Eigen3::Eigen
)

target_compile_definitions(MorghSpicy PRIVATE SDL_MAIN_HANDLED)

if (MSVC)
    target_compile_options(MorghSpicy PRIVATE /W3 /utf-8)
else()
    target_compile_options(MorghSpicy PRIVATE -Wall)
endif()

# font and example circuits next to the exe
add_custom_command(TARGET MorghSpicy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:MorghSpicy>/assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/schematics $<TARGET_FILE_DIR:MorghSpicy>/schematics
)

# on windows copy SDL dlls too, so the exe can be started with a double click
if (WIN32)
    add_custom_command(TARGET MorghSpicy POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:MorghSpicy> $<TARGET_FILE_DIR:MorghSpicy>
            COMMAND_EXPAND_LISTS
    )
endif()
