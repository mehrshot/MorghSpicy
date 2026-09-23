// Created by Ali on 6/6/2025.

#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <optional>
#include <filesystem>
#include "Model/Graph.h"
#include "Model/NodeManager.h"
#include "Controller/SimulationRunner.h"

class CommandParser {
private:
    Graph* graph = nullptr;
    NodeManager* nodeManager = nullptr;
    SimulationRunner* simRunner = nullptr;

    // settings used by "plot v ..." / "plot i ..." (change them with the "tran" command)
    double tranStep = 1e-5;
    double tranStop = 5e-2;
    double tranMax = 0.0;

    // files from the last "show existing schematics", so "load 2" works
    std::vector<std::filesystem::path> lastSchematics;

    void handleAdd(std::istringstream& iss);
    void handlePrintCommand(std::istringstream& iss);
    void handleShowSchematics();
    void handleLoad(std::istringstream& iss);
    void handleSaveCommand(std::istringstream& iss);
    void handleSubcircuitCreate(std::istringstream& iss);
    void handleAddComponent(const std::string& instance_name, const std::string& sub_name,
                            const std::string& external_n1_str, const std::string& external_n2_str);
    void handleLabel(std::istringstream& in);
    void handleConnect(std::istringstream& in);
    void handlePlot(std::istringstream& in);
    void handleTranSettings(std::istringstream& in);
    void handleAC(std::istringstream& iss, bool print);
    void handlePhase(std::istringstream& iss, bool print);
    void handleValue(std::istringstream& iss);
    void handleScope(const std::vector<std::string>& parts);
    void printHelp() const;

    std::vector<OutputVariable> readOutputs(std::istringstream& iss) const;
    void emit(const PlotData& pd, bool printTable);
    std::string nodeName(int id) const;

public:
    CommandParser() = default;
    CommandParser(Graph* g, NodeManager* n, SimulationRunner* r);

    // callbacks to the GUI (App sets them, the CLI leaves them empty)
    std::function<void(const std::string& path, double Fs, double tStop, int chunk)> onScopeLoad;
    std::function<void()> onScopeClear;
    std::function<void(const PlotData&)> onPlotData;

    void parseCommand(const std::string& line);

    static std::optional<double> parseValueWithPrefix(const std::string& str);
};

#endif // COMMANDPARSER_H
