//
// Created by Mehrshad on 8/9/2025.
//

#include <iostream>
#include <algorithm>
#include <SDL3/SDL.h>

#include "View/App.h"
#include "Controller/CommandParser.h"
#include "Controller/SimulationRunner.h"
#include "Model/NodeManager.h"
#include "Model/Elements.h"
#include "View/CircuitGrid.h"


std::vector<Point> App::combineSameGrid(const std::vector<Point>& a,
                                        const std::vector<Point>& b,
                                        double K, char op) {
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
    std::vector<Point> out; out.reserve(a.size());
    for (const auto& p : a) out.push_back({ p.x, K * p.y });
    return out;
}

static std::vector<Point> toSeries(const std::vector<double>& xs,
                                   const std::vector<double>& ys) {
    std::vector<Point> out;
    const size_t n = std::min(xs.size(), ys.size());
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) out.push_back({ xs[i], ys[i] });
    return out;
}

// ---------------------------------------------------------------

App::App() : simRunner(&graph, &mnaSolver, &nodeManager) {}

bool App::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {               // SDL3: returns bool
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    window = SDL_CreateWindow("MorghSpicy", 900, 650, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_SetWindowPosition(window, 100, 100);

    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        return false;
    }

    if (!TTF_Init()) {
        std::cerr << "TTF_Init failed: " << SDL_GetError() << "\n";
    }
    mainFont = TTF_OpenFont("assets/roboto.ttf", 16);
    if (!mainFont) mainFont = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 16);
    simSettingsButton.setFont(mainFont);


    gridPage = std::make_unique<View::CircuitGrid>(window, &graph, &nodeManager, &parser);



    isRunning = true;
    return true;
}

void App::cleanup() {
    if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
    if (window)   { SDL_DestroyWindow(window);     window   = nullptr; }
    SDL_Quit();
}

void App::showPlot(const PlotData& pd) {
    plotter.clear();
    std::vector<std::vector<Point>> added;
    for (size_t k = 0; k < pd.data_series.size(); ++k) {
        auto s = toSeries(pd.time_axis, pd.data_series[k]);
        added.push_back(s);
        plotter.addSeries(
                (k < pd.series_names.size() ? pd.series_names[k] : "sig_" + std::to_string(k)),
                s
        );
    }
    if (pd.series_names.size() == 2) {
        bool v0 = pd.series_names[0].rfind("V(", 0) == 0;
        bool v1 = pd.series_names[1].rfind("V(", 0) == 0;
        if (v0 && v1) {
            auto diff = combineSameGrid(added[0], added[1], 1.0, '-');
            std::string name = pd.series_names[0] + "-" + pd.series_names[1];
            plotter.addSeries(name, diff);
        }
    }
}


void App::loadAndPlotSignal(const std::string& path, double Fs, double tStop, int chunkSize) {
    Signal s(path, Fs, tStop, chunkSize);
    auto pairs = s.readAllAsPoints();  // internally chunked, but returns full (t,y)
    std::vector<Point> pts; pts.reserve(pairs.size());
    for (auto& pr : pairs) pts.push_back({ pr.first, pr.second });
    plotter.addSeries(path, pts);
    std::cout << "[Signal] Loaded: " << pts.size() << " samples\n";
}

void App::loadAndPlotSignal(const std::string& path, double Fs, double tStop, int chunkSize, bool byChunks) {
    if (!byChunks) { loadAndPlotSignal(path, Fs, tStop, chunkSize); return; }

    // Demo streaming: load only the first chunk
    Signal s(path, Fs, tStop, chunkSize);
    if (!s.open()) { std::cerr << "[Signal] Cannot open: " << path << "\n"; return; }
    std::vector<std::pair<double,double>> pairs;
    if (s.readNextChunk()) s.appendCurrentChunkAsPoints(pairs, 0.0);
    s.close();

    std::vector<Point> pts; pts.reserve(pairs.size());
    for (auto& pr : pairs) pts.push_back({ pr.first, pr.second });
    plotter.addSeries(path + " (chunk)", pts);
    std::cout << "[Signal] Loaded first chunk: " << pts.size() << " samples (chunk=" << chunkSize << ")\n";
}

