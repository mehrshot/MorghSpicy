//
// Created by Ali on 6/6/2025.
//

#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include <string>
#include "Graph.h"
#include "NodeManager.h"

class CommandParser {
private:
    Graph* graph;
    NodeManager* nodeManager;

public:
    CommandParser(Graph* g, NodeManager* nm);
    void parseCommand(const std::string& line);
};

#endif // COMMANDPARSER_H


