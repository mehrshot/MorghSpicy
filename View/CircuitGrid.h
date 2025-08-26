#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vector>
#include <string>
#include <optional>
#include <memory>
#include <unordered_map>
#include "../Model/Graph.h"
#include "../Model/NodeManager.h"
#include "../Model/Elements.h"
#include "../Controller/CommandParser.h"

namespace View {

    struct PlacedElement {
        std::string kind;
        std::string n1, n2;
        int gx1, gy1;
        int gx2, gy2;
        std::string name;
        std::string valueStr;
        double      value = 0.0;
    };

    class CircuitGrid {
    public:
        CircuitGrid(SDL_Window* win, Graph* g, NodeManager* nm, CommandParser* p, TTF_Font* font);
        ~CircuitGrid();

        void handleEvent(const SDL_Event& e);
        void render(SDL_Renderer* ren);
        void commitToModel();

    private:
        SDL_Window* window;
        Graph* graph;
        NodeManager* nm;
        CommandParser* parser;
        TTF_Font* font = nullptr;
        SDL_Color textColor = {0, 0, 0, 255};

        int cellSize = 40;
        std::string currentKind = "Resistor";

        enum class ToolKind { Wire, Resistor, Capacitor, Inductor, VoltageSource, CurrentSource, Diode, Ground, VoltageProbe, CurrentProbe };
        ToolKind currentTool = ToolKind::Wire;

        std::vector<PlacedElement> staged;
        std::optional<std::pair<int,int>> pendingFirstNode;
        std::unordered_map<ToolKind,int> kindCounters;

        std::optional<int> selectedIndex;
        bool editing = false;
        enum class EditField { None, Name, Value } editField = EditField::None;
        std::string inputBuffer;

        uint64_t lastClickTicks = 0;
        SDL_FPoint lastClickPos{0,0};

        void startEditName();
        void startEditValue();
        void commitEdit();
        void cancelEdit();
        void renderText(SDL_Renderer* ren, const std::string& text, int x, int y, bool isEditing = false);
    };

}