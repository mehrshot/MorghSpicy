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
#include "View/CircuitGrid.h"
#include "Button.h"
#include "Controller/CommandParser.h"

#include <SDL3_ttf/SDL_ttf.h>


struct SigUI {
    double Fs      = 10000.0;             // sampling rate for text signals
    double tStop   = 0.1;                 // seconds to display
    int    chunkSz = 8192;                // chunk size for reading
    bool   byChunks = false;              // if true, overlay only first chunk (demo streaming)
    std::string lastPath = "schematics/rc_signal.txt";
};
enum class Page {
    Plotter,
    Grid
};

//enum class ToolKind { None, Wire, Resistor, Capacitor, Inductor, VSource, ISource };
//struct UiElement {
//    std::string name;
//    ToolKind kind;
//    SDL_FRect bbox;
//    int n1;
//    int n2;
//    std::string value;
//};

class App {
public:
    App();
    int run();

private:
    Page currentPage = Page::Grid; // پیش‌فرض بفرستیم روی گرید
    std::unique_ptr<View::CircuitGrid> gridPage;
    bool init();
    void handleEvents();
    void update();
    void render();
    void renderSchematic();
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

//    SDL_FPoint snapToGrid(const SDL_FPoint& p) const;
//    float gridSize = 20.0f;
//    int nextNodeId = 1;
//    std::unordered_map<long long,int> gridNode;
//
//    ToolKind currentTool = ToolKind::None;
//    bool placing = false;
//    int placeNode1 = -1;
//    SDL_FPoint placePos1{0,0};
//    std::vector<UiElement> uiElements;
//    uint64_t lastClickTicks = 0;
//    SDL_FPoint lastClickPos{0,0};
//
//    int findOrCreateNodeAt(const SDL_FPoint& p);
//    int hitElementIndexAt(const SDL_FPoint& p) const;
//    std::string promptText(const std::string& title, const std::string& initial);
//    void applyPlacement(ToolKind kind, int n1, int n2, const SDL_FPoint& a, const SDL_FPoint& b);
//    void requestElementEdit(int idx);
//    void handleMouseDown(const SDL_MouseButtonEvent& e);
private:
    bool         isRunning{};
    SDL_Window*  window{};
    SDL_Renderer* renderer{};
    TTF_Font* mainFont{};

    Graph        graph;
    NodeManager  nodeManager;
    MNASolver    mnaSolver;
    SimulationRunner simRunner{&graph, &mnaSolver, &nodeManager};

    CommandParser parser;

    Plotter      plotter{ SDL_FRect{60, 40, 700, 500} };
    std::string commandInputBuffer;
    SigUI        sigui{};
    Button simSettingsButton;
    bool showSimSettingsWindow = false;
};

#endif //MORGHSPICY_APP_H