#include "NodeManager.h"
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <set>

NodeManager::NodeManager() {
    reset();
}

void NodeManager::reset() {
    parent.clear();
    rankv.clear();
    labelToId.clear();
    idToLabel.clear();

    // ground always exists and is its own root
    parent[0] = 0;
    rankv[0]  = 1;
    idToLabel[0] = "0";
    labelToId["0"] = 0;
    labelToId["gnd"] = 0;
    labelToId["GND"] = 0;
}

bool NodeManager::isInteger(const std::string& s) {
    if (s.empty()) return false;
    char* end = nullptr;
    std::strtol(s.c_str(), &end, 10);
    return end && *end == '\0';
}

bool NodeManager::isGroundToken(const std::string& s) {
    return (s == "0" || s == "gnd" || s == "GND");
}

// smallest positive id that isn't taken yet
int NodeManager::newNodeId() {
    int cand = 1;
    while (parent.count(cand)) ++cand;
    parent[cand] = cand;
    rankv[cand] = 0;
    return cand;
}

int NodeManager::findRep(int u) const {
    if (u == 0) return 0;
    auto it = parent.find(u);
    if (it == parent.end()) {
        parent[u] = u;
        rankv[u]  = 0;
        return u;
    }
    if (it->second == u) return u;
    int r = findRep(it->second);
    parent[u] = r;
    return r;
}

int NodeManager::unite(int a, int b) {
    a = findRep(a);
    b = findRep(b);
    if (a == b) return a;

    // keep 0 as the root when ground is involved
    if (a == 0) { parent[b] = 0; return 0; }
    if (b == 0) { parent[a] = 0; return 0; }

    int ra = rankv[a], rb = rankv[b];
    if (ra < rb) std::swap(a, b);
    parent[b] = a;
    if (ra == rb) rankv[a]++;
    return a;
}

int NodeManager::resolveId(const std::string& tok) {
    if (isGroundToken(tok)) return 0;

    if (isInteger(tok)) {
        int id = std::atoi(tok.c_str());
        if (!parent.count(id)) { parent[id] = id; rankv[id] = 0; }
        return findRep(id);
    }

    auto it = labelToId.find(tok);
    if (it != labelToId.end()) return findRep(it->second);

    // new label -> fresh node
    int u = newNodeId();
    idToLabel[u]   = tok;
    labelToId[tok] = u;
    return u;
}

std::optional<int> NodeManager::findId(const std::string& tok) const {
    if (isGroundToken(tok)) return 0;
    if (isInteger(tok)) {
        int id = std::atoi(tok.c_str());
        if (!parent.count(id)) return std::nullopt;
        return findRep(id);
    }
    auto it = labelToId.find(tok);
    if (it == labelToId.end()) return std::nullopt;
    return findRep(it->second);
}

int NodeManager::labelNode(const std::string& label, int nodeId) {
    if (isGroundToken(label)) return 0;
    if (nodeId < 0) nodeId = newNodeId();
    parent.emplace(nodeId, nodeId);
    rankv.emplace(nodeId, 0);

    // same label twice means the same net
    auto it = labelToId.find(label);
    if (it != labelToId.end()) nodeId = unite(nodeId, it->second);

    int rep = findRep(nodeId);
    labelToId[label] = rep;
    if (!idToLabel.count(rep)) idToLabel[rep] = label;
    return rep;
}

int NodeManager::connect(const std::string& a, const std::string& b) {
    int ia = resolveId(a);
    int ib = resolveId(b);
    int r  = unite(ia, ib);
    rebuildLabelTable();
    return r;
}

std::string NodeManager::nameOf(int nodeId) const {
    auto it = idToLabel.find(findRep(nodeId));
    return it == idToLabel.end() ? std::string{} : it->second;
}

void NodeManager::setLabel(int nodeId, const std::string& label) {
    if (nodeId == 0 || label.empty()) return;
    int rep = findRep(nodeId);
    labelToId[label] = rep;
    idToLabel[rep]   = label;
}

int NodeManager::canonical(int u) const { return findRep(u); }

void NodeManager::rebuildLabelTable() {
    std::unordered_map<std::string,int> newL2I;
    std::unordered_map<int,std::string> newI2L;

    // keep the old display name of a representative if it still has one
    for (auto& kv : idToLabel) {
        int rep = findRep(kv.first);
        if (kv.first == rep) newI2L[rep] = kv.second;
    }
    for (auto& kv : labelToId) {
        int rep = findRep(kv.second);
        newL2I[kv.first] = rep;
        if (!newI2L.count(rep)) newI2L[rep] = kv.first;
    }
    labelToId.swap(newL2I);
    idToLabel.swap(newI2L);

    idToLabel[0] = "0";
    labelToId["0"]   = 0;
    labelToId["gnd"] = 0;
    labelToId["GND"] = 0;
}

void NodeManager::assignNodeAsGND(const std::string& tok) {
    connect(tok, "0");
}

void NodeManager::displayNodes() const {
    std::set<int> reps;
    for (auto& kv : parent) reps.insert(findRep(kv.first));

    std::cout << "Nodes:\n";
    for (int r : reps) {
        std::cout << "  " << r;
        // print every label that points to this node
        std::vector<std::string> names;
        for (auto& kv : labelToId) {
            if (findRep(kv.second) == r && kv.first != "gnd" && kv.first != "GND" && kv.first != "0")
                names.push_back(kv.first);
        }
        std::sort(names.begin(), names.end());
        if (r == 0) std::cout << "  (GND)";
        for (auto& s : names) std::cout << "  " << s;
        std::cout << "\n";
    }
}

bool NodeManager::renameNode(const std::string& oldLabel, const std::string& newLabel) {
    auto id = findId(oldLabel);
    if (!id) return false;
    if (isGroundToken(newLabel) || *id == 0) return false;
    labelToId.erase(oldLabel);
    setLabel(*id, newLabel);
    rebuildLabelTable();
    return true;
}
