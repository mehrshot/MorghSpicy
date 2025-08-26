#include "CircuitGrid.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <iostream>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


namespace View {

    CircuitGrid::CircuitGrid(SDL_Window* win, Graph* g, NodeManager* n, CommandParser* p, TTF_Font* main_font)
            : window(win), graph(g), nm(n), parser(p), font(main_font) {
    }

    CircuitGrid::~CircuitGrid() {
    }

    void CircuitGrid::renderText(SDL_Renderer* ren, const std::string& text, int x, int y, bool isEditing) {
        if (!font || text.empty()) {
            return;
        }
        SDL_Surface* textSurface = TTF_RenderText_Blended(font, text.c_str(), 0, textColor);
        if (textSurface == nullptr) {
            std::cerr << "Unable to render text surface! SDL_Error: " << SDL_GetError() << "\n";
            return;
        }

        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(ren, textSurface);
        if (textTexture == nullptr) {
            std::cerr << "Unable to create texture from rendered text! SDL_Error: " << SDL_GetError() << "\n";
            SDL_DestroySurface(textSurface);
            return;
        }

        SDL_FRect renderQuad = { (float)x, (float)y, (float)textSurface->w, (float)textSurface->h };

        if (isEditing) {
            SDL_FRect bgRect = { renderQuad.x - 2, renderQuad.y - 2, renderQuad.w + 4, renderQuad.h + 4 };
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderFillRect(ren, &bgRect);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderRect(ren, &bgRect);
        }

        SDL_RenderTexture(ren, textTexture, nullptr, &renderQuad);

        SDL_DestroyTexture(textTexture);
        SDL_DestroySurface(textSurface);
    }


    void CircuitGrid::startEditName() {
    if (!selectedIndex) return;
    editing = true; editField = EditField::Name;
    inputBuffer = staged[*selectedIndex].name;
    SDL_StartTextInput(window);
    std::cout << "[Edit] Rename: type new name, Enter to confirm, Esc to cancel\n";
}

    void CircuitGrid::startEditValue() {
        if (!selectedIndex) return;
        auto& pe = staged[*selectedIndex];
        if (pe.kind == "Wire" || pe.kind == "Ground") return;
        editing = true;
        editField = EditField::Value;
        inputBuffer = pe.valueStr;
        SDL_StartTextInput(window);
        std::cout << "[Edit] New value: Enter to confirm, Esc to cancel\n";
    }

    void CircuitGrid::commitEdit() {
        if (!selectedIndex) { cancelEdit(); return; }
        auto& pe = staged[*selectedIndex];
        if (editField == EditField::Name) {
            pe.name = inputBuffer;
        } else if (editField == EditField::Value) {
            auto v = CommandParser::parseValueWithPrefix(inputBuffer);
            if (v.has_value()) {
                pe.value = *v;
                pe.valueStr = inputBuffer;
                std::cout << "[Edit] Value set to: " << pe.valueStr << " -> " << pe.value << "\n";
            } else {
                std::cout << "[Edit] Invalid value: " << inputBuffer << "\n";
            }
        }
        cancelEdit();
    }

void CircuitGrid::cancelEdit() {
    editing = false; editField = EditField::None; inputBuffer.clear();
    SDL_StopTextInput(window);
}


