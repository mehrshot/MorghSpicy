//
// Created by Mehrshad on 8/9/2025.
//

#include <iostream>
#include <algorithm>
#include <sstream>

#include "View/App.h"

static std::vector<Point> toSeries(const std::vector<double>& xs, const std::vector<double>& ys) {
    std::vector<Point> out;
    const size_t n = std::min(xs.size(), ys.size());
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) out.push_back({ xs[i], ys[i] });
    return out;
}

std::vector<Point> App::combineSameGrid(const std::vector<Point>& a, const std::vector<Point>& b, double K, char op) {
    std::vector<Point> out;
    const size_t n = std::min(a.size(), b.size());
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        double y = 0.0;
        if (op == '+') y = a[i].y + b[i].y;
        else if (op == '-') y = a[i].y - b[i].y;
        else if (op == '*') y = a[i].y * b[i].y;
        out.push_back({ a[i].x, K * y });
    }
    return out;
}

std::vector<Point> App::scaleSeries(const std::vector<Point>& a, double K) {
    std::vector<Point> out;
    out.reserve(a.size());
    for (const auto& p : a) out.push_back({ p.x, K * p.y });
    return out;
}

App::App() : simRunner(&graph, &mnaSolver, &nodeManager),
             parser(&graph, &nodeManager, &simRunner) {}

bool App::init() {
    // SDL3 functions return true on success
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }
    if (!TTF_Init()) {
        std::cerr << "TTF_Init failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return false;
    }

    window = SDL_CreateWindow("MorghSpicy", 1100, 720, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(renderer, 1);

    // font is copied next to the exe by cmake, but try the working dir too
    std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "";
    for (const std::string& path : { base + "assets/roboto.ttf", std::string("assets/roboto.ttf"),
                                     std::string("C:/Windows/Fonts/arial.ttf") }) {
        mainFont = TTF_OpenFont(path.c_str(), 15);
        if (mainFont) break;
    }
    if (!mainFont) {
        std::cerr << "Warning: no font found (assets/roboto.ttf), text won't be shown.\n";
    }

    plotter = Plotter(plotArea(), mainFont);
    gridPage = std::make_unique<View::CircuitGrid>(window, &graph, &nodeManager, &parser, mainFont);

    isRunning = true;
    return true;
}

void App::cleanup() {
    gridPage.reset();
    if (mainFont) { TTF_CloseFont(mainFont); mainFont = nullptr; }
    if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
    if (window) { SDL_DestroyWindow(window); window = nullptr; }
    TTF_Quit();
    SDL_Quit();
}

SDL_FRect App::plotArea() const {
    int w = 1100, h = 720;
    if (window) SDL_GetWindowSize(window, &w, &h);
    return SDL_FRect{ 80.f, 40.f, float(w - 110), float(h - 130) };
}

void App::switchPage(Page p) {
    if (p == currentPage) return;
    if (p == Page::Plotter) {
        gridPage->commitToModel();
    } else {
        commandActive = false;
        SDL_StopTextInput(window);
    }
    currentPage = p;
}

void App::showPlot(const PlotData& pd) {
    if (pd.empty()) return;

    // different kind of analysis -> start a fresh plot
    if (pd.x_label != plotter.xLabel() || pd.log_x != plotter.isLogX()) {
        plotter.clear();
        plotter.setXAxis(pd.x_label, pd.log_x);
    }

    std::vector<std::vector<Point>> added;
    for (size_t k = 0; k < pd.data_series.size(); ++k) {
        auto s = toSeries(pd.time_axis, pd.data_series[k]);
        added.push_back(s);
        plotter.addSeries(k < pd.series_names.size() ? pd.series_names[k] : "sig_" + std::to_string(k), s);
    }

    // "plot v a b" also shows V(a)-V(b)
    if (pd.series_names.size() == 2 &&
        pd.series_names[0].rfind("V(", 0) == 0 && pd.series_names[1].rfind("V(", 0) == 0) {
        plotter.addSeries(pd.series_names[0] + "-" + pd.series_names[1],
                          combineSameGrid(added[0], added[1], 1.0, '-'));
    }
    switchPage(Page::Plotter);
}

