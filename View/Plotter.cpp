//
// Created by Mehrshad on 8/9/2025.
//

#include "Plotter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

static const SDL_Color palette[] = {
        {  46, 204, 113, 255}, {  52, 152, 219, 255}, {231,  76,  60, 255},
        { 155,  89, 182, 255}, { 241, 196,  15, 255}, { 26, 188, 156, 255},
};
static const int paletteSize = sizeof(palette) / sizeof(palette[0]);

Plotter::Plotter(SDL_FRect plotArea, TTF_Font* f) : area(plotArea), font(f) {
    applyAutoZoom();
}

void Plotter::setArea(SDL_FRect plotArea) {
    area = plotArea;
    if (autoZoom) applyAutoZoom();
}

void Plotter::renderText(SDL_Renderer* ren, const std::string& text, float x, float y) const {
    if (!font || text.empty()) return;

    SDL_Color textColor = {0, 0, 0, 255};
    SDL_Surface* textSurface = TTF_RenderText_Blended(font, text.c_str(), 0, textColor);
    if (!textSurface) return;

    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(ren, textSurface);
    if (textTexture) {
        SDL_FRect quad = { x, y, (float)textSurface->w, (float)textSurface->h };
        SDL_RenderTexture(ren, textTexture, nullptr, &quad);
        SDL_DestroyTexture(textTexture);
    }
    SDL_DestroySurface(textSurface);
}

double Plotter::toAxisX(double x) const {
    if (!logX) return x;
    return std::log10(std::max(x, 1e-300));
}

double Plotter::fromAxisX(double ax) const {
    return logX ? std::pow(10.0, ax) : ax;
}

SDL_FPoint Plotter::worldToScreen(double x, double y) const {
    float sx = static_cast<float>(area.x + (toAxisX(x) + offsetX) * scaleX);
    float sy = static_cast<float>(area.y + area.h - (y + offsetY) * scaleY);
    return SDL_FPoint{sx, sy};
}

Point Plotter::screenToWorld(float sx, float sy) const {
    double ax = (sx - area.x) / scaleX - offsetX;
    double wy = (area.y + area.h - sy) / scaleY - offsetY;
    return {fromAxisX(ax), wy};
}

void Plotter::drawAxisLabels(SDL_Renderer* r) const {
    if (!font) return;

    char buffer[32];
    for (int j = 0; j <= nyDiv; ++j) {
        float y_screen = area.y + j * (area.h / nyDiv);
        double v = screenToWorld(area.x, y_screen).y;
        if (std::abs(v) < 1e-4 * (area.h / nyDiv) / scaleY) v = 0;   // rounding, show 0 not "-2e-07"
        snprintf(buffer, sizeof(buffer), "%.4g", v);
        int text_w = 0, text_h = 0;
        TTF_GetStringSize(font, buffer, 0, &text_w, &text_h);
        renderText(r, buffer, area.x - text_w - 5, y_screen - text_h / 2.0f);
    }

    int every = (nxDiv > 6) ? 2 : 1;
    for (int i = 0; i <= nxDiv; i += every) {
        float x_screen = area.x + i * (area.w / nxDiv);
        double v = screenToWorld(x_screen, area.y).x;
        if (!logX && std::abs(v) < 1e-4 * (area.w / nxDiv) / scaleX) v = 0;
        snprintf(buffer, sizeof(buffer), "%.4g", v);
        int text_w = 0, text_h = 0;
        TTF_GetStringSize(font, buffer, 0, &text_w, &text_h);
        renderText(r, buffer, x_screen - text_w / 2.0f, area.y + area.h + 4);
    }

    std::string xl = xAxisLabel + (logX ? "  [log]" : "");
    int w = 0, h = 0;
    TTF_GetStringSize(font, xl.c_str(), 0, &w, &h);
    renderText(r, xl, area.x + area.w - w, area.y + area.h + 4 + h);
}

