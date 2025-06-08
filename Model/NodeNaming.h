//
// Created by Er on 6/6/2025.
//
#ifndef MORGHSPICY_NODENAMING_H
#define MORGHSPICY_NODENAMING_H

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <iostream>

class NodeNaming {
private:
    std::unordered_map<std::string, int> nodeNameToId;
    std::unordered_map<int, std::string> nodeIdToName;
    int nextNodeId;

public:
    NodeNaming() : nextNodeId(1) {
        assignNodeAsGND("GND");
    }

    int getOrCreateNodeId(const std::string& node_name) {
        if (nodeNameToId.count(node_name) && nodeNameToId[node_name] == 0) {
            return 0;
        }
        if (nodeNameToId.count(node_name)) {
            return nodeNameToId[node_name];
        }

        int new_node_id = nextNodeId++;
        nodeNameToId[node_name] = new_node_id;
        nodeIdToName[new_node_id] = node_name;

        return new_node_id;
    }

    void assignNodeAsGND(const std::string& node_name) {
        if (nodeIdToName.count(0) && nodeIdToName[0] != node_name) {
            std::string old_gnd_name = nodeIdToName[0];
            nodeNameToId.erase(old_gnd_name);
        }
        nodeNameToId[node_name] = 0;
        nodeIdToName[0] = node_name;
    }

    bool nodeExists(const std::string& node_name) const {
        return nodeNameToId.count(node_name);
    }

    bool renameNode(const std::string& old_name, const std::string& new_name) {
        if (!nodeExists(old_name)) {
            std::cerr << "ERROR: Node '" << old_name << "' does not exist in the circuit." << std::endl;
            std::cerr << "اقدامات اصلاحی: نام نود را با دستور .nodes بررسی کنید. به حروف بزرگ و کوچک حساس باشید." << std::endl;
            return false;
        }

        if (nodeExists(new_name)) {
            std::cerr << "ERROR: Node name '" << new_name << "' already exists." << std::endl;
            std::cerr << "اقدامات اصلاحی: نام جدیدی برای نود انتخاب کنید و از دستور .nodes برای مشاهده نام‌های موجود استفاده کنید." << std::endl;
            return false;
        }

        if (nodeNameToId[old_name] == 0) {
            assignNodeAsGND(new_name);
        } else {
            int node_id = nodeNameToId[old_name];

            nodeNameToId.erase(old_name);
            nodeNameToId[new_name] = node_id;

            nodeIdToName[node_id] = new_name;
        }

        std::cout << "SUCCESS: Node renamed from '" << old_name << "' to '" << new_name << "'." << std::endl;
        return true;
    }

    void displayNodes() const {
        std::cout << "Available nodes:" << std::endl;
        if (nodeNameToId.empty()) {
            std::cout << "    (No nodes defined yet)" << std::endl;
            return;
        }
        std::vector<std::string> node_names;
        for (const auto& pair : nodeNameToId) {
            node_names.push_back(pair.first);
        }
        std::sort(node_names.begin(), node_names.end());

        for (const std::string& name : node_names) {
            std::cout << "    " << name << " (ID: " << nodeNameToId.at(name) << ")" << std::endl;
        }
    }
};
#endif //MORGHSPICY_NODENAMING_H
