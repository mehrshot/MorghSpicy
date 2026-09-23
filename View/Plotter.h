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

class Plotter {
public:
    Plotter() = default;
    explicit Plotter(SDL_FRect plotArea, TTF_Font* font);

    void setArea(SDL_FRect plotArea);
    void setFont(TTF_Font* f) { font = f; }

    void clear();
    void addSeries(const std::string& name,
                   const std::vector<Point>& pts,
                   std::optional<SDL_Color> color = std::nullopt);

    void setAutoZoom(bool on);
    void applyAutoZoom();

    // x axis settings (log scale is used for dec/oct AC sweeps)
    void setXAxis(const std::string& label, bool logScale);
    const std::string& xLabel() const { return xAxisLabel; }
    bool isLogX() const { return logX; }

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
    // value of a series at x (nearest sample)
    std::optional<double> valueAt(size_t idx, double x) const;

    void setLegendVisible(bool on) { legendVisible = on; }
    bool isLegendVisible() const { return legendVisible; }

    size_t seriesCount() const { return series.size(); }
    bool selectByIndex(size_t idx);
    std::optional<size_t> getSelectedIndex() const { return selectedIndex; }
    std::string getSelectedName() const;
    bool toggleSelectedVisibility();
    bool removeSelected();

    bool setSeriesName(size_t index, const std::string& newName);
    const std::vector<Series>& allSeries() const { return series; }

    void cycleSeriesColor(size_t i);

private:
    SDL_FRect area{};
    std::vector<Series> series;
    TTF_Font* font = nullptr;

    double minX{}, maxX{}, minY{}, maxY{};
    int nxDiv = 10, nyDiv = 8;     // grid divisions
    double scaleX{1.0}, scaleY{1.0};
    double offsetX{0.0}, offsetY{0.0};
    bool   autoZoom{true};

    std::string xAxisLabel = "t (s)";
    bool logX = false;

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
    void renderText(SDL_Renderer* ren, const std::string& text, float x, float y) const;
    void printCursorInfo() const;

    // x goes through log10 when the axis is logarithmic
    double toAxisX(double x) const;
    double fromAxisX(double ax) const;

    SDL_FPoint worldToScreen(double x, double y) const;
    Point screenToWorld(float sx, float sy) const;
    void recomputeBounds();
};
