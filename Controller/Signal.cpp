//
// Created by Mehrshad on 8/11/2025.
//

#include "Signal.h"
#include <sstream>

Signal::Signal(std::string filePath, double Fs_, double tStop_, int chunk)
        : fileLocation(std::move(filePath)), Fs(Fs_), tStop(tStop_), chunkSize(chunk) {}

bool Signal::open() { close(); file.open(fileLocation); return file.is_open(); }
void Signal::close() { if (file.is_open()) file.close(); }

bool Signal::readNextChunk() {
    currentChunk.clear();
    if (!file.is_open()) return false;
    currentChunk.reserve(chunkSize);

    std::string line;
    int n = 0;
    while (n < chunkSize && std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        double v;
        if (iss >> v) { currentChunk.push_back(v); ++n; }
    }
    return !currentChunk.empty();
}

void Signal::appendCurrentChunkAsPoints(std::vector<std::pair<double,double>>& out, double tStart) const {
    const double dt = 1.0 / Fs;
    for (size_t i = 0; i < currentChunk.size(); ++i)
        out.emplace_back(tStart + i * dt, currentChunk[i]);
}

std::vector<std::pair<double,double>> Signal::readAllAsPoints() {
    std::vector<std::pair<double,double>> pts;
    if (!open()) return pts;

    double t = 0.0;
    while (readNextChunk() && t <= tStop) {
        appendCurrentChunkAsPoints(pts, t);
        t += currentChunk.size() / Fs;
    }
    close();
    return pts;
}

