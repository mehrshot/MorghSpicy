cmake_minimum_required(VERSION 3.30)
project(MorghSpicy)

set(CMAKE_CXX_STANDARD 20)

set(PCH_FILE Common_Includes.h)

add_executable(MorghSpicy main.cpp
        Model/Node.h
        Model/Edge.h
        Model/Graph.h
        Model/ElementTypes.h
        Model/Elements.h
)

target_precompile_headers(MorghSpicy PRIVATE ${PCH_FILE})

target_include_directories(MorghSpicy PRIVATE ${CMAKE_SOURCE_DIR})
