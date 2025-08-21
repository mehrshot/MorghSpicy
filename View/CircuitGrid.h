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
        CircuitGrid(SDL_Window* win,Graph* g, NodeManager* nm);
        ~CircuitGrid(); // <--- اضافه شد: برای آزادسازی منابع فونت

        void handleEvent(const SDL_Event& e);
        void render(SDL_Renderer* ren);
        void commitToModel(Graph* g, NodeManager* nm);

    private:
        SDL_Window* window;
        Graph* graph;
        NodeManager* nm;

        int cellSize = 40;
        std::string currentKind = "Resistor";

        std::vector<PlacedElement> staged;

        std::optional<std::pair<int,int>> pendingFirstNode;
        std::unordered_map<std::string,int> kindCounters;

        // --- انتخاب و ویرایش ---
        std::optional<int> selectedIndex; // ایندکس قطعه‌ی انتخاب‌شده در staged
        bool editing = false;             // آیا در حالت ورود متن هستیم؟
        enum class EditField { None, Name, Value } editField = EditField::None;
        std::string inputBuffer;          // بافر ورودی متنی

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


        static std::optional<double> parseEng(const std::string& s); // "10k" -> 10000
        static std::string trim(const std::string& s);
    };

} // namespace View