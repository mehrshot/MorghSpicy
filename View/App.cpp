//
// Created by Mehrshad on 8/9/2025.
//

#include "App.h"
#include <cstdlib>   // for _putenv_s
#if defined(_WIN32)
#include <windows.h>
#endif
#include <iostream>
#include <SDL3/SDL.h>

// In the App.cpp constructor
App::App() : isRunning(false), window(nullptr), renderer(nullptr),
             nodeManager(&graph),
             simRunner(&graph, &mnaSolver, &nodeManager),
             plotter({50.0f, 50.0f, 700.0f, 500.0f}) {}

App::~App() {
    cleanup();
}

int App::run() {
    if (!init()) {
        return -1;
    }

    // Load the circuit file
    CommandParser parser(&graph, &nodeManager, &simRunner);
    parser.parseCommand("load schematics/rc_step.txt");

    // Define simulation parameters
    double stopTime = 5e-3;      // 5ms
    double initialTimestep = 1e-6; // 1us initial step
    double maxTimestep = 1e-5;     // 10us max allowed step

    std::vector<OutputVariable> vars_to_plot;
    vars_to_plot.push_back({OutputVariable::VOLTAGE, "N2"});
    // You can add more variables to plot here
    // vars_to_plot.push_back({OutputVariable::CURRENT, "R1"});

    // Run simulation and get the data for plotting
    // FIX: Using clearer variable names for the parameters.
    PlotData data = simRunner.runTransient(initialTimestep, stopTime, maxTimestep, vars_to_plot);
    plotter.setData(data);

    while (isRunning) {
        handleEvents();
        update();
        render();
    }

    return 0;
}

bool App::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    window = SDL_CreateWindow("MorghSpicy Plotter", 800, 600, 0);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
    renderer = SDL_CreateRendererWithProperties(props);
    SDL_DestroyProperties(props);

    if (!renderer) {
        std::cerr << "SDL_CreateRendererWithProperties Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    isRunning = true;
    return true;
}

void App::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            isRunning = false;
        }
    }
}

void App::update() {
    // در این پروژه ساده، منطق آپدیت خاصی نداریم
}

void App::render() {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    plotter.render(renderer);

    SDL_RenderPresent(renderer);
}

void App::cleanup() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}