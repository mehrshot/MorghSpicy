// Created by Ali on 6/6/2025.

#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include <string>
#include <sstream>
#include "Model/Graph.h"
#include "Model/NodeManager.h"
#include "Controller/SimulationRunner.h"
#include <functional>


class CommandParser {
private:
    Graph* graph;
    NodeManager* nodeManager;
    SimulationRunner* simRunner;

    void handlePrintCommand(std::istringstream& iss);
    void handleShowSchematics();
    void handleSaveCommand(std::istringstream& iss);

    void cmd_scope_load(const std::vector<std::string>& tokens);
    void cmd_scope_clear();

public:

    CommandParser();  // <- DECLARE default ctor (you define it in .cpp)
    CommandParser(Graph* g, NodeManager* nm, SimulationRunner* runner);

    // Optional callbacks for scope->App bridge (set these in App if you want)
    std::function<void(const std::string& path, double Fs, double tStop, int chunk)> onScopeLoad;
    std::function<void()> onScopeClear;

// New wrapper that adds 'scope' commands then falls back to your legacy parser
    void parseCommand(const std::string& line);

// Rename your old parseCommand(...) body to this:
    void parseCommandCore(const std::string& line);

};

#endif // COMMANDPARSER_H