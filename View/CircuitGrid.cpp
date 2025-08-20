#include "CircuitGrid.h"
#include <SDL3/SDL.h>
#include <cmath>
#include<bits/stdc++.h>
#include <iostream>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace View {

    CircuitGrid::CircuitGrid(Graph* g, NodeManager* nm) : graph(g), nm(nm) {}

    void CircuitGrid::handleEvent(const SDL_Event& e) {
        if (e.type == SDL_EVENT_KEY_DOWN) {
            SDL_Keycode key = e.key.key;
            switch (key) {
                case SDLK_R: currentKind = "Resistor"; break;
                case SDLK_C: currentKind = "Capacitor"; break;
                case SDLK_L: currentKind = "Inductor"; break;
                case SDLK_V: currentKind = "VoltageSource"; break;
                case SDLK_I: currentKind = "CurrentSource"; break;
                case SDLK_D: currentKind = "Diode"; break;
                case SDLK_W: currentKind = "Wire"; break;
                case SDLK_G: currentKind = "Ground"; break;

                default: break;
            }
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            float mx, my;
            SDL_GetMouseState(&mx, &my);
            int gx = (int)mx / cellSize;
            int gy = (int)my / cellSize;

            if (!pendingFirstNode.has_value()) {
                // اولین کلیک → ذخیره نود اول
                pendingFirstNode = std::make_pair(gx, gy);
            } else {
                // دومین کلیک → نود دوم و ساختن المان

                // اسنپ کردن به محور نزدیک‌تر
                int startX = pendingFirstNode->first;
                int startY = pendingFirstNode->second;

                int snapX = gx;
                int snapY = gy;

                int dx = abs(gx - startX);
                int dy = abs(gy - startY);

                if (dx > dy) {
                    // نزدیک‌تر به محور افقی
                    snapY = startY;
                } else {
                    // نزدیک‌تر به محور عمودی
                    snapX = startX;
                }

                PlacedElement pe;
                pe.kind = currentKind;
                pe.n1 = "N" + std::to_string(startX) + "_" + std::to_string(startY);
                pe.n2 = "N" + std::to_string(snapX) + "_" + std::to_string(snapY);

                pe.gx1 = startX;
                pe.gy1 = startY;
                pe.gx2 = snapX;
                pe.gy2 = snapY;

                staged.push_back(pe);

                pendingFirstNode.reset(); // آماده برای المان بعدی
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
}


    void CircuitGrid::commitToModel(Graph* g, NodeManager* nm) {
        for (auto& pe : staged) {
            if (pe.kind == "Wire") {
                nm->connect(pe.n1, pe.n2);
            }
            int n1 = nm->getOrCreateNodeId(pe.n1);
            int n2 = nm->getOrCreateNodeId(pe.n2);

            int id = ++kindCounters[pe.kind];
            std::string uniqueName = pe.kind.substr(0,1) + std::to_string(id);

            if (pe.kind == "Resistor") {
                g->addElement(new Resistor(pe.kind, n1, n2, 1000.0));
            } else if (pe.kind == "Capacitor") {
                g->addElement(new Capacitor(pe.kind, n1, n2, 1e-6));
            } else if (pe.kind == "Inductor") {
                g->addElement(new Inductor(pe.kind, n1, n2, 1e-3));
            } else if (pe.kind == "VoltageSource") {
                g->addElement(new VoltageSource(pe.kind, n1, n2, 5.0));
            } else if (pe.kind == "CurrentSource") {
                g->addElement(new CurrentSource(pe.kind, n1, n2, 1e-3));
            } else if (pe.kind == "Diode") {
                g->addElement(new Diode(pe.kind, n1, n2, "default"));
            // } else if (pe.kind == "Wire") {
            //     nm->connect(pe.n1, pe.n2);
            }else if (pe.kind == "Ground") {
                int nid = nm->getOrCreateNodeId(pe.n1);
                nm->assignNodeAsGND(pe.n1); // تابع جدید: نود رو Ground کن
            }

        }
    }

} // namespace View
