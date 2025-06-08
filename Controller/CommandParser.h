// Created by Ali on 6/6/2025.

#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include <string>
#include <sstream>
#include "Model/Graph.h"
#include "Model/NodeManager.h"
#include "Controller/SimulationRunner.h"

class CommandParser {
private:
    Graph* graph;
    NodeManager* nodeManager;
    SimulationRunner* simRunner;

    void handlePrintCommand(std::istringstream& iss);
    void handleShowSchematics();

public:
    CommandParser(Graph* g, NodeManager* nm, SimulationRunner* runner);
    void parseCommand(const std::string& line);
};

#endif // COMMANDPARSER_H