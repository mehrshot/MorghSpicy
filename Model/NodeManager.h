#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cctype>

class Graph; // fwd

class NodeManager {
public:
    NodeManager();

    // ---- public API used by parser / app ----
    // Return canonical numeric id for a token that can be "0", "12", or "vdd"
    int resolveId(const std::string& token);

    // Force a specific label name to refer to a (possibly new) node id, return id
    int labelNode(const std::string& label, int nodeId = -1);

    // Connect/short two labels (or label and number). Returns canonical id.
    int connect(const std::string& a, const std::string& b);

    // Query / set label on an existing node
    std::string nameOf(int nodeId) const;
    void setLabel(int nodeId, const std::string& label);

    // Canonical DSU id for a node id (0 stays 0)
    int canonical(int u) const;

    // After DSU merges, refresh label→id to the canonical representatives.
    void rebuildLabelTable();

// ---- simple wrappers used by CommandParser ----
    int  getOrCreateNodeId(const std::string& tok) { return resolveId(tok); }
    void assignNodeAsGND(const std::string& tok);  // implemented in .cpp (below)
    void displayNodes() const;                      // implemented in .cpp (below)
    void renameNode(const std::string& oldLabel, const std::string& newLabel);
    std::string getNodeNameById(int id) const { return nameOf(id); }

    static bool isGroundToken(const std::string& s);


private:
    // --- DSU / Union-Find ---
    mutable std::unordered_map<int,int> parent;
    mutable std::unordered_map<int,int> rankv;

    static bool isNumber(const std::string& s);
    int  findRep(int u) const;
    int  unite(int a, int b);

    // label <-> id
    std::unordered_map<std::string,int> labelToId;
    std::unordered_map<int,std::string> idToLabel;

    int newNodeId();       // create a fresh positive node id
    int nextId = 1;        // 0 is ground
};
