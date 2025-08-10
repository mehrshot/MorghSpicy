//
// Created by Mehrshad on 8/9/2025.
//

#ifndef MORGHSPICY_APP_H
#define MORGHSPICY_APP_H


#pragma once
#include <SDL3/SDL.h>
#include "View/Plotter.h" // FIX: Corrected include path
#include "Controller/SimulationRunner.h" // FIX: Corrected include path
#include "Model/Graph.h" // FIX: Corrected include path
#include "Model/NodeManager.h" // FIX: Corrected include path
#include "Controller/CommandParser.h" // FIX: Corrected include path


class App {
public:
    App();
    ~App();

    int run();

private:
    bool init();
    void handleEvents();
    void update();
    void render();
    void cleanup();

    bool isRunning;
    SDL_Window* window;
    SDL_Renderer* renderer;

    Graph graph;
    NodeManager nodeManager;
    MNASolver mnaSolver;
    SimulationRunner simRunner;

    Plotter plotter;
};


#endif //MORGHSPICY_APP_H