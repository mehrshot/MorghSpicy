#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <cctype>

// Keeps track of node names and which nodes are shorted together.
// Nodes can be numbers ("0", "12") or labels ("vdd", "N3_4").
// Shorting (wires, GND) is done with a union-find, 0 is always ground.
class NodeManager {
public:
    NodeManager();

    // forget everything (used when the grid re-builds the circuit)
    void reset();

    // canonical numeric id for "0", "12" or "vdd" (creates the node if it's new)
    int resolveId(const std::string& token);

    // same thing but never creates a node
    std::optional<int> findId(const std::string& token) const;

    // give a label to a (possibly new) node, return id
    int labelNode(const std::string& label, int nodeId = -1);

    // short two nodes together, returns the representative id
    int connect(const std::string& a, const std::string& b);

    std::string nameOf(int nodeId) const;
    void setLabel(int nodeId, const std::string& label);

    // union-find representative (0 stays 0)
    int canonical(int u) const;

    // after merges, move labels to the representatives
    void rebuildLabelTable();

    void assignNodeAsGND(const std::string& tok);
    void displayNodes() const;
    bool renameNode(const std::string& oldLabel, const std::string& newLabel);
    std::string getNodeNameById(int id) const { return nameOf(id); }

    static bool isGroundToken(const std::string& s);
    int newNodeId();

private:
    mutable std::unordered_map<int,int> parent;
    mutable std::unordered_map<int,int> rankv;

    int  findRep(int u) const;
    int  unite(int a, int b);
    static bool isInteger(const std::string& s);

    // label <-> id
    std::unordered_map<std::string,int> labelToId;
    std::unordered_map<int,std::string> idToLabel;
};
