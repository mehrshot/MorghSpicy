cmake_minimum_required(VERSION 3.29.6)
project(MorghSpicy)

set(CMAKE_CXX_STANDARD 20)

add_executable(MorghSpicy main.cpp
        Model/Node.h
        Model/Edge.h
        Model/Graph.h
        Model/ElementTypes.h
        Model/Elements.h
)
