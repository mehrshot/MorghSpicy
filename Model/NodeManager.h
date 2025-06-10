//
// Created by Ali on 6/6/2025.
//

#ifndef NODE_MANAGER_H
#define NODE_MANAGER_H

#include <string>
#include <unordered_map>
#include <vector> //
#include <algorithm> //
#include <iostream> //
#include "Node.h"
#include "Graph.h"

class NodeManager {
private:
    std::unordered_map<std::string, int> name_to_id;
    std::unordered_map<int, std::string> id_to_name;
    int next_id;
    Graph* graph;

    bool ground_assigned_by_user = false;

public:
    NodeManager(Graph* g);

    int getOrCreateNodeId(const std::string& node_name);

    void assignNodeAsGND(const std::string& node_name);

    void displayNodes() const;

    std::string getNodeNameById(int id) const;

    bool renameNode(const std::string& old_name, const std::string& new_name);
};

#endif // NODE_MANAGER_H