void App::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            isRunning = false;
        }

        if (e.type == SDL_EVENT_WINDOW_RESIZED) {
            int w = 0, h = 0;
            SDL_GetWindowSizeInPixels(window, &w, &h);
            plotter = Plotter(SDL_FRect{60.f, 40.f, float(w - 120), float(h - 100)});
        }

        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_ESCAPE) {
                isRunning = false;
            }
            if (e.key.key == SDLK_F2) {
                if (currentPage == Page::Grid) {
                    currentPage = Page::Plotter;
                    gridPage->commitToModel();
                    SDL_StartTextInput(window);
                } else {
                    currentPage = Page::Grid;
                    SDL_StopTextInput(window);
                }
            }
        }

        if (currentPage == Page::Grid) {
            if (gridPage) {
                gridPage->handleEvent(e);
            }
        }
        else {
            plotter.handleEvent(e);

            if (e.type == SDL_EVENT_TEXT_INPUT) {
                commandInputBuffer += e.text.text;
            }
            else if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_RETURN) {
                    if (!commandInputBuffer.empty()) {
                        std::istringstream iss(commandInputBuffer);
                        std::string cmd;
                        iss >> cmd;
                        if (cmd == "relabel") {
                            size_t idx;
                            std::string newName;
                            if (iss >> idx && iss >> newName) {
                                if (plotter.setSeriesName(idx - 1, newName)) {
                                    std::cout << "Relabeled series " << idx << " to " << newName << "\n";
                                } else {
                                    std::cout << "Error: Invalid series index '" << idx << "'\n";
                                }
                            } else {
                                std::cout << "Usage: relabel <index> <NewName>\n";
                            }
                        } else {
                            parser.parseCommand(commandInputBuffer);
                        }
                        commandInputBuffer.clear();
                    }
                } else if (e.key.key == SDLK_BACKSPACE && !commandInputBuffer.empty()) {
                    commandInputBuffer.pop_back();
                }

                switch (e.key.key) {
                    case SDLK_L:
                        loadAndPlotSignal(sigui.lastPath, sigui.Fs, sigui.tStop, sigui.chunkSz, sigui.byChunks);
                        break;
                    case SDLK_S:
                        sigui.byChunks = !sigui.byChunks;
                        std::cout << "[Signal] byChunks = " << (sigui.byChunks ? "true" : "false") << "\n";
                        break;
                    case SDLK_R: {
                        gridPage->commitToModel();
                        PlotData pd = simRunner.runTransient(1e-5, 0.05, 1e-3, {{OutputVariable::VOLTAGE, "N2"}});
                        showPlot(pd);
                        break;
                    }
                    case SDLK_M: {
                        const auto& ss = plotter.debugSeries();
                        if (ss.size() >= 2) {
                            auto C = combineSameGrid(ss[ss.size()-2].points, ss.back().points, 1.0, '+');
                            plotter.addSeries(ss[ss.size()-2].name + "_plus_" + ss.back().name, C);
                        }
                        break;
                    }
                    case SDLK_N: {
                        const auto& ss = plotter.debugSeries();
                        if (ss.size() >= 2) {
                            auto C = combineSameGrid(ss[ss.size()-2].points, ss.back().points, 1.0, '-');
                            plotter.addSeries(ss[ss.size()-2].name + "_minus_" + ss.back().name, C);
                        }
                        break;
                    }
                    case SDLK_K: {
                        const auto& ss = plotter.debugSeries();
                        if (!ss.empty()) {
                            auto S = scaleSeries(ss.back().points, 2.0);
                            plotter.addSeries(ss.back().name + "_x2", S);
                        }
                        break;
                    }
                    case SDLK_1: case SDLK_2: case SDLK_3:
                    case SDLK_4: case SDLK_5: case SDLK_6:
                    case SDLK_7: case SDLK_8: case SDLK_9: {
                        int digit = e.key.key - SDLK_0;
                        if (plotter.selectByIndex(digit - 1)) {
                            std::cout << "[Legend] selected #" << digit << " -> " << plotter.getSelectedName() << "\n";
                        }
                        break;
                    }
                    case SDLK_V:
                        plotter.toggleSelectedVisibility();
                        break;
                    case SDLK_DELETE:
                        plotter.removeSelected();
                        break;
                    case SDLK_G:
                        plotter.setLegendVisible(!plotter.isLegendVisible());
                        break;
                    case SDLK_B: {
                        auto selectedIdx = plotter.getSelectedIndex();
                        if (selectedIdx.has_value()) {
                            plotter.cycleSeriesColor(*selectedIdx);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }
}

void App::update() {
    // future: animations/continuous sim
}

void App::render() {
    SDL_SetRenderDrawColor(renderer, 245,245,245,255);
    SDL_RenderClear(renderer);
    renderSchematic();

    // --- اضافه شد: رندر شرطی ---
    if (currentPage == Page::Grid && gridPage) {
        gridPage->render(renderer);
    } else {
        plotter.render(renderer);
    }

    // Optional: cursor readouts in console
    if (auto x1 = plotter.getCursor1X()) {
        if (auto x2 = plotter.getCursor2X()) {
            double dt = *x2 - *x1;
            std::cout << "[Cursors] Δt = " << dt << " s";

            const auto& ss = plotter.debugSeries();
            auto selectedIdx = plotter.getSelectedIndex();
            const auto& seriesToMeasure = (selectedIdx && *selectedIdx < ss.size()) ? ss[*selectedIdx] : ss.back();

            if (!ss.empty() && !seriesToMeasure.points.empty()) {
                const auto& pts = seriesToMeasure.points;
                auto at = [&](double x){
                    auto it = std::lower_bound(pts.begin(), pts.end(), x,
                                               [](const Point& p, double xv){ return p.x < xv; });
                    if (it == pts.end() || it == pts.begin()) return it == pts.begin() ? pts.front().y : pts.back().y;
                    return it->y;
                };
                double dy = at(*x2) - at(*x1);
                std::cout << ", Δy(" << seriesToMeasure.name << ") = " << dy;

                if (std::abs(dt) > 1e-12) {
                    double slope = dy / dt;
                    std::cout << ", Slope = " << slope;
                }
            }
            std::cout << "\n";
        }
    }

    if (currentPage == Page::Grid && gridPage) {
        gridPage->render(renderer);
    } else {
        plotter.render(renderer);
        // نمایش بافر دستور در پایین صفحه Plotter
        if (mainFont && !commandInputBuffer.empty()) {
            SDL_Surface* surface = TTF_RenderText_Blended(mainFont, commandInputBuffer.c_str(), commandInputBuffer.size(), {0, 0, 0, 255});
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FRect dstRect = { 10, 570, (float)surface->w, (float)surface->h };
                SDL_RenderTexture(renderer, texture, nullptr, &dstRect);
                SDL_DestroyTexture(texture);
                SDL_DestroySurface(surface);
            }
        }
    }

    SDL_RenderPresent(renderer);
}
void App::renderSchematic() {
    // For now, let's assume the schematic area is the whole window.
    // We can define a specific SDL_FRect for it later.

    for (const auto& elem : graph.getElements()) {
        if (!elem) continue;

        if (elem->type == SUBCIRCUIT) { // This is the requirement of section 4.4
            // Draw a rectangle for the Subcircuit
            SDL_SetRenderDrawColor(renderer, 0, 0, 200, 255); // Blue color
            SDL_FRect sub_rect = {elem->x, elem->y, 100.0f, 80.0f}; // A fixed size of 100x80 pixels
            SDL_RenderFillRect(renderer, &sub_rect);

            // You would also need to render the name and ports, which requires SDL_ttf library for fonts.
            // For now, the blue rectangle is enough to fulfill the basic requirement.

        } else {
            // Draw a simple placeholder for other elements (e.g., a small red box)
            SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); // Red color
            SDL_FRect elem_rect = {elem->x, elem->y, 40.0f, 20.0f};
            SDL_RenderFillRect(renderer, &elem_rect);
        }
    }
}

