//
// Created by Mehrshad on 8/9/2025.
//

#include "Plotter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <SDL3_ttf/SDL_ttf.h>

static SDL_Color palette[] = {
        {  46, 204, 113, 255}, {  52, 152, 219, 255}, {231,  76,  60, 255},
        { 155,  89, 182, 255}, { 241, 196,  15, 255}, { 26, 188, 156, 255},
};

Plotter::Plotter(SDL_FRect plotArea, TTF_Font* f) : area(plotArea), font(f) {
    applyAutoZoom();
}

void Plotter::renderText(SDL_Renderer* ren, const std::string& text, int x, int y) const {
    if (!font || text.empty()) {
        return;
    }
    SDL_Color textColor = {0, 0, 0, 255};
    SDL_Surface* textSurface = TTF_RenderText_Blended(font, text.c_str(), 0, textColor);
    if (textSurface == nullptr) {
        return;
    }

    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(ren, textSurface);
    if (textTexture == nullptr) {
        SDL_DestroySurface(textSurface);
        return;
    }

    SDL_FRect renderQuad = { (float)x, (float)y, (float)textSurface->w, (float)textSurface->h };
    SDL_RenderTexture(ren, textTexture, nullptr, &renderQuad);

    SDL_DestroyTexture(textTexture);
    SDL_DestroySurface(textSurface);
}

Point Plotter::screenToWorld(float sx, float sy) const {
    double wx = (sx - area.x) / scaleX - offsetX;
    double wy = (area.y + area.h - sy) / scaleY - offsetY;
    return {wx, wy};
}


void Plotter::drawAxisLabels(SDL_Renderer* r) const {
    if (!font) return;

    const int nx = 10, ny = 8;
    char buffer[32];

    for (int j = 0; j <= ny; ++j) {
        float y_screen = area.y + j * (area.h / ny);
        Point p_world = screenToWorld(area.x, y_screen);
        snprintf(buffer, sizeof(buffer), "%.2g", p_world.y);
        int text_w, text_h;
        TTF_GetStringSize(font, buffer, 0, &text_w, &text_h);
        renderText(r, buffer, area.x - text_w - 5, y_screen - text_h / 2);
    }

    for (int i = 0; i <= nx; ++i) {
        float x_screen = area.x + i * (area.w / nx);
        Point p_world = screenToWorld(x_screen, area.y);
        snprintf(buffer, sizeof(buffer), "%.2g", p_world.x);
        int text_w, text_h;
        TTF_GetStringSize(font, buffer, 0, &text_w, &text_h);
        renderText(r, buffer, x_screen - text_w / 2, area.y + area.h + 5);
    }
}

void Plotter::render(SDL_Renderer* r) {
    drawGrid(r);
    drawAxisLabels(r);
    for (size_t i = 0; i < series.size(); ++i) {
        const auto& s = series[i];
        if (!s.visible || s.points.size() < 2) continue;

        Uint8 alpha = (!selectedIndex || *selectedIndex == i) ? 255 : 120;
        SDL_SetRenderDrawColor(r, s.color.r, s.color.g, s.color.b, alpha);

        std::vector<SDL_FPoint> pts; pts.reserve(s.points.size());
        for (const auto& p : s.points) pts.push_back(worldToScreen(p.x, p.y));
        SDL_RenderLines(r, pts.data(), (int)pts.size());
    }
    drawCursors(r);
    drawLegend(r);
}


void Plotter::clear() {
    series.clear();
    minX = minY = 0.0; maxX = maxY = 1.0;
    scaleX = scaleY = 1.0; offsetX = offsetY = 0.0;
    cursor1X.reset(); cursor2X.reset();
}

void Plotter::addSeries(const std::string& name,
                        const std::vector<Point>& pts,
                        std::optional<SDL_Color> color) {
    Series s;
    s.name = name;
    s.points = pts;
    s.color = color.value_or(palette[series.size() % (sizeof(palette)/sizeof(palette[0]))]);
    series.push_back(std::move(s));
    recomputeBounds();
    if (autoZoom) applyAutoZoom();
}

void Plotter::setAutoZoom(bool on) { autoZoom = on; if (on) applyAutoZoom(); }

void Plotter::applyAutoZoom() {
    recomputeBounds();
    if (maxX <= minX) maxX = minX + 1.0;
    if (maxY <= minY) maxY = minY + 1.0;
    scaleX = (area.w - 40.0) / (maxX - minX);
    scaleY = (area.h - 40.0) / (maxY - minY);
    offsetX = -minX + 20.0 / scaleX;
    offsetY = -minY + 20.0 / scaleY;
}

void Plotter::zoomX(double f) { scaleX *= f; }
void Plotter::zoomY(double f) { scaleY *= f; }
void Plotter::panX(double dx) { offsetX += dx / scaleX; }
void Plotter::panY(double dy) { offsetY += dy / scaleY; }

void Plotter::enableCursor(bool on) { cursorEnabled = on; if (!on){ cursor1X.reset(); cursor2X.reset(); } }
void Plotter::enableDoubleCursor(bool on) { doubleCursor = on; if (!on) cursor2X.reset(); }

