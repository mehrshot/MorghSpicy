cmake_minimum_required(VERSION 3.30)
project(MorghSpicy CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(MorghSpicy
        main.cpp
        Model/Elements.cpp
        Model/MNASolver.cpp
)

target_include_directories(MorghSpicy PRIVATE ${CMAKE_SOURCE_DIR})

set(EIGEN_INCLUDE_PATH "C:/msys64/ucrt64/include")

target_include_directories(MorghSpicy PRIVATE ${Eigen3_INCLUDE_DIRS})