void Plotter::render(SDL_Renderer* r) {
    drawGrid(r);
    drawAxisLabels(r);

    // don't draw lines outside the plot box
    SDL_Rect clip{ (int)area.x, (int)area.y, (int)area.w + 1, (int)area.h + 1 };
    SDL_SetRenderClipRect(r, &clip);
    for (size_t i = 0; i < series.size(); ++i) {
        const auto& s = series[i];
        if (!s.visible || s.points.size() < 2) continue;

        Uint8 alpha = (!selectedIndex || *selectedIndex == i) ? 255 : 120;
        SDL_SetRenderDrawColor(r, s.color.r, s.color.g, s.color.b, alpha);

        std::vector<SDL_FPoint> pts;
        pts.reserve(s.points.size());
        for (const auto& p : s.points) pts.push_back(worldToScreen(p.x, p.y));
        SDL_RenderLines(r, pts.data(), (int)pts.size());
    }
    drawCursors(r);
    SDL_SetRenderClipRect(r, nullptr);

    drawLegend(r);
}

void Plotter::clear() {
    series.clear();
    selectedIndex.reset();
    cursor1X.reset();
    cursor2X.reset();
    recomputeBounds();
    applyAutoZoom();
}

void Plotter::setXAxis(const std::string& label, bool logScale) {
    xAxisLabel = label;
    logX = logScale;
    recomputeBounds();
    if (autoZoom) applyAutoZoom();
}

void Plotter::addSeries(const std::string& name,
                        const std::vector<Point>& pts,
                        std::optional<SDL_Color> color) {
    Series s;
    s.name = name;
    s.points = pts;
    s.color = color.value_or(palette[series.size() % paletteSize]);
    series.push_back(std::move(s));
    recomputeBounds();
    if (autoZoom) applyAutoZoom();
}

void Plotter::setAutoZoom(bool on) {
    autoZoom = on;
    if (on) applyAutoZoom();
}

// 1, 2, 2.5, 5 or 10 times a power of ten, so the grid lines land on round numbers
static double niceStep(double range, int div) {
    double raw = range / div;
    double mag = std::pow(10.0, std::floor(std::log10(raw)));
    double f = raw / mag;
    double nf = (f <= 1.0) ? 1.0 : (f <= 2.0) ? 2.0 : (f <= 2.5) ? 2.5 : (f <= 5.0) ? 5.0 : 10.0;
    return nf * mag;
}

// rounds [mn, mx] out to whole steps, returns how many grid divisions that gives
static int niceRange(double mn, double mx, int target, double& lo, double& hi) {
    if (mx - mn < 1e-12 * std::max(1.0, std::abs(mx))) {
        double pad = (mn == 0) ? 1.0 : std::abs(mn) * 0.5;
        mn -= pad; mx += pad;
    }
    double step = niceStep(mx - mn, target);
    lo = std::floor(mn / step + 1e-9) * step;
    hi = std::ceil(mx / step - 1e-9) * step;
    if (hi <= lo) hi = lo + step;
    return std::max(1, (int)std::lround((hi - lo) / step));
}

void Plotter::applyAutoZoom() {
    recomputeBounds();

    // a bit of room above and below the curves (but keep 0 on the edge if it's there)
    double ymin = minY, ymax = maxY, span = maxY - minY;
    if (ymin != 0) ymin -= 0.04 * span;
    if (ymax != 0) ymax += 0.04 * span;

    double xlo, xhi, ylo, yhi;
    nyDiv = niceRange(ymin, ymax, 8, ylo, yhi);
    if (logX) {
        // whole decades
        xlo = std::floor(minX);
        xhi = std::ceil(maxX);
        if (xhi <= xlo) xhi = xlo + 1;
        nxDiv = std::clamp(int(xhi - xlo), 1, 12);
        xhi = xlo + nxDiv;
    } else {
        nxDiv = niceRange(minX, maxX, 10, xlo, xhi);
    }

    scaleX = area.w / (xhi - xlo);
    scaleY = area.h / (yhi - ylo);
    offsetX = -xlo;
    offsetY = -ylo;
}

void Plotter::zoomX(double f) {
    // zoom around the middle of the box, not the left edge
    double mid = (area.w / 2) / scaleX - offsetX;
    scaleX *= f;
    offsetX = (area.w / 2) / scaleX - mid;
}

void Plotter::zoomY(double f) {
    double mid = (area.h / 2) / scaleY - offsetY;
    scaleY *= f;
    offsetY = (area.h / 2) / scaleY - mid;
}

void Plotter::panX(double dx) { offsetX += dx / scaleX; }
void Plotter::panY(double dy) { offsetY += dy / scaleY; }