    void CircuitGrid::handleEvent(const SDL_Event& e) {
        if (editing) {
            if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_RETURN) commitEdit();
                else if (e.key.key == SDLK_ESCAPE) cancelEdit();
                else if (e.key.key == SDLK_BACKSPACE && !inputBuffer.empty()) inputBuffer.pop_back();
            } else if (e.type == SDL_EVENT_TEXT_INPUT) {
                inputBuffer += e.text.text;
            }
            return;
        }

        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (selectedIndex.has_value()) {
                if (e.key.key == SDLK_V) { startEditValue(); return; }
                if (e.key.key == SDLK_DELETE || e.key.key == SDLK_BACKSPACE) {
                    staged.erase(staged.begin() + *selectedIndex);
                    selectedIndex.reset();
                    return;
                }
            }
            switch (e.key.key) {
                case SDLK_R: currentTool = ToolKind::Resistor; std::cout << "Tool: Resistor\n"; break;
                case SDLK_C: currentTool = ToolKind::Capacitor; std::cout << "Tool: Capacitor\n"; break;
                case SDLK_L: currentTool = ToolKind::Inductor; std::cout << "Tool: Inductor\n"; break;
                case SDLK_V: currentTool = ToolKind::VoltageSource; std::cout << "Tool: VSource\n"; break;
                case SDLK_I: currentTool = ToolKind::CurrentSource; std::cout << "Tool: ISource\n"; break;
                case SDLK_D: currentTool = ToolKind::Diode; std::cout << "Tool: Diode\n"; break;
                case SDLK_W: currentTool = ToolKind::Wire; std::cout << "Tool: Wire\n"; break;
                case SDLK_G: currentTool = ToolKind::Ground; std::cout << "Tool: Ground\n"; break;
                case SDLK_P: currentTool = ToolKind::VoltageProbe; std::cout << "Tool: Voltage Probe\n"; break;
                case SDLK_O: currentTool = ToolKind::CurrentProbe; std::cout << "Tool: Current Probe\n"; break;
                default: break;
            }
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            float mx = e.button.x;
            float my = e.button.y;
            uint64_t now = SDL_GetTicks();
            bool isDoubleClick = (now - lastClickTicks < 400) && (std::abs(mx - lastClickPos.x) < 5 && std::abs(my - lastClickPos.y) < 5);
            lastClickTicks = now;
            lastClickPos = {mx, my};

            if (currentTool == ToolKind::VoltageProbe || currentTool == ToolKind::CurrentProbe) {
                int gx = mx / cellSize;
                int gy = my / cellSize;
                std::string command;

                if (currentTool == ToolKind::VoltageProbe) {
                    std::string nodeName = "N" + std::to_string(gx) + "_" + std::to_string(gy);
                    command = "plot v " + nodeName;
                } else { // CurrentProbe
                    auto distToSeg = [](float x, float y, float x1, float y1, float x2, float y2){
                        const float A = x - x1, B = y - y1;
                        const float C = x2 - x1, D = y2 - y1;
                        float dot = A*C + B*D;
                        float len2 = C*C + D*D + 1e-9f;
                        float t = std::max(0.f, std::min(1.f, dot / len2));
                        float px = x1 + t*C, py = y1 + t*D;
                        float dx = x - px, dy = y - py;
                        return std::sqrt(dx*dx + dy*dy);
                    };

                    int best = -1; float bestd = 1e9f;
                    for (int i = 0; i < (int)staged.size(); ++i) {
                        auto& pe = staged[i];
                        float x1 = pe.gx1 * cellSize + cellSize/2.0f;
                        float y1 = pe.gy1 * cellSize + cellSize/2.0f;
                        float x2 = pe.gx2 * cellSize + cellSize/2.0f;
                        float y2 = pe.gy2 * cellSize + cellSize/2.0f;
                        float d = distToSeg(mx, my, x1, y1, x2, y2);
                        if (d < bestd) { bestd = d; best = i; }
                    }

                    if (best != -1 && bestd <= 12.0f) {
                        command = "plot i " + staged[best].name;
                    }
                }

                if (!command.empty()) {
                    std::cout << "Probe executing: " << command << std::endl;
                    commitToModel();
                    parser->parseCommand(command);
                }
                return;
            }

            auto distToSeg = [](float x, float y, float x1, float y1, float x2, float y2){
                const float A = x - x1, B = y - y1;
                const float C = x2 - x1, D = y2 - y1;
                float dot = A*C + B*D;
                float len2 = C*C + D*D + 1e-9f;
                float t = std::max(0.f, std::min(1.f, dot / len2));
                float px = x1 + t*C, py = y1 + t*D;
                float dx = x - px, dy = y - py;
                return std::sqrt(dx*dx + dy*dy);
            };

            int best = -1; float bestd = 1e9f;
            for (int i = 0; i < (int)staged.size(); ++i) {
                auto& pe = staged[i];
                float x1 = pe.gx1 * cellSize + cellSize/2.0f;
                float y1 = pe.gy1 * cellSize + cellSize/2.0f;
                float x2 = pe.gx2 * cellSize + cellSize/2.0f;
                float y2 = pe.gy2 * cellSize + cellSize/2.0f;
                float d = distToSeg(mx, my, x1, y1, x2, y2);
                if (d < bestd) { bestd = d; best = i; }
            }

            if (best != -1 && bestd <= 12.0f) {
                selectedIndex = best;
                if (isDoubleClick) {
                    startEditValue();
                    return;
                }
            } else {
                selectedIndex.reset();
            }

            if (e.button.button == SDL_BUTTON_LEFT && !isDoubleClick) {
                int gx = mx / cellSize;
                int gy = my / cellSize;
                if (!pendingFirstNode.has_value()) {
                    pendingFirstNode = std::make_pair(gx, gy);
                    if (currentTool == ToolKind::Ground) {
                        PlacedElement pe;
                        pe.kind = "Ground";
                        pe.name = "GND";
                        pe.n1 = "N" + std::to_string(gx) + "_" + std::to_string(gy);
                        pe.n2 = pe.n1;
                        pe.gx1 = gx; pe.gy1 = gy; pe.gx2 = gx; pe.gy2 = gy;
                        staged.push_back(pe);
                        pendingFirstNode.reset();
                    }
                } else {
                    int startX = pendingFirstNode->first;
                    int startY = pendingFirstNode->second;
                    int snapX = gx, snapY = gy;
                    if (abs(gx - startX) > abs(gy - startY)) { snapY = startY; } else { snapX = startX; }

                    if (startX == snapX && startY == snapY) {
                        pendingFirstNode.reset();
                        return;
                    }

                    PlacedElement pe;
                    pe.n1 = "N" + std::to_string(startX) + "_" + std::to_string(startY);
                    pe.n2 = "N" + std::to_string(snapX) + "_" + std::to_string(snapY);
                    pe.gx1 = startX; pe.gy1 = startY; pe.gx2 = snapX; pe.gy2 = snapY;

                    switch (currentTool) {
                        case ToolKind::Resistor: pe.kind = "Resistor"; pe.valueStr = "1k"; break;
                        case ToolKind::Capacitor: pe.kind = "Capacitor"; pe.valueStr = "1u"; break;
                        case ToolKind::Inductor: pe.kind = "Inductor"; pe.valueStr = "1m"; break;
                        case ToolKind::VoltageSource: pe.kind = "VoltageSource"; pe.valueStr = "5"; break;
                        case ToolKind::CurrentSource: pe.kind = "CurrentSource"; pe.valueStr = "1"; break;
                        case ToolKind::Diode: pe.kind = "Diode"; pe.valueStr = "D"; break;
                        case ToolKind::Wire: pe.kind = "Wire"; pe.valueStr = ""; break;

                        default: pendingFirstNode.reset(); return;
                    }
                    pe.value = CommandParser::parseValueWithPrefix(pe.valueStr).value_or(0.0);
                    kindCounters[currentTool]++;
                    pe.name = std::string(1, pe.kind[0]) + std::to_string(kindCounters[currentTool]);

                    staged.push_back(pe);
                    pendingFirstNode.reset();
                }
            } else if (e.button.button == SDL_BUTTON_RIGHT) {
                pendingFirstNode.reset();
            }
        }
    }

    void CircuitGrid::render(SDL_Renderer* ren) {
    // --- گرید ---
    SDL_SetRenderDrawColor(ren, 220, 220, 220, 255);
    for (int x = 0; x < 800; x += cellSize) SDL_RenderLine(ren, x, 0, x, 600);
    for (int y = 0; y < 600; y += cellSize) SDL_RenderLine(ren, 0, y, 800, y);



    // --- عناصر ---
    for (auto& pe : staged) {
        int x1 = pe.gx1 * cellSize + cellSize / 2;
        int y1 = pe.gy1 * cellSize + cellSize / 2;
        int x2 = pe.gx2 * cellSize + cellSize / 2;
        int y2 = pe.gy2 * cellSize + cellSize / 2;


        int midx = (x1 + x2) / 2;
        int midy = (y1 + y2) / 2;


        // بعد از رسم هر pe:
        bool isSel = (selectedIndex.has_value() && *selectedIndex == (&pe - &staged[0]));
        if (isSel) {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 255);
            // نقطه‌های برجسته روی دو سر
            for (int r = -2; r <= 2; ++r) {
                SDL_RenderLine(ren, x1-2, y1+r, x1+2, y1+r);
                SDL_RenderLine(ren, x2-2, y2+r, x2+2, y2+r);
            }
            // قاب کوچک در مرکز
            SDL_RenderLine(ren, midx-8, midy-8, midx+8, midy-8);
            SDL_RenderLine(ren, midx+8, midy-8, midx+8, midy+8);
            SDL_RenderLine(ren, midx+8, midy+8, midx-8, midy+8);
            SDL_RenderLine(ren, midx-8, midy+8, midx-8, midy-8);
        }


        if (pe.kind == "Resistor") {
            SDL_SetRenderDrawColor(ren, 200, 0, 0, 255);
            // خط شکسته (Z شکل)
            SDL_RenderLine(ren, x1, y1, midx - 15, midy - 10);
            SDL_RenderLine(ren, midx - 15, midy - 10, midx, midy + 10);
            SDL_RenderLine(ren, midx, midy + 10, midx + 15, midy - 10);
            SDL_RenderLine(ren, midx + 15, midy - 10, x2, y2);

        } else if (pe.kind == "Capacitor") {
            SDL_SetRenderDrawColor(ren, 0, 0, 200, 255);
            // دو خط موازی عمود
            SDL_RenderLine(ren, midx - 5, midy - 15, midx - 5, midy + 15);
            SDL_RenderLine(ren, midx + 5, midy - 15, midx + 5, midy + 15);
            // اتصال به نودها
            SDL_RenderLine(ren, x1, y1, midx - 5, midy);
            SDL_RenderLine(ren, midx + 5, midy, x2, y2);

        } else if (pe.kind == "Inductor") {
            SDL_SetRenderDrawColor(ren, 0, 150, 0, 255);
            // سه نیم‌دایره ساده
            for (int i = -1; i <= 1; i++) {
                int cx = midx + i * 10;
                for (int a = 0; a < 180; a += 10) {
                    int px = cx + int(8 * cos(a * M_PI / 180.0));
                    int py = midy + int(8 * sin(a * M_PI / 180.0));
                    SDL_RenderPoint(ren, px, py);
                }
            }
            // خط به نودها
            SDL_RenderLine(ren, x1, y1, midx - 20, midy);
            SDL_RenderLine(ren, midx + 20, midy, x2, y2);

        } else if (pe.kind == "VoltageSource") {
            SDL_SetRenderDrawColor(ren, 255, 165, 0, 255);
            // دایره
            for (int a = 0; a < 360; a += 5) {
                int px = midx + int(15 * cos(a * M_PI / 180.0));
                int py = midy + int(15 * sin(a * M_PI / 180.0));
                SDL_RenderPoint(ren, px, py);
            }
            // خط به نودها
            SDL_RenderLine(ren, x1, y1, midx - 15, midy);
            SDL_RenderLine(ren, midx + 15, midy, x2, y2);

        } else if (pe.kind == "CurrentSource") {
            SDL_SetRenderDrawColor(ren, 128, 0, 128, 255);
            // دایره
            for (int a = 0; a < 360; a += 5) {
                int px = midx + int(15 * cos(a * M_PI / 180.0));
                int py = midy + int(15 * sin(a * M_PI / 180.0));
                SDL_RenderPoint(ren, px, py);
            }
            // فلش داخل دایره
            SDL_RenderLine(ren, midx, midy - 8, midx, midy + 8);
            SDL_RenderLine(ren, midx, midy + 8, midx - 5, midy + 3);
            SDL_RenderLine(ren, midx, midy + 8, midx + 5, midy + 3);
            // خط به نودها
            SDL_RenderLine(ren, x1, y1, midx - 15, midy);
            SDL_RenderLine(ren, midx + 15, midy, x2, y2);

        } else if (pe.kind == "Diode") {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            // مثلث
            SDL_RenderLine(ren, midx - 10, midy - 10, midx - 10, midy + 10);
            SDL_RenderLine(ren, midx - 10, midy - 10, midx + 5, midy);
            SDL_RenderLine(ren, midx - 10, midy + 10, midx + 5, midy);
            // خط عمود
            SDL_RenderLine(ren, midx + 5, midy - 10, midx + 5, midy + 10);
            // خط به نودها
            SDL_RenderLine(ren, x1, y1, midx - 10, midy);
            SDL_RenderLine(ren, midx + 5, midy, x2, y2);

        }else if (pe.kind == "Ground") {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderLine(ren, midx - 10, midy, midx + 10, midy);
            SDL_RenderLine(ren, midx - 6, midy + 5, midx + 6, midy + 5);
            SDL_RenderLine(ren, midx - 3, midy + 10, midx + 3, midy + 10);
        }




        else if (pe.kind == "Wire") {
            SDL_SetRenderDrawColor(ren, 100, 100, 100, 255);
            SDL_RenderLine(ren, x1, y1, x2, y2);
        }
    }
        if (editing && selectedIndex.has_value()) {
            // پیدا کردن قطعه انتخاب شده
            const auto& pe = staged.at(*selectedIndex);

            // محاسبه یک موقعیت مناسب برای کادر ویرایش (مثلاً زیر قطعه)
            int mid_x = (pe.gx1 * cellSize + pe.gx2 * cellSize) / 2 + cellSize / 2;
            int mid_y = (pe.gy1 * cellSize + pe.gy2 * cellSize) / 2 + cellSize / 2;
            SDL_FRect editRect = {
                (float)mid_x - 50,  // x
                (float)mid_y + 20,  // y
                100.0f,             // width
                25.0f               // height
            };

            // رسم پس‌زمینه سفید برای کادر
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderFillRect(ren, &editRect);

            // رسم حاشیه مشکی برای کادر
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderRect(ren, &editRect);

            // رسم متنی که کاربر تایپ کرده است (از inputBuffer)
            // با استفاده از همان تابع renderText که قبلاً نوشته‌ایم
            if (!inputBuffer.empty()) {
                renderText(ren, inputBuffer, (int)editRect.x + 5, (int)editRect.y + 5);
            }

            // می‌توانید یک مکان‌نمای چشمک‌زن هم اضافه کنید
            // (این بخش اختیاری است)
            Uint32 ticks = SDL_GetTicks();
            if (ticks % 1000 < 500) { // هر نیم ثانیه چشمک بزند
                SDL_Surface* tempSurface = TTF_RenderText_Blended(font, inputBuffer.c_str(), 0, textColor);
                if (tempSurface) {
                    int text_w = tempSurface->w;
                    SDL_DestroySurface(tempSurface);
                    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
                    SDL_RenderLine(ren, (int)editRect.x + 5 + text_w, (int)editRect.y + 5, (int)editRect.x + 5 + text_w, (int)editRect.y + 20);
                }
            }
        }

}


    void CircuitGrid::commitToModel() {
        graph->clear();
        nm->rebuildLabelTable();

        for (auto& pe : staged) {
            std::string cmd;
            if (pe.kind == "Wire") {
                cmd = "connect " + pe.n1 + " " + pe.n2;
            } else if (pe.kind == "Ground") {
                cmd = "add GND " + pe.n1;
            } else {
                std::string n1 = pe.n1;
                std::string n2 = pe.n2;
                if (pe.kind != "Wire" && pe.kind != "Ground" && n1 == n2)
                    n2 = std::to_string(nm->newNodeId());
                cmd = "add " + pe.name + " " + n1 + " " + n2 + " " + pe.valueStr;
            }
            parser->parseCommand(cmd);
        }
        std::cout << "Circuit committed to model. " << staged.size() << " elements processed.\n";
    }

} // namespace View
