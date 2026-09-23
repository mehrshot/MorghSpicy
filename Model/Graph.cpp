//
// Created by Mehrshad on 8/15/2025.
//

#include "Graph.h"
#include "NodeManager.h"
#include <stack>

void Graph::canonicalizeNodes(const NodeManager& nm) {
    for (Element* el : elements) {
        if (!el) continue;
        el->node1 = nm.canonical(el->node1);
        el->node2 = nm.canonical(el->node2);

        // dependent sources also keep control nodes
        if (auto* g = dynamic_cast<vccs*>(el)) {
            g->setControlNodes(nm.canonical(g->controlPos()), nm.canonical(g->controlNeg()));
        } else if (auto* e = dynamic_cast<vcvs*>(el)) {
            e->setControlNodes(nm.canonical(e->controlPos()), nm.canonical(e->controlNeg()));
        }
    }
}

void Graph::displayElementsByType(const std::string& type_filter) {
    if (type_filter.empty()) return;
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

void Graph::updateTimeDependentSources(double time) {
    for (Element* elem : elements) {
        if (auto* pulse_src = dynamic_cast<PulseSource*>(elem)) {
            pulse_src->updateTime(time);
        } else if (auto* sin_src = dynamic_cast<SinusoidalSource*>(elem)) {
            sin_src->updateTime(time);
        }
    }
}

bool Graph::hasGround() const {
    for (const Element* e : elements) {
        if (e->node1 == 0 || e->node2 == 0) return true;
    }
    return false;
}

// every node has to be reachable from ground through the elements
bool Graph::isConnected() const {
    if (elements.empty() || !hasGround()) return false;

    std::map<int, std::vector<int>> adj;
    for (const auto* elem : elements) {
        adj[elem->node1].push_back(elem->node2);
        adj[elem->node2].push_back(elem->node1);
    }

    std::set<int> visited;
    std::stack<int> st;
    st.push(0);
    while (!st.empty()) {
        int u = st.top();
        st.pop();
        if (!visited.insert(u).second) continue;
        for (int v : adj[u]) {
            if (!visited.count(v)) st.push(v);
        }
    }

    for (const auto& pair : adj) {
        if (!visited.count(pair.first)) return false;
    }
    return true;
}

void Graph::displayGraph() {
    if (elements.empty()) {
        std::cout << "Circuit is empty.\n";
        return;
    }
    std::cout << "Elements in the graph:\n";
    for (const auto& elem : elements) {
        elem->display();
    }
}

bool Graph::removeElementByName(const std::string& name) {
    for (auto it = elements.begin(); it != elements.end(); ++it) {
        if ((*it)->name == name) {
            delete *it;
            elements.erase(it);
            std::cout << "Element '" << name << "' removed." << std::endl;
            return true;
        }
    }
    return false;
}
