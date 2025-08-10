//
// Created by Mehrshad on 8/9/2025.
//

#ifndef MORGHSPICY_PLOTTER_H
#define MORGHSPICY_PLOTTER_H


#pragma once
#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include "Controller/SimulationRunner.h"

struct Point {
    double x, y;
};

class Plotter {
public:
    Plotter(SDL_FRect plotArea);
    void setData(const PlotData& data);
    void render(SDL_Renderer* renderer);

private:
    SDL_FRect area;
    std::vector<std::vector<Point>> seriesPoints;
    double minX, maxX, minY, maxY;
};


#endif //MORGHSPICY_PLOTTER_H