//
// Created by Ali on 6/6/2025.
//

#ifndef NODE_MANAGER_H
#define NODE_MANAGER_H

#include <string>
#include <unordered_map>
#include "Node.h"
#include "Graph.h"

class NodeManager {
private:
    std::unordered_map<std::string, int> name_to_id;
    int next_id;
    Graph* graph;

public:
    NodeManager(Graph* g);

    // اگر نود با این اسم وجود داشت، IDش رو برمی‌گردونه، وگرنه می‌سازه
    int getOrCreateNodeId(const std::string& node_name);

    // تخصیص مستقیم یک نام به GND (ID=0)
    void assignNodeAsGND(const std::string& node_name);
};

#endif // NODE_MANAGER_H


