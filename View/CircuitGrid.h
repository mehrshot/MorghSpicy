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



        std::string name;     // نام قابل ویرایش (R1, Rload, ...)
        std::string valueStr; // رشته‌ی مقدار (مثلا "10k", "1u", "default")
        double      value = 0.0; // مقدار عددی (برای R/C/L/V/I)
    };

    class CircuitGrid {
    public:
        CircuitGrid(SDL_Window* win, Graph* g, NodeManager* nm, CommandParser* p);
        ~CircuitGrid();

        void handleEvent(const SDL_Event& e);
        void render(SDL_Renderer* ren);
        void commitToModel();
    private:
        SDL_Window* window;
        Graph* graph;
        NodeManager* nm;
        CommandParser* parser;

        int cellSize = 40;
        std::string currentKind = "Resistor";

        enum class ToolKind { Wire, Resistor, Capacitor, Inductor, VoltageSource, CurrentSource, Diode, Ground };
        ToolKind currentTool = ToolKind::Wire;

        std::vector<PlacedElement> staged;

        std::optional<std::pair<int,int>> pendingFirstNode;
        std::unordered_map<ToolKind,int> kindCounters;

        // --- انتخاب و ویرایش ---
        std::optional<int> selectedIndex; // ایندکس قطعه‌ی انتخاب‌شده در staged
        bool editing = false;             // آیا در حالت ورود متن هستیم؟
        enum class EditField { None, Name, Value } editField = EditField::None;
        std::string inputBuffer;          // بافر ورودی متنی

        uint64_t lastClickTicks = 0;
        SDL_FPoint lastClickPos{0,0};

        // --- اعضای مربوط به رندر متن ---  <--- اضافه شد
        TTF_Font* font = nullptr;
        SDL_Color textColor = {0, 0, 0, 255}; // رنگ متن: مشکی

        // helpers
        void startEditName();
        void startEditValue();
        void commitEdit();
        void cancelEdit();

        // --- توابع کمکی برای متن --- <--- اضافه شد
        void loadFont(const std::string& fontPath, int size);
        void renderText(SDL_Renderer* ren, const std::string& text, int x, int y, bool isEditing = false);
    };

} // namespace View