int App::run() {
    if (!init()) return -1;

    parser.onPlotData = [this](const PlotData& pd){ this->showPlot(pd); };
    parser.onScopeLoad = [this](const std::string& p, double Fs, double t, int c){
        this->loadAndPlotSignal(p, Fs, t, c);
    };
    parser.onScopeClear = [this](){ this->plotter.clear(); };

    PlotData pd = simRunner.runTransient(1e-5, 0.05, 1e-3, {{OutputVariable::VOLTAGE, "N2"}});
    showPlot(pd);

    while (isRunning) {
        handleEvents();
        update();
        render();
        SDL_Delay(10);
    }

    cleanup();
    return 0;
}

//std::string App::promptText(const std::string& title, const std::string& initial) {
//    std::string buffer = initial;
//    bool done = false;
//    SDL_StartTextInput(window);
//    while (!done) {
//        SDL_Event e;
//        while (SDL_PollEvent(&e)) {
//            if (e.type == SDL_EVENT_QUIT) { SDL_StopTextInput(window); return ""; }
//            if (e.type == SDL_EVENT_KEY_DOWN) {
//                if (e.key.key == SDLK_RETURN) { done = true; break; }
//                if (e.key.key == SDLK_ESCAPE) { SDL_StopTextInput(window); return ""; }
//                if (e.key.key == SDLK_BACKSPACE) { if (!buffer.empty()) buffer.pop_back(); }
//            }
//            if (e.type == SDL_EVENT_TEXT_INPUT) {
//                buffer += e.text.text;
//            }
//        }
//        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 192);
//        SDL_RenderClear(renderer);
//        SDL_FRect r{80.0f, 80.0f, 600.0f, 180.0f};
//        SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
//        SDL_RenderFillRect(renderer, &r);
//        if (mainFont) {
//            SDL_Surface* s1 = TTF_RenderText_Solid(mainFont, title.c_str(), (size_t)title.size(), SDL_Color{255,255,255,255});
//            SDL_Texture* t1 = s1 ? SDL_CreateTextureFromSurface(renderer, s1) : nullptr;
//            SDL_FRect d1{100.0f, 100.0f, s1 ? (float)s1->w : 0.0f, s1 ? (float)s1->h : 0.0f};
//            if (t1) SDL_RenderTexture(renderer, t1, nullptr, &d1);
//            SDL_Surface* s2 = TTF_RenderText_Solid(mainFont, buffer.c_str(), (size_t)buffer.size(), SDL_Color{200,255,200,255});
//            SDL_Texture* t2 = s2 ? SDL_CreateTextureFromSurface(renderer, s2) : nullptr;
//            SDL_FRect d2{100.0f, 160.0f, s2 ? (float)s2->w : 0.0f, s2 ? (float)s2->h : 0.0f};
//            if (t2) SDL_RenderTexture(renderer, t2, nullptr, &d2);
//            if (t1) SDL_DestroyTexture(t1);
//            if (t2) SDL_DestroyTexture(t2);
//            if (s1) SDL_DestroySurface(s1);
//            if (s2) SDL_DestroySurface(s2);
//        }
//        SDL_RenderPresent(renderer);
//    }
//    SDL_StopTextInput(window);
//    return buffer;
//}
//
//int App::hitElementIndexAt(const SDL_FPoint& p) const {
//    for (int i = (int)uiElements.size()-1; i >= 0; --i) {
//        const auto& b = uiElements[i].bbox;
//        if (p.x >= b.x && p.x <= b.x + b.w && p.y >= b.y && p.y <= b.y + b.h) return i;
//    }
//    return -1;
//}
//
//void App::requestElementEdit(int idx) {
//    if (idx < 0 || idx >= (int)uiElements.size()) return;
//    auto& e = uiElements[idx];
//    std::string v = promptText("Value for " + e.name, e.value.empty() ? "1k" : e.value);
//    if (v.empty()) return;
//    e.value = v;
//    char kindChar = 'R';
//    if (e.kind == ToolKind::Capacitor) kindChar = 'C';
//    else if (e.kind == ToolKind::Inductor) kindChar = 'L';
//    else if (e.kind == ToolKind::VSource) kindChar = 'V';
//    else if (e.kind == ToolKind::ISource) kindChar = 'I';
//    std::ostringstream ss;
//    ss << "value " << e.name << " " << e.value;
//    parser.parseCommand(ss.str());
//}
//
//void App::applyPlacement(ToolKind kind, int n1, int n2, const SDL_FPoint& a, const SDL_FPoint& b) {
//    std::string base = "X";
//    char kindChar = 'R';
//    if (kind == ToolKind::Resistor) { base = "R"; kindChar = 'R'; }
//    else if (kind == ToolKind::Capacitor) { base = "C"; kindChar = 'C'; }
//    else if (kind == ToolKind::Inductor) { base = "L"; kindChar = 'L'; }
//    else if (kind == ToolKind::VSource) { base = "V"; kindChar = 'V'; }
//    else if (kind == ToolKind::ISource) { base = "I"; kindChar = 'I'; }
//    int idx = 1;
//    for (auto& u : uiElements) if (u.kind == kind) idx++;
//    std::string name = base + std::to_string(idx);
//    std::string v = promptText("Value for " + name, kind == ToolKind::Resistor ? "1k" : (kind == ToolKind::Capacitor ? "1u" : (kind == ToolKind::Inductor ? "1m" : "5")));
//    if (v.empty()) return;
//    SDL_FRect bbox;
//    bbox.x = std::min(a.x, b.x);
//    bbox.y = std::min(a.y, b.y);
//    bbox.w = std::abs(a.x - b.x);
//    bbox.h = std::abs(a.y - b.y);
//    if (bbox.w < 20) bbox.w = 20;
//    if (bbox.h < 20) bbox.h = 20;
//    UiElement ue;
//    ue.name = name;
//    ue.kind = kind;
//    ue.bbox = bbox;
//    ue.n1 = n1;
//    ue.n2 = n2;
//    ue.value = v;
//    uiElements.push_back(ue);
//    std::ostringstream ss;
//    ss << "add " << kindChar << " " << name << " " << n1 << " " << n2 << " " << v;
//    parser.parseCommand(ss.str());
//}
//
//void App::handleMouseDown(const SDL_MouseButtonEvent& e) {
//    SDL_FPoint p{(float)e.x, (float)e.y};
//    uint64_t now = SDL_GetTicks();
//    bool dbl = (now - lastClickTicks < 350) && (std::abs(p.x - lastClickPos.x) < 8 && std::abs(p.y - lastClickPos.y) < 8);
//    lastClickTicks = now;
//    lastClickPos = p;
//
//    if (dbl) {
//        int idx = hitElementIndexAt(p);
//        if (idx >= 0) { requestElementEdit(idx); return; }
//    }
//
//    if (currentTool == ToolKind::Resistor || currentTool == ToolKind::Capacitor || currentTool == ToolKind::Inductor || currentTool == ToolKind::VSource || currentTool == ToolKind::ISource) {
//        if (!placing) {
//            placePos1 = p;
//            placeNode1 = findOrCreateNodeAt(p);
//            placing = true;
//            return;
//        } else {
//            int n2 = findOrCreateNodeAt(p);
//            applyPlacement(currentTool, placeNode1, n2, placePos1, p);
//            placing = false;
//            placeNode1 = -1;
//            return;
//        }
//    }
//}
//
//static inline long long makeKey(int gx, int gy) {
//    return ( (long long)gx << 32 ) ^ (unsigned int)gy;
//}
//
//SDL_FPoint App::snapToGrid(const SDL_FPoint& p) const {
//    SDL_FPoint out;
//    out.x = std::round(p.x / gridSize) * gridSize;
//    out.y = std::round(p.y / gridSize) * gridSize;
//    return out;
//}
//
//int App::findOrCreateNodeAt(const SDL_FPoint& p) {
//    SDL_FPoint s = snapToGrid(p);
//    int gx = (int)std::lround(s.x / gridSize);
//    int gy = (int)std::lround(s.y / gridSize);
//    long long k = makeKey(gx, gy);
//    auto it = gridNode.find(k);
//    if (it != gridNode.end()) return it->second;
//    int id = nextNodeId++;
//    gridNode[k] = id;
//    return id;
//}