void App::loadAndPlotSignal(const std::string& path, double Fs, double tStop, int chunkSize, bool byChunks) {
    Signal s(path, Fs, tStop, chunkSize);
    std::vector<std::pair<double,double>> pairs;

    if (!byChunks) {
        pairs = s.readAllAsPoints();
    } else if (s.open()) {
        // only the first chunk, to show that streaming works
        if (s.readNextChunk()) s.appendCurrentChunkAsPoints(pairs, 0.0);
        s.close();
    }
    if (pairs.empty()) {
        std::cerr << "[Signal] Cannot read: " << path << "\n";
        return;
    }

    std::vector<Point> pts;
    pts.reserve(pairs.size());
    for (auto& pr : pairs) pts.push_back({ pr.first, pr.second });

    if (plotter.xLabel() != "t (s)" || plotter.isLogX()) {
        plotter.clear();
        plotter.setXAxis("t (s)", false);
    }
    plotter.addSeries(byChunks ? path + " (chunk)" : path, pts);
    sigui.lastPath = path;
    std::cout << "[Signal] Loaded " << pts.size() << " samples from " << path << "\n";
    switchPage(Page::Plotter);
}

void App::runCommand(const std::string& line) {
    std::cout << "> " << line << std::endl;
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    // a few commands only make sense for the plot window, the rest go to the parser
    if (cmd == "relabel") {
        size_t idx = 0;
        std::string newName;
        if (iss >> idx && std::getline(iss >> std::ws, newName) && !newName.empty()) {
            if (plotter.setSeriesName(idx - 1, newName)) std::cout << "Relabeled series " << idx << " to '" << newName << "'\n";
            else std::cout << "Error: Invalid series index '" << idx << "'\n";
        } else {
            std::cout << "Usage: relabel <index> <new name>\n";
        }
    } else if (cmd == "clear") {
        plotter.clear();
    } else if (cmd == "grid") {
        switchPage(Page::Grid);
    } else {
        parser.parseCommand(line);
    }
}

void App::handleCommandBar(const SDL_Event& e) {
    if (e.type == SDL_EVENT_TEXT_INPUT) {
        commandInputBuffer += e.text.text;
        return;
    }
    if (e.type != SDL_EVENT_KEY_DOWN) return;

    switch (e.key.key) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (!commandInputBuffer.empty()) {
                std::string line = commandInputBuffer;
                history.push_back(line);
                historyPos = history.size();
                commandInputBuffer.clear();
                runCommand(line);
            }
            commandActive = false;
            SDL_StopTextInput(window);
            break;
        case SDLK_ESCAPE:
            commandActive = false;
            commandInputBuffer.clear();
            SDL_StopTextInput(window);
            break;
        case SDLK_BACKSPACE:
            if (!commandInputBuffer.empty()) commandInputBuffer.pop_back();
            break;
        case SDLK_UP:
            if (historyPos > 0) commandInputBuffer = history[--historyPos];
            break;
        case SDLK_DOWN:
            if (historyPos + 1 < history.size()) commandInputBuffer = history[++historyPos];
            else { historyPos = history.size(); commandInputBuffer.clear(); }
            break;
        default:
            break;
    }
}

