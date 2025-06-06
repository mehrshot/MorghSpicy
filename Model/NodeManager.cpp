//
// Created by Ali on 6/6/2025.
//

#include "NodeManager.h"
#include <iostream>

NodeManager::NodeManager(Graph* g) : graph(g), next_id(1) {
    // تعریف اولیه GND
    name_to_id["0"] = 0;
    name_to_id["GND"] = 0;
    graph->addNode(new Node(0, "GND"));
}

int NodeManager::getOrCreateNodeId(const std::string& node_name) {
    // اگر قبلاً ساختیم:
    if (name_to_id.count(node_name)) {
        return name_to_id[node_name];
    }

    // اگر نه، آیدی جدید بده و نود بساز
    int id = next_id++;
    name_to_id[node_name] = id;
    graph->addNode(new Node(id, node_name));
    return id;
}

void NodeManager::assignNodeAsGND(const std::string& node_name) {
    // اگر اسم قبلاً وجود داره ولی آیدی‌اش 0 نیست، هشدار بده
    if (name_to_id.count(node_name) && name_to_id[node_name] != 0) {
        std::cerr << "Warning: Reassigning existing node \"" << node_name << "\" as GND.\n";
    }
    name_to_id[node_name] = 0;
}


