//
// Created by Mehrshad on 5/26/2025.
//

#ifndef MORGHSPICY_GRAPH_H
#define MORGHSPICY_GRAPH_H

#include "Common_Includes.h"
#include "Elements.h"

class NodeManager;

class Graph {
public:
    std::vector<Element*> elements;

    Graph() = default;
    ~Graph() { clear(); }

    // the graph owns its elements, so no copies
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    // replace every node id with its representative in the node manager
    void canonicalizeNodes(const NodeManager& nm);

    void displayElementsByType(const std::string& type_filter);
    void updateTimeDependentSources(double time);

    bool hasGround() const;
    bool isConnected() const;

    Element* findElement(const std::string& name) const {
        for (Element* elem_ptr : elements) {
            if (elem_ptr->name == name) return elem_ptr;
        }
        return nullptr;
    }

    void addElement(Element* elem) { elements.push_back(elem); }
    const std::vector<Element*>& getElements() const { return elements; }

    void displayGraph();

    // deleting an element by its name (used in command handler)
    bool removeElementByName(const std::string& name);

    void clear() {
        for (Element* elem_ptr : elements) delete elem_ptr;
        elements.clear();
    }
};

#endif // MORGHSPICY_GRAPH_H