void Plotter::enableCursor(bool on) {
    cursorEnabled = on;
    if (!on) { cursor1X.reset(); cursor2X.reset(); }
    std::cout << "[Cursor] " << (on ? "on (click on the plot)" : "off") << "\n";
}

void Plotter::enableDoubleCursor(bool on) {
    doubleCursor = on;
    if (!on) cursor2X.reset();
    std::cout << "[Cursor] double cursor " << (on ? "on" : "off") << "\n";
}

std::optional<double> Plotter::valueAt(size_t idx, double x) const {
    if (idx >= series.size() || series[idx].points.empty()) return std::nullopt;
    const auto& pts = series[idx].points;
    auto it = std::lower_bound(pts.begin(), pts.end(), x,
                               [](const Point& p, double xv) { return p.x < xv; });
    if (it == pts.end()) return pts.back().y;
    if (it == pts.begin()) return pts.front().y;
    auto prev = it - 1;
    return (x - prev->x < it->x - x) ? prev->y : it->y;
}

void Plotter::printCursorInfo() const {
    if (series.empty() || !cursor1X) return;
    size_t idx = selectedIndex.value_or(series.size() - 1);
    const std::string& name = series[idx].name;

    auto y1 = valueAt(idx, *cursor1X);
    std::cout << "[Cursor] x1 = " << *cursor1X << ", " << name << " = " << y1.value_or(0.0);
    if (doubleCursor && cursor2X) {
        auto y2 = valueAt(idx, *cursor2X);
        double dx = *cursor2X - *cursor1X;
        double dy = y2.value_or(0.0) - y1.value_or(0.0);
        std::cout << " | x2 = " << *cursor2X << ", dx = " << dx << ", dy = " << dy;
        if (std::abs(dx) > 1e-15) std::cout << ", slope = " << dy / dx;
    }
    std::cout << "\n";
}

void Plotter::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        if (e.wheel.y > 0) { zoomX(1.1); zoomY(1.1); }
        else if (e.wheel.y < 0) { zoomX(1.0/1.1); zoomY(1.0/1.1); }
        autoZoom = false;
    } else if (e.type == SDL_EVENT_MOUSE_MOTION &&
               ((e.motion.state & SDL_BUTTON_RMASK) || (!cursorEnabled && (e.motion.state & SDL_BUTTON_LMASK)))) {
        // drag to pan (left button is taken by the cursor when it's on, right always works)
        panX(e.motion.xrel);
        panY(-e.motion.yrel);
        autoZoom = false;
    } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        bool inside = e.button.x >= area.x && e.button.x <= area.x + area.w &&
                      e.button.y >= area.y && e.button.y <= area.y + area.h;
        if (cursorEnabled && inside) {
            double xw = screenToWorld(e.button.x, e.button.y).x;
            if (!cursor1X || !doubleCursor || cursor2X) {
                cursor1X = xw;
                cursor2X.reset();
            } else {
                cursor2X = xw;
            }
            printCursorInfo();
        }
    } else if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
            case SDLK_A: setAutoZoom(true); break;
            case SDLK_C: enableCursor(!cursorEnabled); break;
            case SDLK_D: enableDoubleCursor(!doubleCursor); break;
            case SDLK_LEFTBRACKET:  zoomX(1.0/1.1); autoZoom = false; break;
            case SDLK_RIGHTBRACKET: zoomX(1.1); autoZoom = false; break;
            case SDLK_MINUS:        zoomY(1.0/1.1); autoZoom = false; break;
            case SDLK_EQUALS:       zoomY(1.1); autoZoom = false; break;
            default: break;
        }
    }
}

void Plotter::drawGrid(SDL_Renderer* r) const {
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderFillRect(r, &area);

    SDL_SetRenderDrawColor(r, 230,230,230,255);
    for (int i = 0; i <= nxDiv; i++) { float x = area.x + i*(area.w/nxDiv); SDL_RenderLine(r, x, area.y, x, area.y + area.h); }
    for (int j = 0; j <= nyDiv; j++) { float y = area.y + j*(area.h/nyDiv); SDL_RenderLine(r, area.x, y, area.x + area.w, y); }

    // y = 0 line so negative values are easy to see
    float zeroY = worldToScreen(0, 0).y;
    if (zeroY > area.y && zeroY < area.y + area.h) {
        SDL_SetRenderDrawColor(r, 170, 170, 170, 255);
        SDL_RenderLine(r, area.x, zeroY, area.x + area.w, zeroY);
    }

    SDL_SetRenderDrawColor(r, 0,0,0,255);
    SDL_RenderRect(r, &area);
}

