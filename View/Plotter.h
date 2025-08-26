//
// Created by Mehrshad on 8/9/2025.
//

#ifndef MORGHSPICY_PLOTTER_H
#define MORGHSPICY_PLOTTER_H

#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <optional>

struct Point { double x{}, y{}; };

struct Series {
    std::string name;
    SDL_Color   color{0,0,0,255};
    std::vector<Point> points;
    bool visible{true};
};

enum class PlotMode { Time, Frequency, Phase };

class Plotter {
public:
    explicit Plotter(SDL_FRect plotArea);

    // data
    void clear();
    void addSeries(const std::string& name,
                   const std::vector<Point>& pts,
                   std::optional<SDL_Color> color = std::nullopt);

    // view
    void setAutoZoom(bool on);
    void applyAutoZoom();
    void setMode(PlotMode m) { mode = m; }

    // interaction
    void handleEvent(const SDL_Event& e);
    void render(SDL_Renderer* r);

    // scaling / panning
    void zoomX(double factor);
    void zoomY(double factor);
    void panX(double dx);
    void panY(double dy);

    // cursors
    void enableCursor(bool on);
    void enableDoubleCursor(bool on);
    std::optional<double> getCursor1X() const { return cursor1X; }
    std::optional<double> getCursor2X() const { return cursor2X; }

    // legend & selection
    void setLegendVisible(bool on) { legendVisible = on; }
    bool isLegendVisible() const { return legendVisible; }

    size_t seriesCount() const { return series.size(); }
    bool selectByIndex(size_t idx);
    std::optional<size_t> getSelectedIndex() const { return selectedIndex; }
    std::string getSelectedName() const;
    bool toggleSelectedVisibility();
    bool removeSelected();

    // series management
    std::vector<std::string> getSeriesNames() const;
    bool setSeriesVisible(const std::string& name, bool on);
    bool removeSeries(const std::string& name);
    bool setSeriesName(size_t index, const std::string& newName);
    const std::vector<Point>* getSeries(const std::string& name) const;
    const std::vector<Series>& debugSeries() const { return series; } // read-only

    // ----- quick series utilities (used by App hotkeys) -----
    void toggleSeriesVisible(int i);
    void cycleSeriesColor(int i);
private:
    // geometry / state
    SDL_FRect area;
    std::vector<Series> series;

    double minX{}, maxX{}, minY{}, maxY{};
    double scaleX{1.0}, scaleY{1.0};
    double offsetX{0.0}, offsetY{0.0};
    bool   autoZoom{true};
    PlotMode mode{PlotMode::Time};

    // legend & selection
    bool legendVisible{true};
    std::optional<size_t> selectedIndex;

    // cursors
    bool cursorEnabled{false};
    bool doubleCursor{false};
    std::optional<double> cursor1X;
    std::optional<double> cursor2X;

    // helpers
    void drawGrid(SDL_Renderer* r) const;
    void drawLegend(SDL_Renderer* r) const;
    void drawCursors(SDL_Renderer* r) const;
    SDL_FPoint worldToScreen(double x, double y) const;
    void recomputeBounds();
};


#endif //MORGHSPICY_PLOTTER_H