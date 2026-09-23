#include "CircuitGrid.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <numbers>
#include <set>

namespace View {

    CircuitGrid::CircuitGrid(SDL_Window* win, Graph* g, NodeManager* n, CommandParser* p, TTF_Font* main_font)
            : window(win), graph(g), nm(n), parser(p), font(main_font) {}

    std::string CircuitGrid::nodeLabel(int gx, int gy) {
        return "N" + std::to_string(gx) + "_" + std::to_string(gy);
    }

    const char* CircuitGrid::toolName(ToolKind t) {
        switch (t) {
            case ToolKind::Wire: return "Wire";
            case ToolKind::Resistor: return "Resistor";
            case ToolKind::Capacitor: return "Capacitor";
            case ToolKind::Inductor: return "Inductor";
            case ToolKind::VoltageSource: return "Voltage source";
            case ToolKind::CurrentSource: return "Current source";
            case ToolKind::Diode: return "Diode";
            case ToolKind::Ground: return "Ground";
            case ToolKind::VoltageProbe: return "Voltage probe";
            case ToolKind::CurrentProbe: return "Current probe";
        }
        return "";
    }

    // R1, R2, ... skipping names that are already on the grid
    std::string CircuitGrid::nextName(const std::string& prefix) const {
        for (int i = 1;; ++i) {
            std::string cand = prefix + std::to_string(i);
            bool used = std::any_of(staged.begin(), staged.end(),
                                    [&](const PlacedElement& pe) { return pe.name == cand; });
            if (!used) return cand;
        }
    }

    int CircuitGrid::gridBottom() const {
        int w = 0, h = 0;
        SDL_GetWindowSize(window, &w, &h);
        return h - statusBarH;
    }