void Plotter::drawLegend(SDL_Renderer* r) const {
    if (!legendVisible || series.empty()) return;

    const float rowH = 20.f, swatch = 14.f;
    float boxW = 60.f;
    for (const auto& s : series) {
        int w = 0, h = 0;
        if (font) TTF_GetStringSize(font, s.name.c_str(), 0, &w, &h);
        boxW = std::max(boxW, w + swatch + 40.f);
    }

    const float x = area.x + area.w - boxW - 10.f;
    float y = area.y + 10.f;
    SDL_FRect box{ x, y, boxW, rowH * series.size() + 8.f };
    SDL_SetRenderDrawColor(r, 255,255,255,230);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 0,0,0,255);
    SDL_RenderRect(r, &box);

    y += 4.f;
    for (size_t i = 0; i < series.size(); ++i) {
        const auto& s = series[i];
        float lx = x + 8.f;

        SDL_SetRenderDrawColor(r, s.color.r, s.color.g, s.color.b, 255);
        SDL_FRect sw{ lx, y + rowH / 2 - 2, swatch, 4.f };
        SDL_RenderFillRect(r, &sw);

        std::string label = std::to_string(i + 1) + ". " + s.name + (s.visible ? "" : " (hidden)");
        renderText(r, label, lx + swatch + 6, y + 1);

        if (selectedIndex && *selectedIndex == i) {
            SDL_SetRenderDrawColor(r, 0,0,0,255);
            SDL_FRect hi{ x + 3, y, boxW - 6, rowH };
            SDL_RenderRect(r, &hi);
        }
        y += rowH;
    }
}

void Plotter::drawCursors(SDL_Renderer* r) const {
    if (!cursorEnabled) return;
    SDL_SetRenderDrawColor(r, 20,20,20,180);
    auto drawX = [&](double xw) {
        float sx = worldToScreen(xw, 0).x;
        SDL_RenderLine(r, sx, area.y, sx, area.y + area.h);
    };
    if (cursor1X) drawX(*cursor1X);
    if (doubleCursor && cursor2X) drawX(*cursor2X);
}

void Plotter::recomputeBounds() {
    minX = minY = 1e300;
    maxX = maxY = -1e300;
    bool any = false;
    for (const auto& s : series) {
        if (!s.visible) continue;
        for (const auto& p : s.points) {
            if (!std::isfinite(p.y)) continue;
            double ax = toAxisX(p.x);
            minX = std::min(minX, ax); maxX = std::max(maxX, ax);
            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
            any = true;
        }
    }
    if (!any) { minX = minY = 0; maxX = maxY = 1; }
}

void Plotter::cycleSeriesColor(size_t i) {
    if (i >= series.size()) return;
    // move to the next palette color after the current one
    int cur = 0;
    for (int k = 0; k < paletteSize; ++k) {
        const auto& c = palette[k];
        const auto& sc = series[i].color;
        if (c.r == sc.r && c.g == sc.g && c.b == sc.b) { cur = k; break; }
    }
    series[i].color = palette[(cur + 1) % paletteSize];
}

bool Plotter::selectByIndex(size_t idx) {
    if (idx >= series.size()) return false;
    selectedIndex = idx;
    return true;
}

std::string Plotter::getSelectedName() const {
    if (!selectedIndex || *selectedIndex >= series.size()) return {};
    return series[*selectedIndex].name;
}

bool Plotter::toggleSelectedVisibility() {
    if (!selectedIndex || *selectedIndex >= series.size()) return false;
    series[*selectedIndex].visible = !series[*selectedIndex].visible;
    recomputeBounds();
    if (autoZoom) applyAutoZoom();
    return true;
}

bool Plotter::removeSelected() {
    if (!selectedIndex || *selectedIndex >= series.size()) return false;
    series.erase(series.begin() + *selectedIndex);
    selectedIndex.reset();
    recomputeBounds();
    if (autoZoom) applyAutoZoom();
    return true;
}

bool Plotter::setSeriesName(size_t index, const std::string& newName) {
    if (index >= series.size()) return false;
    series[index].name = newName;
    return true;
}
