//
// Created by Mehrshad on 8/9/2025.
//

#include "Plotter.h"

Plotter::Plotter(SDL_FRect plotArea) : area(plotArea), minX(0), maxX(0), minY(0), maxY(0) {}

void Plotter::setData(const PlotData& data) {
    seriesPoints.clear();
    if (data.time_axis.empty() || data.data_series.empty()) return;

    minX = data.time_axis.front();
    maxX = data.time_axis.back();
    minY = maxY = data.data_series[0][0];

    for(const auto& series : data.data_series) {
        for(double val : series) {
            if (val < minY) minY = val;
            if (val > maxY) maxY = val;
        }
    }

    for(const auto& series_data : data.data_series) {
        std::vector<Point> points;
        for(size_t i = 0; i < data.time_axis.size(); ++i) {
            points.push_back({data.time_axis[i], series_data[i]});
        }
        seriesPoints.push_back(points);
    }
}

void Plotter::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderRect(renderer, &area);

    if (seriesPoints.empty()) return;

    double rangeX = maxX - minX;
    if (rangeX == 0) rangeX = 1;
    double rangeY = maxY - minY;
    if (rangeY == 0) rangeY = 1;

    // رنگ های مختلف برای نمودار های مختلف
    SDL_Color colors[] = {{255, 0, 0, 255}, {0, 0, 255, 255}, {0, 255, 0, 255}};
    int colorIndex = 0;

    for (const auto& points : seriesPoints) {
        if (points.size() < 2) continue;

        SDL_SetRenderDrawColor(renderer, colors[colorIndex % 3].r, colors[colorIndex % 3].g, colors[colorIndex % 3].b, 255);
        colorIndex++;

        std::vector<SDL_FPoint> sdlPoints;
        for (const auto& p : points) {
            float screenX = area.x + static_cast<float>(((p.x - minX) / rangeX) * area.w);
            float screenY = area.y + area.h - static_cast<float>(((p.y - minY) / rangeY) * area.h);
            sdlPoints.push_back({screenX, screenY});
        }
        SDL_RenderLines(renderer, sdlPoints.data(), sdlPoints.size());
    }
}