//
// Created by Mehrshad on 5/26/2025.
//

#ifndef MORGHSPICY_GRAPH_H
#define MORGHSPICY_GRAPH_H

#include <vector>
#include <bits/stdc++.h>
#include "Edge.h"

using namespace std;

class Graph {
private:
    vector <Node*> nodes;
    vector <Edge*> edges;
public:
    void addNode(Node* node) {
        nodes.push_back(node);
    }

    void addEdge(Edge* edge) {
        edges.push_back(edge);
    }

    void desplayGraph() {
        cout << "Nodes in the graph:\n";
        for (auto node: nodes) {
            cout << "Node " << node->getName() << " (ID: " << node->getId() << ")\n";
        }
        cout << "\nEdges in the graph:\n";
        for (auto edge: edges) {
            cout << "Edge between Node " << edge->getNode1()->getName() << " and Node " << edge->getNode2()->getName()
                 << " (Element: " << edge->getElement() << ")\n";
        }
    }
};

#endif //MORGHSPICY_GRAPH_H
