//
// Created by Mehrshad on 8/15/2025.
//

#include "Graph.h"
#include "NodeManager.h"

void Graph::canonicalizeNodes(const NodeManager& nm) {
    // Update all element node indices to their canonical reps in NodeManager
    for (auto& e : elements) {                 // adjust if your container is different
        if (!e) continue;
        auto* el = e; // if elements is vector<unique_ptr<Element>>
        // if it's vector<Element*> then: auto* el = e;

        el->node1 = nm.canonical(el->node1);
        el->node2 = nm.canonical(el->node2);

        // If your Element has control-node fields (common for dependent sources),
        // keep these lines; otherwise delete them.
//        if (el->c1 != -1) el->c1 = nm.canonical(el->c1);
//        if (el->c2 != -1) el->c2 = nm.canonical(el->c2);
    }
}
