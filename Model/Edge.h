//
// Created by Mehrshad on 5/26/2025.
//

#ifndef MORGHSPICY_EDGE_H
#define MORGHSPICY_EDGE_H

#include "Node.h"
#include "ElementTypes.h"

class Edge {
private:
    Node* node1;
    Node* node2;
    ElementType element;
public:
    Edge(Node* n1, Node* n2, ElementType type): node1(n1), node2(n2), element(type) {}
};
#endif //MORGHSPICY_EDGE_H