void App::handlePlotterKey(const SDL_Event& e) {
    const auto& ss = plotter.allSeries();
    switch (e.key.key) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            commandActive = true;
            SDL_StartTextInput(window);
            return;
        case SDLK_1: case SDLK_2: case SDLK_3:
        case SDLK_4: case SDLK_5: case SDLK_6:
        case SDLK_7: case SDLK_8: case SDLK_9: {
            int digit = int(e.key.key - SDLK_0);
            if (plotter.selectByIndex(digit - 1)) {
                std::cout << "[Legend] selected #" << digit << " -> " << plotter.getSelectedName() << "\n";
            }
            return;
        }
        case SDLK_V: plotter.toggleSelectedVisibility(); return;
        case SDLK_DELETE: plotter.removeSelected(); return;
        case SDLK_G: plotter.setLegendVisible(!plotter.isLegendVisible()); return;
        case SDLK_B:
            if (auto idx = plotter.getSelectedIndex()) plotter.cycleSeriesColor(*idx);
            return;
        case SDLK_X: plotter.clear(); return;
        case SDLK_M:   // sum of the last two
            if (ss.size() >= 2) {
                auto C = combineSameGrid(ss[ss.size()-2].points, ss.back().points, 1.0, '+');
                plotter.addSeries(ss[ss.size()-2].name + "+" + ss.back().name, C);
            }
            return;
        case SDLK_N:   // difference of the last two
            if (ss.size() >= 2) {
                auto C = combineSameGrid(ss[ss.size()-2].points, ss.back().points, 1.0, '-');
                plotter.addSeries(ss[ss.size()-2].name + "-" + ss.back().name, C);
            }
            return;
        case SDLK_K:
            if (!ss.empty()) plotter.addSeries(ss.back().name + "*2", scaleSeries(ss.back().points, 2.0));
            return;
        case SDLK_L:
            loadAndPlotSignal(sigui.lastPath, sigui.Fs, sigui.tStop, sigui.chunkSz, sigui.byChunks);
            return;
        case SDLK_S:
            sigui.byChunks = !sigui.byChunks;
            std::cout << "[Signal] read by chunks: " << (sigui.byChunks ? "on" : "off") << "\n";
            return;
        default:
            plotter.handleEvent(e);   // zoom / cursor keys
            return;
    }
}

void App::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            isRunning = false;
            continue;
        }
        if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            plotter.setArea(plotArea());
            continue;
        }
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F2) {
            switchPage(currentPage == Page::Grid ? Page::Plotter : Page::Grid);
            continue;
        }

        if (currentPage == Page::Grid) {
            gridPage->handleEvent(e);
        } else if (commandActive) {
            handleCommandBar(e);
        } else if (e.type == SDL_EVENT_KEY_DOWN) {
            handlePlotterKey(e);
        } else {
            plotter.handleEvent(e);   // mouse
        }
    }
}

void App::drawText(const std::string& text, float x, float y, SDL_Color color) {
    if (!mainFont || text.empty()) return;
    SDL_Surface* surface = TTF_RenderText_Blended(mainFont, text.c_str(), 0, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_FRect dst = { x, y, (float)surface->w, (float)surface->h };
        SDL_RenderTexture(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_DestroySurface(surface);
}

void App::renderCommandBar(int w, int h) {
    SDL_FRect bar{ 0, (float)h - 30, (float)w, 30 };
    SDL_SetRenderDrawColor(renderer, commandActive ? 255 : 230, commandActive ? 255 : 230, commandActive ? 255 : 230, 255);
    SDL_RenderFillRect(renderer, &bar);
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_RenderLine(renderer, 0, bar.y, (float)w, bar.y);

    if (commandActive) {
        std::string shown = "> " + commandInputBuffer;
        // blinking cursor
        if ((SDL_GetTicks() / 500) % 2 == 0) shown += "_";
        drawText(shown, 8, bar.y + 6);
    } else {
        drawText("Press Enter to type a command (help, add, print TRAN ..., ac ..., plot v N1)", 8, bar.y + 6, {90, 90, 90, 255});
    }
}

void App::render() {
    SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
    SDL_RenderClear(renderer);

    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);

    if (currentPage == Page::Grid) {
        gridPage->render(renderer);
    } else {
        plotter.render(renderer);
        drawText("F2 grid | 1-9 select  V hide  Del remove  B color  G legend | C/D cursors  A auto zoom  [ ] - = zoom | M/N/K math  X clear",
                 10, 10, {70, 70, 70, 255});
        renderCommandBar(w, h);
    }

    SDL_RenderPresent(renderer);
}

int App::run() {
    if (!init()) return -1;

    parser.onPlotData = [this](const PlotData& pd) { showPlot(pd); };
    parser.onScopeLoad = [this](const std::string& p, double Fs, double t, int c) {
        loadAndPlotSignal(p, Fs, t, c, false);
    };
    parser.onScopeClear = [this]() { plotter.clear(); };

    std::cout << "MorghSpicy - draw on the grid, F2 switches to the plot/command page, type 'help' there.\n";

    while (isRunning) {
        handleEvents();
        render();
        SDL_Delay(5);
    }

    cleanup();
    return 0;
}
