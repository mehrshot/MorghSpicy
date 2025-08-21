cmake_minimum_required(VERSION 3.24)
project(MorghSpicy LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Packages ---
find_package(SDL3 REQUIRED CONFIG)
find_package(Eigen3 QUIET CONFIG)

# --- Sources ---
add_executable(MorghSpicy
        main.cpp

        # Controller
        Controller/SimulationRunner.cpp
        Controller/CommandParser.cpp
        Controller/Signal.cpp
        Controller/Signal.h

        # Model
        Model/Elements.cpp
        Model/NodeManager.cpp
        Model/MNASolver.cpp
        Model/Graph.cpp

        # View
        View/App.cpp
        View/Plotter.cpp
        View/CircuitGrid.cpp
        View/CircuitGrid.h
)

# --- Include paths (project headers) ---
target_include_directories(MorghSpicy PRIVATE ${CMAKE_SOURCE_DIR})

# --- SDL libraries ---
find_library(SDL3_TTF_LIB SDL3_ttf
        PATHS "C:/msys64/ucrt64/lib"
)

target_link_libraries(MorghSpicy
        PRIVATE
        SDL3::SDL3
        ${SDL3_TTF_LIB}
)

# --- Eigen (optional) ---
if (Eigen3_FOUND)
    target_link_libraries(MorghSpicy PRIVATE Eigen3::Eigen)
endif()

target_compile_definitions(MorghSpicy PRIVATE SDL_MAIN_HANDLED)
