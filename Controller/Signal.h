//
// Created by Mehrshad on 8/11/2025.
//

#ifndef MORGHSPICY_SIGNAL_H
#define MORGHSPICY_SIGNAL_H


#pragma once
#include <string>
#include <fstream>
#include <vector>

class Signal {
public:
    // Text file: one sample (double) per line.
    explicit Signal(std::string filePath = "", double Fs = 1000.0, double tStop = 1.0, int chunk = 4096);

    bool open();
    void close();

    // Read next chunk; returns false on EOF.
    bool readNextChunk();

    // Append current chunk as (t,y) points starting from tStart
    void appendCurrentChunkAsPoints(std::vector<std::pair<double,double>>& out, double tStart) const;

    // Convenience: read whole file and return (t,y) points
    std::vector<std::pair<double,double>> readAllAsPoints();

    // Config
    void setFs(double fs) { Fs = fs; }
    void setTStop(double t) { tStop = t; }
    void setChunkSize(int n) { chunkSize = n; }
    const std::string& path() const { return fileLocation; }

private:
    std::string fileLocation;
    std::ifstream file;
    double Fs;      // samples per second
    double tStop;   // seconds
    int chunkSize;  // samples per read
    std::vector<double> currentChunk;
};



#endif //MORGHSPICY_SIGNAL_H
