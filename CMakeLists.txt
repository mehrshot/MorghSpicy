cmake_minimum_required(VERSION 3.24)
project(MorghSpicy LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Dependencies ---
# Do NOT set CMAKE_PREFIX_PATH for MSYS2; CLion’s MSYS2 toolchain already knows it.
find_package(SDL3 REQUIRED CONFIG)            # provides SDL3::SDL3
# Eigen is optional; use if available.
find_package(Eigen3 QUIET CONFIG)             # provides Eigen3::Eigen when installed

# --- Sources ---
add_executable(MorghSpicy
        main.cpp

        # Controller
        Controller/SimulationRunner.cpp
        Controller/CommandParser.cpp
        Controller/Signal.cpp

        # Model
        Model/Elements.cpp
        Model/NodeManager.cpp
        Model/MNASolver.cpp

        # View
        View/App.cpp
        View/Plotter.cpp
        Controller/Signal.cpp
        Controller/Signal.h
)

# --- Include paths (project headers) ---
target_include_directories(MorghSpicy PRIVATE ${CMAKE_SOURCE_DIR})

# --- Link libraries ---
target_link_libraries(MorghSpicy
        PRIVATE
        SDL3::SDL3
)

# If Eigen3 was found, link it. Otherwise you can skip it or add a fallback include path.
if (Eigen3_FOUND)
    target_link_libraries(MorghSpicy PRIVATE Eigen3::Eigen)
endif()

target_compile_definitions(MorghSpicy PRIVATE SDL_MAIN_HANDLED)