    void CircuitGrid::renderText(SDL_Renderer* ren, const std::string& text, float x, float y, bool boxed) {
        if (!font || text.empty()) return;
        SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), 0, textColor);
        if (!surf) return;

        SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
        if (tex) {
            SDL_FRect quad = { x, y, (float)surf->w, (float)surf->h };
            if (boxed) {
                SDL_FRect bg = { quad.x - 2, quad.y - 2, quad.w + 4, quad.h + 4 };
                SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                SDL_RenderFillRect(ren, &bg);
                SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
                SDL_RenderRect(ren, &bg);
            }
            SDL_RenderTexture(ren, tex, nullptr, &quad);
            SDL_DestroyTexture(tex);
        }
        SDL_DestroySurface(surf);
    }

    // only the middle part of an element counts, so clicking its end point still starts a new wire there
    std::optional<size_t> CircuitGrid::hitTest(float mx, float my) const {
        std::optional<size_t> best;
        float bestd = 10.0f;
        for (size_t i = 0; i < staged.size(); ++i) {
            const auto& pe = staged[i];
            float x1 = pe.gx1 * cellSize + cellSize / 2.0f;
            float y1 = pe.gy1 * cellSize + cellSize / 2.0f;
            float x2 = pe.gx2 * cellSize + cellSize / 2.0f;
            float y2 = pe.gy2 * cellSize + cellSize / 2.0f;

            float d;
            if (pe.kind == "Ground") {
                d = std::hypot(mx - x1, my - (y1 + 18));
            } else {
                float cx = x2 - x1, cy = y2 - y1;
                float t = ((mx - x1) * cx + (my - y1) * cy) / (cx * cx + cy * cy + 1e-9f);
                if (t < 0.15f || t > 0.85f) continue;
                d = std::hypot(mx - (x1 + t * cx), my - (y1 + t * cy));
            }
            if (d < bestd) { bestd = d; best = i; }
        }
        return best;
    }

    void CircuitGrid::startEditName() {
        if (!selectedIndex) return;
        auto& pe = staged[*selectedIndex];
        if (pe.kind == "Ground") return;
        editing = true;
        editField = EditField::Name;
        inputBuffer = pe.name;
        SDL_StartTextInput(window);
    }

    void CircuitGrid::startEditValue() {
        if (!selectedIndex) return;
        auto& pe = staged[*selectedIndex];
        if (pe.kind == "Wire" || pe.kind == "Ground") return;
        editing = true;
        editField = EditField::Value;
        inputBuffer = pe.valueStr;
        SDL_StartTextInput(window);
    }

    void CircuitGrid::commitEdit() {
        if (!selectedIndex || *selectedIndex >= staged.size()) { cancelEdit(); return; }
        auto& pe = staged[*selectedIndex];

        if (editField == EditField::Name) {
            bool taken = std::any_of(staged.begin(), staged.end(),
                                     [&](const PlacedElement& o) { return &o != &pe && o.name == inputBuffer; });
            // the first letter decides the element type, so it can't change
            if (inputBuffer.empty() || taken || std::toupper((unsigned char)inputBuffer[0]) != std::toupper((unsigned char)pe.name[0])) {
                std::cout << "[Edit] Name must be unique and start with '" << pe.name[0] << "'\n";
            } else {
                pe.name = inputBuffer;
                dirty = true;
            }
        } else if (editField == EditField::Value) {
            bool ok = (pe.kind == "Diode") ? (inputBuffer == "D" || inputBuffer == "Z")
                                           : CommandParser::parseValueWithPrefix(inputBuffer).has_value();
            if (ok) {
                pe.valueStr = inputBuffer;
                dirty = true;
                std::cout << "[Edit] " << pe.name << " = " << pe.valueStr << "\n";
            } else {
                std::cout << "[Edit] Invalid value: " << inputBuffer << "\n";
            }
        }
        cancelEdit();
    }

    void CircuitGrid::cancelEdit() {
        editing = false;
        editField = EditField::None;
        inputBuffer.clear();
        SDL_StopTextInput(window);
    }

    void CircuitGrid::placeElement(int gx, int gy) {
        if (currentTool == ToolKind::Ground) {
            PlacedElement pe;
            pe.kind = "Ground";
            pe.name = "GND";
            pe.n1 = pe.n2 = nodeLabel(gx, gy);
            pe.gx1 = pe.gx2 = gx;
            pe.gy1 = pe.gy2 = gy;
            staged.push_back(pe);
            dirty = true;
            return;
        }

        if (!pendingFirstNode) {
            pendingFirstNode = std::make_pair(gx, gy);
            return;
        }

        // second click: snap to a straight horizontal or vertical line
        int startX = pendingFirstNode->first;
        int startY = pendingFirstNode->second;
        int endX = gx, endY = gy;
        if (std::abs(gx - startX) > std::abs(gy - startY)) endY = startY;
        else endX = startX;
        pendingFirstNode.reset();
        if (startX == endX && startY == endY) return;

        PlacedElement pe;
        pe.n1 = nodeLabel(startX, startY);
        pe.n2 = nodeLabel(endX, endY);
        pe.gx1 = startX; pe.gy1 = startY;
        pe.gx2 = endX;   pe.gy2 = endY;

        std::string prefix;
        switch (currentTool) {
            case ToolKind::Resistor:      pe.kind = "Resistor";      prefix = "R"; pe.valueStr = "1k"; break;
            case ToolKind::Capacitor:     pe.kind = "Capacitor";     prefix = "C"; pe.valueStr = "1u"; break;
            case ToolKind::Inductor:      pe.kind = "Inductor";      prefix = "L"; pe.valueStr = "1m"; break;
            case ToolKind::VoltageSource: pe.kind = "VoltageSource"; prefix = "V"; pe.valueStr = "5";  break;
            case ToolKind::CurrentSource: pe.kind = "CurrentSource"; prefix = "I"; pe.valueStr = "1m"; break;
            case ToolKind::Diode:         pe.kind = "Diode";         prefix = "D"; pe.valueStr = "D";  break;
            case ToolKind::Wire:          pe.kind = "Wire";          prefix = "W"; break;
            default: return;
        }
        pe.name = nextName(prefix);
        staged.push_back(pe);
        dirty = true;
    }

    void CircuitGrid::probe(float mx, float my) {
        std::string command;
        if (currentTool == ToolKind::VoltageProbe) {
            int gx = (int)mx / cellSize, gy = (int)my / cellSize;
            commitToModel();
            std::string node = nodeLabel(gx, gy);
            if (!nm->findId(node)) {
                std::cout << "Probe: nothing is connected at this point\n";
                return;
            }
            command = "plot v " + node;
        } else {
            auto hit = hitTest(mx, my);
            if (!hit || staged[*hit].kind == "Wire" || staged[*hit].kind == "Ground") {
                std::cout << "Probe: click on the middle of an element\n";
                return;
            }
            commitToModel();
            command = "plot i " + staged[*hit].name;
        }
        std::cout << "> " << command << std::endl;
        parser->parseCommand(command);
    }

    void CircuitGrid::handleEvent(const SDL_Event& e) {
        if (editing) {
            if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER) commitEdit();
                else if (e.key.key == SDLK_ESCAPE) cancelEdit();
                else if (e.key.key == SDLK_BACKSPACE && !inputBuffer.empty()) inputBuffer.pop_back();
            } else if (e.type == SDL_EVENT_TEXT_INPUT) {
                inputBuffer += e.text.text;
            }
            return;
        }

        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (selectedIndex) {
                switch (e.key.key) {
                    case SDLK_DELETE:
                    case SDLK_BACKSPACE:
                        staged.erase(staged.begin() + *selectedIndex);
                        selectedIndex.reset();
                        dirty = true;
                        return;
                    case SDLK_RETURN:
                    case SDLK_E: startEditValue(); return;
                    case SDLK_N: startEditName(); return;
                    default: break;
                }
            }
            switch (e.key.key) {
                case SDLK_R: currentTool = ToolKind::Resistor; break;
                case SDLK_C: currentTool = ToolKind::Capacitor; break;
                case SDLK_L: currentTool = ToolKind::Inductor; break;
                case SDLK_V: currentTool = ToolKind::VoltageSource; break;
                case SDLK_I: currentTool = ToolKind::CurrentSource; break;
                case SDLK_D: currentTool = ToolKind::Diode; break;
                case SDLK_W: currentTool = ToolKind::Wire; break;
                case SDLK_G: currentTool = ToolKind::Ground; break;
                case SDLK_P: currentTool = ToolKind::VoltageProbe; break;
                case SDLK_O: currentTool = ToolKind::CurrentProbe; break;
                case SDLK_ESCAPE:
                    pendingFirstNode.reset();
                    selectedIndex.reset();
                    return;
                default: return;
            }
            pendingFirstNode.reset();
            return;
        }

        if (e.type != SDL_EVENT_MOUSE_BUTTON_DOWN) return;

        float mx = e.button.x, my = e.button.y;
        if (my >= gridBottom()) return;   // status bar

        if (e.button.button == SDL_BUTTON_RIGHT) {
            pendingFirstNode.reset();
            selectedIndex.reset();
            return;
        }
        if (e.button.button != SDL_BUTTON_LEFT) return;

        uint64_t now = SDL_GetTicks();
        bool isDoubleClick = (now - lastClickTicks < 400) &&
                             std::abs(mx - lastClickPos.x) < 5 && std::abs(my - lastClickPos.y) < 5;
        lastClickTicks = now;
        lastClickPos = {mx, my};

        if (currentTool == ToolKind::VoltageProbe || currentTool == ToolKind::CurrentProbe) {
            probe(mx, my);
            return;
        }

        // clicking an element selects it (unless we're in the middle of placing something)
        if (!pendingFirstNode) {
            auto hit = hitTest(mx, my);
            if (hit) {
                selectedIndex = hit;
                if (isDoubleClick) startEditValue();
                return;
            }
        }
        selectedIndex.reset();
        placeElement((int)mx / cellSize, (int)my / cellSize);
    }

    void CircuitGrid::drawElement(SDL_Renderer* ren, const PlacedElement& pe, bool selected) {
        const float x1 = pe.gx1 * cellSize + cellSize / 2.0f;
        const float y1 = pe.gy1 * cellSize + cellSize / 2.0f;
        const float x2 = pe.gx2 * cellSize + cellSize / 2.0f;
        const float y2 = pe.gy2 * cellSize + cellSize / 2.0f;
        const float midx = (x1 + x2) / 2, midy = (y1 + y2) / 2;
        const bool vertical = (pe.gx1 == pe.gx2);

        if (pe.kind == "Ground") {
            if (selected) {
                SDL_SetRenderDrawColor(ren, 255, 0, 0, 90);
                SDL_FRect r{ x1 - 14, y1, 28, 30 };
                SDL_RenderFillRect(ren, &r);
            }
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderLine(ren, x1, y1, x1, y1 + 14);
            SDL_RenderLine(ren, x1 - 10, y1 + 14, x1 + 10, y1 + 14);
            SDL_RenderLine(ren, x1 - 6, y1 + 19, x1 + 6, y1 + 19);
            SDL_RenderLine(ren, x1 - 2, y1 + 24, x1 + 2, y1 + 24);
            return;
        }

        // unit vector along the element and one across it; symbols are drawn in these
        // local coordinates so the same code works for horizontal and vertical parts
        float len = std::hypot(x2 - x1, y2 - y1);
        float ux = (x2 - x1) / len, uy = (y2 - y1) / len;
        float nx = -uy, ny = ux;
        auto P = [&](float a, float c) { return SDL_FPoint{ midx + a * ux + c * nx, midy + a * uy + c * ny }; };
        auto line = [&](float a1, float c1, float a2, float c2) {
            SDL_FPoint p = P(a1, c1), q = P(a2, c2);
            SDL_RenderLine(ren, p.x, p.y, q.x, q.y);
        };
        auto leads = [&](float body) {
            SDL_FPoint p = P(-body, 0), q = P(body, 0);
            SDL_RenderLine(ren, x1, y1, p.x, p.y);
            SDL_RenderLine(ren, q.x, q.y, x2, y2);
        };
        auto circle = [&](float r) {
            std::vector<SDL_FPoint> pts;
            for (int a = 0; a <= 360; a += 10) {
                double t = a * std::numbers::pi / 180.0;
                pts.push_back({ midx + float(r * std::cos(t)), midy + float(r * std::sin(t)) });
            }
            SDL_RenderLines(ren, pts.data(), (int)pts.size());
        };

        if (selected) {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 70);
            SDL_FRect r = vertical ? SDL_FRect{ x1 - 16, std::min(y1, y2), 32, std::abs(y2 - y1) }
                                   : SDL_FRect{ std::min(x1, x2), y1 - 16, std::abs(x2 - x1), 32 };
            SDL_RenderFillRect(ren, &r);
        }

        if (pe.kind == "Wire") {
            SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
            SDL_RenderLine(ren, x1, y1, x2, y2);
        } else if (pe.kind == "Resistor") {
            SDL_SetRenderDrawColor(ren, 200, 0, 0, 255);
            leads(15);
            float c = 7;
            for (int i = 0; i < 6; ++i) {
                float a1 = -15 + i * 5, a2 = a1 + 5;
                float c1 = (i == 0) ? 0 : ((i % 2) ? c : -c);
                float c2 = (i == 5) ? 0 : ((i % 2) ? -c : c);
                line(a1, c1, a2, c2);
            }
        } else if (pe.kind == "Capacitor") {
            SDL_SetRenderDrawColor(ren, 0, 0, 200, 255);
            leads(4);
            line(-4, -12, -4, 12);
            line(4, -12, 4, 12);
        } else if (pe.kind == "Inductor") {
            SDL_SetRenderDrawColor(ren, 0, 140, 0, 255);
            leads(15);
            for (int k = 0; k < 3; ++k) {
                float ca = -10 + k * 10;   // center of each bump
                std::vector<SDL_FPoint> pts;
                for (int a = 180; a >= 0; a -= 15) {
                    double t = a * std::numbers::pi / 180.0;
                    pts.push_back(P(ca + 5 * float(std::cos(t)), -6 * float(std::sin(t))));
                }
                SDL_RenderLines(ren, pts.data(), (int)pts.size());
            }
        } else if (pe.kind == "VoltageSource") {
            SDL_SetRenderDrawColor(ren, 230, 130, 0, 255);
            leads(14);
            circle(14);
            // + on the first node side
            line(-10, -3, -4, -3);
            line(-7, -6, -7, 0);
            line(4, -3, 10, -3);
        } else if (pe.kind == "CurrentSource") {
            SDL_SetRenderDrawColor(ren, 128, 0, 128, 255);
            leads(14);
            circle(14);
            // arrow from n1 to n2
            line(-8, 0, 8, 0);
            line(8, 0, 3, -4);
            line(8, 0, 3, 4);
        } else if (pe.kind == "Diode") {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            leads(8);
            line(-8, -8, -8, 8);
            line(-8, -8, 6, 0);
            line(-8, 8, 6, 0);
            line(6, -8, 6, 8);
        }

        // small dots on the terminals
        SDL_SetRenderDrawColor(ren, 40, 40, 40, 255);
        SDL_FRect d1{ x1 - 2, y1 - 2, 4, 4 }, d2{ x2 - 2, y2 - 2, 4, 4 };
        SDL_RenderFillRect(ren, &d1);
        SDL_RenderFillRect(ren, &d2);

        if (pe.kind == "Wire") return;
        if (editing && selected) return;   // the edit box is drawn instead
        if (vertical) {
            renderText(ren, pe.name, midx + 18, midy - 20);
            renderText(ren, pe.valueStr, midx + 18, midy);
        } else {
            renderText(ren, pe.name, midx - 12, midy - 36);
            renderText(ren, pe.valueStr, midx - 12, midy + 16);
        }
    }

    void CircuitGrid::drawStatusBar(SDL_Renderer* ren, int w, int h) {
        SDL_FRect bar{ 0, (float)(h - statusBarH), (float)w, (float)statusBarH };
        SDL_SetRenderDrawColor(ren, 225, 225, 225, 255);
        SDL_RenderFillRect(ren, &bar);
        SDL_SetRenderDrawColor(ren, 150, 150, 150, 255);
        SDL_RenderLine(ren, 0, bar.y, (float)w, bar.y);

        std::string msg = std::string("Tool: ") + toolName(currentTool) +
                          "   |  W R C L V I D G   P/O probes  |  dbl-click/E value, N name, Del delete  |  F2 plot";
        renderText(ren, msg, 8, bar.y + 5);
    }

    void CircuitGrid::render(SDL_Renderer* ren) {
        int w = 0, h = 0;
        SDL_GetWindowSize(window, &w, &h);
        int bottom = h - statusBarH;

        // grid points are the cell centers
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        for (int x = cellSize / 2; x < w; x += cellSize)
            for (int y = cellSize / 2; y < bottom; y += cellSize) {
                SDL_FRect dot{ (float)x - 1, (float)y - 1, 2, 2 };
                SDL_RenderFillRect(ren, &dot);
            }

        for (size_t i = 0; i < staged.size(); ++i) {
            drawElement(ren, staged[i], selectedIndex && *selectedIndex == i);
        }

        // show where the element will go while placing it
        if (pendingFirstNode) {
            float px = pendingFirstNode->first * cellSize + cellSize / 2.0f;
            float py = pendingFirstNode->second * cellSize + cellSize / 2.0f;
            float mx, my;
            SDL_GetMouseState(&mx, &my);
            int gx = (int)mx / cellSize, gy = (int)my / cellSize;
            if (std::abs(gx - pendingFirstNode->first) > std::abs(gy - pendingFirstNode->second)) gy = pendingFirstNode->second;
            else gx = pendingFirstNode->first;

            SDL_SetRenderDrawColor(ren, 255, 0, 0, 160);
            SDL_RenderLine(ren, px, py, gx * cellSize + cellSize / 2.0f, gy * cellSize + cellSize / 2.0f);
            SDL_FRect r{ px - 4, py - 4, 8, 8 };
            SDL_RenderFillRect(ren, &r);
        }

        if (editing && selectedIndex && *selectedIndex < staged.size()) {
            const auto& pe = staged[*selectedIndex];
            float mid_x = (pe.gx1 + pe.gx2) * cellSize / 2.0f + cellSize / 2.0f;
            float mid_y = (pe.gy1 + pe.gy2) * cellSize / 2.0f + cellSize / 2.0f;
            SDL_FRect box = { mid_x - 60, mid_y + 18, 120.0f, 26.0f };

            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderFillRect(ren, &box);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderRect(ren, &box);
            renderText(ren, (editField == EditField::Name ? "name: " : "value: ") + inputBuffer, box.x + 5, box.y + 4);
        }

        drawStatusBar(ren, w, h);
    }

    void CircuitGrid::commitToModel(bool force) {
        if (!dirty && !force) return;

        graph->clear();
        nm->reset();

        // every point where something ends (element pins, wire ends, ground)
        std::set<std::pair<int,int>> pins;
        for (const auto& pe : staged) {
            pins.insert({pe.gx1, pe.gy1});
            pins.insert({pe.gx2, pe.gy2});
        }

        bool hasGround = false;
        for (const auto& pe : staged) {
            std::string cmd;
            if (pe.kind == "Wire") {
                // a pin that sits on the middle of a wire is connected too (like LTspice),
                // two wires that only cross each other are not
                int dx = (pe.gx2 > pe.gx1) - (pe.gx2 < pe.gx1);
                int dy = (pe.gy2 > pe.gy1) - (pe.gy2 < pe.gy1);
                for (int x = pe.gx1 + dx, y = pe.gy1 + dy; x != pe.gx2 || y != pe.gy2; x += dx, y += dy) {
                    if (pins.count({x, y})) parser->parseCommand("connect " + pe.n1 + " " + nodeLabel(x, y));
                }
                cmd = "connect " + pe.n1 + " " + pe.n2;
            } else if (pe.kind == "Ground") {
                cmd = "add GND " + pe.n1;
                hasGround = true;
            } else {
                cmd = "add " + pe.name + " " + pe.n1 + " " + pe.n2 + " " + pe.valueStr;
            }
            parser->parseCommand(cmd);
        }
        dirty = false;

        std::cout << "Circuit updated from the grid (" << graph->getElements().size() << " elements)\n";
        if (!staged.empty() && !hasGround) std::cout << "Warning: no ground on the grid, press G and place one\n";
    }

} // namespace View
