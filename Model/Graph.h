//
// Created by Mehrshad on 5/26/2025.
//

#ifndef MORGHSPICY_GRAPH_H
#define MORGHSPICY_GRAPH_H

#include <stack>

#include "Common_Includes.h"
#include "Node.h"
#include "Edge.h"
#include "Elements.h"

class Graph {
private:
    std::vector<Node*> nodes;
    std::vector<Edge*> edges;
    std::vector<Element*> elements;

public:
    Graph() = default;

    ~Graph() {
        // Delete all Node objects pointed to
        for (Node* node_ptr : nodes) {
            delete node_ptr;
        }
        nodes.clear();

        // Delete all Edge objects pointed to
        for (Edge* edge_ptr : edges) {
            delete edge_ptr;
        }
        edges.clear();

        // Delete all Element objects pointed to
        for (Element* elem_ptr : elements) {
            delete elem_ptr;
        }
        elements.clear();
    }
    void displayElementsByType(const std::string& type_filter) {
        std::cout << "Elements of type '" << type_filter << "' in the graph:\n";
        char filter_char = toupper(type_filter[0]);
        bool found = false;
        for (const auto& elem : elements) {
            if (toupper(elem->name[0]) == filter_char) {
                elem->display();
                found = true;
            }
        }
        if (!found) {
            std::cout << "No elements of type '" << type_filter << "' found.\n";
        }
    }

    void updateTimeDependentSources(double time) {
        for (Element* elem : elements) {
            // Attempt to cast to each type of time-dependent source
            if (auto* pulse_src = dynamic_cast<PulseSource*>(elem)) {
                pulse_src->updateTime(time);
            } else if (auto* sin_src = dynamic_cast<SinusoidalSource*>(elem)) {
                sin_src->updateTime(time);
            }
        }
    }

    bool isConnected() const {
        if (nodes.empty() || elements.empty()) {
            return false; // An empty or element-less circuit is not considered validly connected
        }

        std::map<int, std::vector<int>> adj;
        for (const auto* elem : elements) {
            adj[elem->node1].push_back(elem->node2);
            adj[elem->node2].push_back(elem->node1);
        }

        std::set<int> visited;
        std::stack<int> stack;

        stack.push(0);

        while (!stack.empty()) {
            int u = stack.top();
            stack.pop();

            if (visited.find(u) == visited.end()) {
                visited.insert(u);
                if (adj.count(u)) {
                    for (int v : adj.at(u)) {
                        if (visited.find(v) == visited.end()) {
                            stack.push(v);
                        }
                    }
                }
            }
        }

        for (const auto& pair : adj) {
            if (visited.find(pair.first) == visited.end()) {
                return false; // A node was not reached, so the graph is disconnected
            }
        }

        return true;
    }

    // Add Node
    void addNode(Node* node) {
        nodes.push_back(node);
    }

    Element* findElement(const std::string& name) const {
        for (Element* elem_ptr : elements) {
            if (elem_ptr->name == name) {
                return elem_ptr;
            }
        }
        return nullptr;
    }

    // Add Edge
    void addEdge(Edge* edge) {
        edges.push_back(edge);
    }

    // Add Element
    void addElement(Element* elem) {
        elements.push_back(elem);
    }

    // Element Getter
    const std::vector<Element*>& getElements() const {
        return elements;
    }

    // Node Getter
    const std::vector<Node*>& getNodes() const {
        return nodes;
    }

    // Display
    void desplayGraph() {
        std::cout << "Nodes in the graph:\n";
        for (const auto& node : nodes) {
            std::cout << "Node " << node->getName() << " (ID: " << node->getId() << ")\n";
        }
        std::cout << "\nElements in the graph:\n";
        for (const auto& elem : elements) {
            elem->display();
        }
    }

    //deleting an element by its name(used in command handler)
    bool removeElementByName(const std::string& name) {
        for (auto it = elements.begin(); it != elements.end(); ++it) {
            if ((*it)->name == name) {
                delete *it; // 1. Deallocate the Element object's memory

                // 2. Erase the pointer from the vector and get the next valid iterator
                // This is the correct way to erase while iterating.
                elements.erase(it);

                std::cout << "Element '" << name << "' removed." << std::endl;
                return true; // Exit the loop and function immediately
            }
        }
        // This message is now part of the calling function in CommandParser
        // std::cerr << "Element '" << name << "' not found." << std::endl;
        return false;
    }

};

#endif // MORGHSPICY_GRAPH_H