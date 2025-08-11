//
// Created by Mehrshad on 8/9/2025.
//

#ifndef MORGHSPICY_APP_H
#define MORGHSPICY_APP_H

#pragma once
#include <SDL3/SDL.h>
#include <string>
#include "View/Plotter.h"
#include "Controller/SimulationRunner.h"
#include "Controller/Signal.h"
#include "Model/MNASolver.h"
#include "Model/NodeManager.h"
#include "Model/Graph.h"

struct SigUI {
    double Fs      = 10000.0;             // sampling rate for text signals
    double tStop   = 0.1;                 // seconds to display
    int    chunkSz = 8192;                // chunk size for reading
    bool   byChunks = false;              // if true, overlay only first chunk (demo streaming)
    std::string lastPath = "schematics/rc_signal.txt";
};

class App {
public:
    App();
    int run();

private:
    bool init();
    void handleEvents();
    void update();
    void render();
    void cleanup();

    // Plot bridges
    void showPlot(const PlotData& pd);
    void loadAndPlotSignal(const std::string& path, double Fs, double tStop, int chunkSize);
    void loadAndPlotSignal(const std::string& path, double Fs, double tStop, int chunkSize, bool byChunks);

    // Small helpers for math ops
    static std::vector<Point> combineSameGrid(const std::vector<Point>& a,
                                              const std::vector<Point>& b,
                                              double K, char op);
    static std::vector<Point> scaleSeries(const std::vector<Point>& a, double K);

private:
    bool         isRunning{};
    SDL_Window*  window{};
    SDL_Renderer* renderer{};

    Graph        graph;
    NodeManager  nodeManager{&graph};
    MNASolver    mnaSolver;
    SimulationRunner simRunner{&graph, &mnaSolver, &nodeManager};

    Plotter      plotter{ SDL_FRect{60, 40, 700, 500} };
    SigUI        sigui{};
};

#endif //MORGHSPICY_APP_H