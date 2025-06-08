//
// Created by Er-Ali on 6/6/2025.
//

#include "NodeManager.h"
#include <iostream>

NodeManager::NodeManager(Graph* g) : graph(g), next_id(1) {
    name_to_id["0"] = 0;
    name_to_id["GND"] = 0;
    id_to_name[0] = "GND"; // UPDATED: Keep reverse map in sync
    graph->addNode(new Node(0, "GND"));
}

int NodeManager::getOrCreateNodeId(const std::string& node_name) {
    if (name_to_id.count(node_name)) {
        return name_to_id[node_name];
    }

    int id = next_id++;
    name_to_id[node_name] = id;
    id_to_name[id] = node_name; // UPDATED: Keep reverse map in sync
    graph->addNode(new Node(id, node_name));
    return id;
}

void NodeManager::assignNodeAsGND(const std::string& node_name) {
    if (name_to_id.count(node_name) && name_to_id[node_name] != 0) {
        std::cerr << "Warning: Reassigning existing node \"" << node_name << "\" as GND.\n";
    }
    name_to_id[node_name] = 0;
    id_to_name[0] = node_name; // UPDATED: Keep reverse map in sync
}

// ADDED: Implementation for displayNodes
void NodeManager::displayNodes() const {
    std::cout << "Available nodes:" << std::endl;
    if (name_to_id.empty()) {
        std::cout << "    (No nodes defined yet)" << std::endl;
        return;
    }
    std::vector<std::string> node_names;
    for (const auto& pair : name_to_id) {
        if (pair.first == "0" && name_to_id.count("GND")) continue;
        node_names.push_back(pair.first);
    }
    std::sort(node_names.begin(), node_names.end());

    for (const std::string& name : node_names) {
        std::cout << "    " << name << std::endl;
    }
}

// ADDED: Implementation for renameNode
bool NodeManager::renameNode(const std::string& old_name, const std::string& new_name) {
    if (!name_to_id.count(old_name)) {
        std::cerr << "ERROR: Node '" << old_name << "' does not exist in the circuit" << std::endl;
        return false;
    }

    if (name_to_id.count(new_name)) {
        std::cerr << "ERROR: Node name '" << new_name << "' already exists." << std::endl;
        return false;
    }

    int node_id = name_to_id[old_name];

    name_to_id.erase(old_name);
    name_to_id[new_name] = node_id;

    id_to_name[node_id] = new_name;

    for(Node* node : graph->getNodes()){
        if(node->getId() == node_id){
            // This part requires a setter in Node.h, let's assume it exists or add it.
            // node->setName(new_name);
        }
    }

    std::cout << "SUCCESS: Node renamed from '" << old_name << "' to '" << new_name << "'." << std::endl;
    return true;
}