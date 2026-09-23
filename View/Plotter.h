#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
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
    Plotter() = default;
    explicit Plotter(SDL_FRect plotArea, TTF_Font* font);

    void clear();
    void addSeries(const std::string& name,
                   const std::vector<Point>& pts,
                   std::optional<SDL_Color> color = std::nullopt);

    void setAutoZoom(bool on);
    void applyAutoZoom();
    void setMode(PlotMode m) { mode = m; }

    void handleEvent(const SDL_Event& e);
    void render(SDL_Renderer* r);

    void zoomX(double factor);
    void zoomY(double factor);
    void panX(double dx);
    void panY(double dy);

    void enableCursor(bool on);
    void enableDoubleCursor(bool on);
    std::optional<double> getCursor1X() const { return cursor1X; }
    std::optional<double> getCursor2X() const { return cursor2X; }

    void setLegendVisible(bool on) { legendVisible = on; }
    bool isLegendVisible() const { return legendVisible; }

    size_t seriesCount() const { return series.size(); }
    bool selectByIndex(size_t idx);
    std::optional<size_t> getSelectedIndex() const { return selectedIndex; }
    std::string getSelectedName() const;
    bool toggleSelectedVisibility();
    bool removeSelected();

    std::vector<std::string> getSeriesNames() const;
    bool setSeriesVisible(const std::string& name, bool on);
    bool removeSeries(const std::string& name);
    bool setSeriesName(size_t index, const std::string& newName);
    const std::vector<Point>* getSeries(const std::string& name) const;
    const std::vector<Series>& debugSeries() const { return series; }

    void toggleSeriesVisible(int i);
    void cycleSeriesColor(int i);
private:
    SDL_FRect area;
    std::vector<Series> series;
    TTF_Font* font = nullptr;

    double minX{}, maxX{}, minY{}, maxY{};
    double scaleX{1.0}, scaleY{1.0};
    double offsetX{0.0}, offsetY{0.0};
    bool   autoZoom{true};
    PlotMode mode{PlotMode::Time};

    bool legendVisible{true};
    std::optional<size_t> selectedIndex;

    bool cursorEnabled{false};
    bool doubleCursor{false};
    std::optional<double> cursor1X;
    std::optional<double> cursor2X;

    void drawGrid(SDL_Renderer* r) const;
    void drawLegend(SDL_Renderer* r) const;
    void drawCursors(SDL_Renderer* r) const;
    void drawAxisLabels(SDL_Renderer* r) const;
    void renderText(SDL_Renderer* ren, const std::string& text, int x, int y) const;
    SDL_FPoint worldToScreen(double x, double y) const;
    Point screenToWorld(float sx, float sy) const;
    void recomputeBounds();
};