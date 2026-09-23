#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vector>
#include <string>
#include <optional>
#include "Model/Graph.h"
#include "Model/NodeManager.h"
#include "Controller/CommandParser.h"

namespace View {

    struct PlacedElement {
        std::string kind;
        std::string n1, n2;     // node labels, "N<gx>_<gy>"
        int gx1 = 0, gy1 = 0;
        int gx2 = 0, gy2 = 0;
        std::string name;
        std::string valueStr;
    };

    class CircuitGrid {
    public:
        CircuitGrid(SDL_Window* win, Graph* g, NodeManager* nm, CommandParser* p, TTF_Font* font);

        void handleEvent(const SDL_Event& e);
        void render(SDL_Renderer* ren);

        // rebuild the model from what's drawn (only if something changed, unless force)
        void commitToModel(bool force = false);
        bool isEditing() const { return editing; }

    private:
        SDL_Window* window;
        Graph* graph;
        NodeManager* nm;
        CommandParser* parser;
        TTF_Font* font = nullptr;
        SDL_Color textColor = {0, 0, 0, 255};

        const int cellSize = 40;
        const int statusBarH = 28;

        enum class ToolKind { Wire, Resistor, Capacitor, Inductor, VoltageSource, CurrentSource, Diode, Ground, VoltageProbe, CurrentProbe };
        ToolKind currentTool = ToolKind::Wire;

        std::vector<PlacedElement> staged;
        std::optional<std::pair<int,int>> pendingFirstNode;
        bool dirty = false;

        std::optional<size_t> selectedIndex;
        bool editing = false;
        enum class EditField { None, Name, Value } editField = EditField::None;
        std::string inputBuffer;

        uint64_t lastClickTicks = 0;
        SDL_FPoint lastClickPos{0,0};

        static std::string nodeLabel(int gx, int gy);
        static const char* toolName(ToolKind t);
        std::string nextName(const std::string& prefix) const;
        std::optional<size_t> hitTest(float mx, float my) const;
        int gridBottom() const;

        void placeElement(int gx, int gy);
        void probe(float mx, float my);

        void startEditName();
        void startEditValue();
        void commitEdit();
        void cancelEdit();

        void drawElement(SDL_Renderer* ren, const PlacedElement& pe, bool selected);
        void drawStatusBar(SDL_Renderer* ren, int w, int h);
        void renderText(SDL_Renderer* ren, const std::string& text, float x, float y, bool boxed = false);
    };

}
