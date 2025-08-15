#include "NodeManager.h"
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <cstdlib>

static int nextFreeId(const std::unordered_map<int,int>& parent) {
    // choose the smallest positive integer not used as a key in parent
    int cand = 1;                 // reserve 0 for ground
    while (parent.find(cand) != parent.end()) ++cand;
    return cand;
}

NodeManager::NodeManager() {
    // make sure ground exists and is its own rep
    parent[0] = 0;
    rankv[0]  = 1;

    // keep friendly aliases
    idToLabel[0]   = "0";
    labelToId["0"] = 0;
    labelToId["gnd"] = 0;
    labelToId["GND"] = 0;

    // first non-ground id is 1
    nextId = 1;
}



bool NodeManager::isNumber(const std::string& s) {
    if (s.empty()) return false;
    char* end=nullptr;
    std::strtod(s.c_str(), &end);
    return end == s.c_str() + s.size();
}

int NodeManager::newNodeId() { return nextId++; }

int NodeManager::findRep(int u) const {
    if (u == 0) return 0;
    auto it = parent.find(u);
    if (it == parent.end()) {
        parent[u] = u;
        rankv[u]  = 0;
        return u;
    }
    if (it->second == u) return u;
    return parent[u] = findRep(it->second);
}

int NodeManager::unite(int a, int b) {
    a = findRep(a);
    b = findRep(b);
    if (a == b) return a;

    // keep 0 as absolute root
    if (a == 0) { parent[b] = 0; return 0; }
    if (b == 0) { parent[a] = 0; return 0; }

    int ra = rankv[a], rb = rankv[b];
    if (ra < rb) std::swap(a, b);
    parent[b] = a;
    if (ra == rb) rankv[a]++;
    return a;
}



int NodeManager::resolveId(const std::string& tok) {
    // 1) Ground aliases → node 0
    if (isGroundToken(tok)) return 0;

    // 2) Pure integer string → that numeric id (except "0", already handled)
    char* end = nullptr;
    long v = std::strtol(tok.c_str(), &end, 10);
    if (end && *end == '\0') {
        int id = static_cast<int>(v);
        // ensure DSU bookkeeping exists for this numeric node
        if (!parent.count(id)) parent[id] = id;
        if (!rankv.count(id))  rankv[id]  = 0;
        return id;
    }

    // 3) It’s a label — return existing binding if present
    auto it = labelToId.find(tok);
    if (it != labelToId.end()) return it->second;

    // 4) New label → create a fresh node id and bind it
    int u = nextFreeId(parent);   // smallest unused positive integer
    parent[u] = u;
    rankv[u]  = 0;
    idToLabel[u]   = tok;
    labelToId[tok] = u;
    return u;
}

int NodeManager::labelNode(const std::string& label, int nodeId) {
    if (isGroundToken(label)) return 0;
    if (nodeId < 0) nodeId = newNodeId();
    parent.emplace(nodeId, nodeId);
    rankv.emplace(nodeId, 0);
    // If label already exists, short them
    auto it = labelToId.find(label);
    if (it != labelToId.end()) nodeId = unite(nodeId, it->second);
    labelToId[label] = nodeId;
    idToLabel[nodeId] = label;
    return findRep(nodeId);
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
    // move labels to canonical reps
    std::unordered_map<std::string,int> newL2I;
    std::unordered_map<int,std::string> newI2L;
    for (auto& kv : labelToId) {
        int rep = findRep(kv.second);
        newL2I[kv.first] = rep;
        // prefer first label assigned to that rep
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
    rebuildLabelTable();
}

void NodeManager::displayNodes() const {
    // show canonical id and its (first) label if any
    std::cout << "Nodes (canonical rep -> label):\n";
    // Collect all known ids (parents + label binded ids)
    std::unordered_set<int> ids;
    for (auto& kv : parent) ids.insert(kv.first);
    for (auto& kv : labelToId) ids.insert(kv.second);
    ids.insert(0);

    for (int u : ids) {
        int r = findRep(u);
        auto it = idToLabel.find(r);
        std::string label = (it == idToLabel.end() ? "" : it->second);
        std::cout << "  " << r << (label.empty() ? "" : ("  (" + label + ")")) << "\n";
    }
}

void NodeManager::renameNode(const std::string& oldLabel, const std::string& newLabel) {
    int id = resolveId(oldLabel);
    setLabel(id, newLabel);
    rebuildLabelTable();
}

bool NodeManager::isGroundToken(const std::string& s) {
    return (s == "0" || s == "gnd" || s == "GND");
}
