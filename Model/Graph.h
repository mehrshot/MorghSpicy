//
// Created by Mehrshad on 5/26/2025.
//

#ifndef MORGHSPICY_GRAPH_H
#define MORGHSPICY_GRAPH_H

#include <vector>
#include "Edge.h"

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

    }
};
#endif //MORGHSPICY_GRAPH_H