void Plotter::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        if (e.wheel.y > 0) { zoomX(1.1); zoomY(1.1); }
        else if (e.wheel.y < 0) { zoomX(1.0/1.1); zoomY(1.0/1.1); }
    } else if (e.type == SDL_EVENT_MOUSE_MOTION && (e.motion.state & SDL_BUTTON_LMASK)) {
        panX(-e.motion.xrel); panY(e.motion.yrel); // drag to pan
    } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (cursorEnabled) {
            double xw = (e.button.x - area.x)/scaleX - offsetX;
            if (!cursor1X.has_value() || (doubleCursor && cursor1X && cursor2X)) {
                cursor1X = xw; cursor2X.reset();
            } else if (doubleCursor) {
                cursor2X = xw;
            } else { cursor1X = xw; }
        }
    } else if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) { // SDL3: use e.key.key
            case SDLK_A: setAutoZoom(!autoZoom); break;
            case SDLK_C: enableCursor(!cursorEnabled); break;
            case SDLK_D: enableDoubleCursor(!doubleCursor); break;
            case SDLK_LEFTBRACKET:  zoomX(1.0/1.1); break;
            case SDLK_RIGHTBRACKET: zoomX(1.1); break;
            case SDLK_MINUS:        zoomY(1.0/1.1); break;
            case SDLK_EQUALS:       zoomY(1.1); break;
            default: break;
        }
    }
}

SDL_FPoint Plotter::worldToScreen(double x, double y) const {
    float sx = static_cast<float>(area.x + (x + offsetX) * scaleX);
    float sy = static_cast<float>(area.y + area.h - (y + offsetY) * scaleY);
    return SDL_FPoint{sx, sy};
}

void Plotter::drawGrid(SDL_Renderer* r) const {
    SDL_SetRenderDrawColor(r, 230,230,230,255);
    int nx = 10, ny = 8;
    for (int i=0;i<=nx;i++) { float x = area.x + i*(area.w/nx); SDL_RenderLine(r, x, area.y, x, area.y + area.h); }
    for (int j=0;j<=ny;j++) { float y = area.y + j*(area.h/ny); SDL_RenderLine(r, area.x, y, area.x + area.w, y); }
    SDL_SetRenderDrawColor(r, 0,0,0,255); SDL_RenderRect(r, &area);
}

void Plotter::drawLegend(SDL_Renderer* r) const {
    if (!legendVisible || series.empty()) return;

    const float x = area.x + area.w - 150.f, swatch = 10.f;
    float y = area.y + 10.f;
    SDL_FRect box{ x-8, y-6, 140.f, static_cast<float>(16*series.size()+12) };
    SDL_SetRenderDrawColor(r, 255,255,255,230);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 0,0,0,255);
    SDL_RenderRect(r, &box);

    for (size_t i = 0; i < series.size(); ++i) {
        const auto& s = series[i];

        SDL_SetRenderDrawColor(r, s.color.r, s.color.g, s.color.b, 255);
        SDL_RenderLine(r, x, y+8, x+swatch, y+8);

        renderText(r, s.name, x + swatch + 5, y);

        if (selectedIndex && *selectedIndex == i) {
            SDL_SetRenderDrawColor(r, 0,0,0,255);
            SDL_FRect hi{ x-2.f, y+2.f, swatch+4.f, 12.f };
            SDL_RenderRect(r, &hi);
        }
        y += 16.f;
    }
}

void Plotter::drawCursors(SDL_Renderer* r) const {
    if (!cursorEnabled) return;
    SDL_SetRenderDrawColor(r, 20,20,20,180);
    auto drawX = [&](double xw){ SDL_FPoint p1 = worldToScreen(xw, minY); SDL_RenderLine(r, p1.x, area.y, p1.x, area.y + area.h); };
    if (cursor1X) drawX(*cursor1X);
    if (doubleCursor && cursor2X) drawX(*cursor2X);
}


void Plotter::recomputeBounds() {
    if (series.empty()) { minX=minY=0; maxX=maxY=1; return; }
    minX=minY=1e300; maxX=maxY=-1e300;
    for (const auto& s : series)
        for (const auto& p : s.points) {
            minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        }
}

void Plotter::toggleSeriesVisible(int i) {
    if (i < 0 || (size_t)i >= series.size()) return;
    series[(size_t)i].visible = !series[(size_t)i].visible;
}

void Plotter::cycleSeriesColor(int i) {
    if (i < 0 || (size_t)i >= series.size()) return;
    static int rot = 0;
    const int n = int(sizeof(palette)/sizeof(palette[0]));
    series[(size_t)i].color = palette[(rot++ % n)];
}

const std::vector<Point>* Plotter::getSeries(const std::string& name) const {
    for (const auto& s : series) if (s.name == name) return &s.points;
    return nullptr;
}
std::vector<std::string> Plotter::getSeriesNames() const {
    std::vector<std::string> out;
    out.reserve(series.size());
    for (const auto& s : series) out.push_back(s.name);
    return out;
}
bool Plotter::setSeriesVisible(const std::string& name, bool on) {
    for (auto& s : series) if (s.name == name) { s.visible = on; return true; }
    return false;
}
bool Plotter::removeSeries(const std::string& name) {
    auto it = std::remove_if(series.begin(), series.end(),
                             [&](const Series& s){ return s.name == name; });
    if (it == series.end()) return false;
    series.erase(it, series.end());
    recomputeBounds(); if (autoZoom) applyAutoZoom();
    return true;
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
    return true;
}

bool Plotter::removeSelected() {
    if (!selectedIndex || *selectedIndex >= series.size()) return false;
    series.erase(series.begin() + *selectedIndex);
    selectedIndex.reset();
    recomputeBounds(); if (autoZoom) applyAutoZoom();
    return true;
}

bool Plotter::setSeriesName(size_t index, const std::string& newName) {
    if (index >= series.size()) {
        return false;
    }
    series[index].name = newName;
    return true;
}


