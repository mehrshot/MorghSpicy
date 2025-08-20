#pragma once
#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include <optional>
#include <memory>
#include "../Model/Graph.h"
#include "../Model/NodeManager.h"
#include "../Model/Elements.h"

namespace View {

    struct PlacedElement {
        std::string kind;
        std::string n1, n2;
        int gx1, gy1;
        int gx2, gy2;
    };

    class CircuitGrid {
    public:
        CircuitGrid(Graph* g, NodeManager* nm);

        void handleEvent(const SDL_Event& e);
        void render(SDL_Renderer* ren);
        void commitToModel(Graph* g, NodeManager* nm);

    private:
        Graph* graph;
        NodeManager* nm;

        int cellSize = 40;
        std::string currentKind = "Resistor";

        std::vector<PlacedElement> staged;

        // برای ذخیره اولین کلیک (نقطه‌ی شروع المان)
        std::optional<std::pair<int, int>> pendingFirstNode;
        std::unordered_map<std::string, int> kindCounters;
    };

} // namespace View
