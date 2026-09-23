//
// Created by Mehrshad on 8/9/2025.
//

#ifndef MORGHSPICY_APP_H
#define MORGHSPICY_APP_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>
#include <memory>
#include "View/Plotter.h"
#include "View/CircuitGrid.h"
#include "Controller/SimulationRunner.h"
#include "Controller/CommandParser.h"
#include "Controller/Signal.h"
#include "Model/MNASolver.h"
#include "Model/NodeManager.h"
#include "Model/Graph.h"

struct SigUI {
    double Fs      = 10000.0;             // sampling rate for text signals
    double tStop   = 0.1;                 // seconds to display
    int    chunkSz = 8192;                // chunk size for reading
    bool   byChunks = false;              // if true, only the first chunk is shown
    std::string lastPath = "assets/signal_demo.txt";
};

enum class Page {
    Plotter,
    Grid
};

class App {
public:
    App();
    int run();

private:
    bool init();
    void handleEvents();
    void handlePlotterKey(const SDL_Event& e);
    void handleCommandBar(const SDL_Event& e);
    void runCommand(const std::string& line);
    void render();
    void renderCommandBar(int w, int h);
    void drawText(const std::string& text, float x, float y, SDL_Color color = {0, 0, 0, 255});
    void cleanup();
    SDL_FRect plotArea() const;
    void switchPage(Page p);

    void showPlot(const PlotData& pd);
    void loadAndPlotSignal(const std::string& path, double Fs, double tStop, int chunkSize, bool byChunks);

    // math between two series on the same x grid
    static std::vector<Point> combineSameGrid(const std::vector<Point>& a,
                                              const std::vector<Point>& b,
                                              double K, char op);
    static std::vector<Point> scaleSeries(const std::vector<Point>& a, double K);

    bool          isRunning{};
    SDL_Window*   window{};
    SDL_Renderer* renderer{};
    TTF_Font*     mainFont{};

    Graph        graph;
    NodeManager  nodeManager;
    MNASolver    mnaSolver;
    SimulationRunner simRunner;
    CommandParser parser;

    Page currentPage = Page::Grid;
    std::unique_ptr<View::CircuitGrid> gridPage;
    Plotter plotter;

    bool commandActive = false;
    std::string commandInputBuffer;
    std::vector<std::string> history;
    size_t historyPos = 0;

    SigUI sigui{};
};

#endif //MORGHSPICY_APP_H
