// Created by Ali on 6/6/2025.

#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include <string>
#include <sstream>
#include "Model/Graph.h"
#include "Model/NodeManager.h"
#include "Controller/SimulationRunner.h"
#include <functional>

class Graph;
class NodeManager;
class SimulationRunner;

class CommandParser {
private:
    Graph* graph;
    NodeManager* nodeManager;
    SimulationRunner* simRunner;

    void handlePrintCommand(std::istringstream& iss);
    void handleShowSchematics();
    void handleSaveCommand(std::istringstream& iss);
    void handleAddComponent(const std::string& sub_name, const std::string& external_n1_str, const std::string& external_n2_str);

    void cmd_scope_load(const std::vector<std::string>& tokens);
    void cmd_scope_clear();

    // helpers
    int nodeFromToken(const std::string& tok); // delegates to nm->resolveId
    void handleLabel(std::istringstream& in);
    void handleConnect(std::istringstream& in);

public:

    CommandParser();  // <- DECLARE default ctor (you define it in .cpp)

    CommandParser(Graph* g, NodeManager* n, SimulationRunner* r);

    // Optional callbacks for scope->App bridge (set these in App if you want)
    std::function<void(const std::string& path, double Fs, double tStop, int chunk)> onScopeLoad;
    std::function<void()> onScopeClear;

// New wrapper that adds 'scope' commands then falls back to your legacy parser
    void parseCommand(const std::string& line);

    void parseCommandCore(const std::string& line);

};

#endif // COMMANDPARSER_H