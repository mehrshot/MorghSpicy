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

    CircuitGrid::CircuitGrid(SDL_Window* win,Graph* g, NodeManager* nm) : window(win),graph(g), nm(nm) {
        if (TTF_Init() == -1) {
            std::cerr << "SDL_ttf could not initialize! SDL_Error: " << SDL_GetError() << "\n";
            return;
        }
        loadFont("arial.ttf", 14); // فرض بر این است که فایل arial.ttf در کنار اجرایی وجود دارد
    }
     // دیستراکتور
    CircuitGrid::~CircuitGrid() {
        if (font) {
            TTF_CloseFont(font);
            font = nullptr;
        }
        TTF_Quit();
    }

    // تابع بارگذاری فونت
    void CircuitGrid::loadFont(const std::string& fontPath, int size) {
        font = TTF_OpenFont(fontPath.c_str(), size);
        if (font == nullptr) {
            // --- تغییر در اینجا ---
            std::cerr << "Failed to load font: " << fontPath
                      << "! SDL_Error: " << SDL_GetError() << "\n";
        }
    }

    // تابع رندر متن
    void CircuitGrid::renderText(SDL_Renderer* ren, const std::string& text, int x, int y, bool isEditing) {
        if (!font || text.empty()) {
            return;
        }
        SDL_Color bgColor = {0, 0, 0, 0};
        SDL_Surface* textSurface = TTF_RenderText_Blended(font, text.c_str(), 0, textColor);
        if (textSurface == nullptr) {
            // --- تغییر در اینجا ---
            std::cerr << "Unable to render text surface! SDL_Error: " << SDL_GetError() << "\n";
            return;
        }

        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(ren, textSurface);
        if (textTexture == nullptr) {
            // --- تغییر در اینجا ---
            std::cerr << "Unable to create texture from rendered text! SDL_Error: " << SDL_GetError() << "\n";
            SDL_DestroySurface(textSurface);
            return;
        }

        SDL_FRect renderQuad = { (float)x, (float)y, (float)textSurface->w, (float)textSurface->h };

        // اگر در حال ویرایش است، یک پس‌زمینه سفید رسم کنید
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
    // Wire/Ground مقدار ندارند
    if (pe.kind == "Wire" || pe.kind == "Ground") return;
    editing = true; editField = EditField::Value;
    inputBuffer = pe.valueStr;
    SDL_StartTextInput(window);
    std::cout << "[Edit] New value (e.g., 10k, 1u, 5): Enter to confirm, Esc to cancel\n";
}

void CircuitGrid::commitEdit() {
    if (!selectedIndex) { cancelEdit(); return; }
    auto& pe = staged[*selectedIndex];
    if (editField == EditField::Name) {
        std::string s = trim(inputBuffer);
        if (!s.empty()) {
            pe.name = s;
            std::cout << "[Edit] Renamed to: " << pe.name << "\n";
        }
    } else if (editField == EditField::Value) {
        std::string s = trim(inputBuffer);
        if (pe.kind == "Diode") {
            // مدل دیود رشته‌ای است
            if (!s.empty()) pe.valueStr = s;
            std::cout << "[Edit] Diode model: " << pe.valueStr << "\n";
        } else {
            auto v = parseEng(s);
            if (v.has_value()) {
                pe.value = *v;
                pe.valueStr = s;
                std::cout << "[Edit] Value set: " << pe.valueStr << " -> " << pe.value << "\n";
            } else {
                std::cout << "[Edit] Invalid value: " << s << " (kept: " << pe.valueStr << ")\n";
            }
        }
    }
    cancelEdit();
}

void CircuitGrid::cancelEdit() {
    editing = false; editField = EditField::None; inputBuffer.clear();
    SDL_StopTextInput(window);
}

std::string CircuitGrid::trim(const std::string& s) {
    size_t i=0, j=s.size();
    while (i<j && std::isspace((unsigned char)s[i])) ++i;
    while (j>i && std::isspace((unsigned char)s[j-1])) --j;
    return s.substr(i, j-i);
}

std::optional<double> CircuitGrid::parseEng(const std::string& sraw) {
    if (sraw.empty()) return std::nullopt;
    std::string s; s.reserve(sraw.size());
    // normalize
    for (char c : sraw) {
        if (!std::isspace((unsigned char)c)) s.push_back(c);
    }
    // lowercase, but keep 'e' in sci
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });

    // handle 'meg' explicitly
    double mult = 1.0;
    auto ends_with = [&](const std::string& suf){
        return s.size() >= suf.size() && s.compare(s.size()-suf.size(), suf.size(), suf) == 0;
    };

    if (ends_with("meg")) { mult = 1e6; s.erase(s.size()-3); }
    else if (ends_with("g")) { mult = 1e9; s.pop_back(); }
    else if (ends_with("m")) { mult = 1e6; s.pop_back(); } // 'M' اشتباه کاربر؛ اینجا m=1e-3 در SI است
    else if (ends_with("k")) { mult = 1e3; s.pop_back(); }
    else if (ends_with("u") || ends_with("µ")) { mult = 1e-6; s.pop_back(); }
    else if (ends_with("n")) { mult = 1e-9; s.pop_back(); }
    else if (ends_with("p")) { mult = 1e-12; s.pop_back(); }
    else if (ends_with("f")) { mult = 1e-15; s.pop_back(); }
    // اگر هیچ پسوندی نبود، mult=1

    try {
        double base = std::stod(s); // اجازه‌ی 1e-3 هم می‌دهد
        return base * mult;
    } catch (...) { return std::nullopt; }
}


    void CircuitGrid::handleEvent(const SDL_Event& e) {
    // --- منطق حالت ویرایش ---
    // اگر در حال ویرایش هستیم، فقط رویدادهای مربوط به ویرایش را 처리 می‌کنیم
    if (editing) {
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_RETURN) {
                commitEdit();
            } else if (e.key.key == SDLK_ESCAPE) {
                cancelEdit();
            } else if (e.key.key == SDLK_BACKSPACE) {
                if (!inputBuffer.empty()) inputBuffer.pop_back();
            }
        } else if (e.type == SDL_EVENT_TEXT_INPUT) {
            if (e.text.text) inputBuffer += e.text.text;
        }
        return; // <-- مهم: از اجرای بقیه کد جلوگیری می‌کند
    }

    // --- منطق حالت عادی (وقتی در حال ویرایش نیستیم) ---
    if (e.type == SDL_EVENT_KEY_DOWN) {
        // کامیت کردن کل مدار با Enter
        if (e.key.key == SDLK_RETURN) {
            commitToModel(graph, nm);
            staged.clear();
            std::cout << "Committed staged elements to Graph/NodeManager\n";
            return;
        }

        // شروع ویرایش با N, V یا حذف با Delete
        if (selectedIndex.has_value()) {
            if (e.key.key == SDLK_N) { startEditName(); return; }
            if (e.key.key == SDLK_V) { startEditValue(); return; }
            if (e.key.key == SDLK_DELETE) {
                std::cout << "[Grid] Deleted: " << staged[*selectedIndex].name << "\n";
                staged.erase(staged.begin() + *selectedIndex);
                selectedIndex.reset();
                return;
            }
        }

        // تغییر نوع قطعه جاری
        switch (e.key.key) {
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

    // منطق کلیک‌های ماوس
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (e.button.button == SDL_BUTTON_RIGHT) {
            // ... (کد انتخاب قطعه با راست‌کلیک که قبلاً داشتید، اینجا بدون تغییر باقی می‌ماند)
            float mx, my; SDL_GetMouseState(&mx, &my);
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
                std::cout << "[Grid] Selected: " << staged[best].name
                          << " (" << staged[best].kind << "), value=" << staged[best].valueStr << "\n";
            } else {
                selectedIndex.reset();
            }
        } else if (e.button.button == SDL_BUTTON_LEFT) {
            // ... (کد قرار دادن قطعه با چپ‌کلیک که قبلاً داشتید، اینجا بدون تغییر باقی می‌ماند)
            float mx, my; SDL_GetMouseState(&mx, &my);
            int gx = mx / cellSize;
            int gy = my / cellSize;

            if (currentKind == "Ground") {
                PlacedElement pe;
                pe.kind = "Ground";
                pe.name = "GND"; // نام زمین همیشه ثابت است
                pe.n1 = "N" + std::to_string(gx) + "_" + std::to_string(gy);
                pe.n2 = pe.n1; // نود دوم هم همان نود اول است
                pe.gx1 = gx; pe.gy1 = gy;
                pe.gx2 = gx; pe.gy2 = gy; // مختصات دوم هم همان اول است
                staged.push_back(pe);
                pendingFirstNode.reset(); // اطمینان از اینکه منتظر کلیک دوم نمی‌مانیم
                return; // از اجرای بقیه کد جلوگیری می‌کنیم
            }


            if (!pendingFirstNode.has_value()) {
                pendingFirstNode = std::make_pair(gx, gy);
            } else {
                int startX = pendingFirstNode->first;
                int startY = pendingFirstNode->second;
                int snapX = gx, snapY = gy;
                if (abs(gx - startX) > abs(gy - startY)) { snapY = startY; } else { snapX = startX; }

                PlacedElement pe;
                pe.kind = currentKind;
                int id = ++kindCounters[currentKind];
                std::string prefix = std::string(1, std::toupper(currentKind[0]));
                pe.name = prefix + std::to_string(id);
                if (currentKind == "Resistor")      { pe.value = 1000.0;   pe.valueStr = "1k"; }
                else if (currentKind == "Capacitor"){ pe.value = 1e-6;     pe.valueStr = "1u"; }
                // ... بقیه مقادیر پیش‌فرض

                pe.n1 = "N" + std::to_string(startX) + "_" + std::to_string(startY);
                pe.n2 = "N" + std::to_string(snapX) + "_" + std::to_string(snapY);
                pe.gx1 = startX; pe.gy1 = startY; pe.gx2 = snapX; pe.gy2 = snapY;
                staged.push_back(pe);
                pendingFirstNode.reset();
            }
        }
    }
}

   void CircuitGrid::render(SDL_Renderer* ren) {
    // --- گرید ---
    SDL_SetRenderDrawColor(ren, 220, 220, 220, 255);
    for (int x = 0; x < 800; x += cellSize) SDL_RenderLine(ren, x, 0, x, 600);
    for (int y = 0; y < 600; y += cellSize) SDL_RenderLine(ren, 0, y, 800, y);

    // --- عناصر ---
    for (size_t i = 0; i < staged.size(); ++i) {
        auto& pe = staged[i];
        int x1 = pe.gx1 * cellSize + cellSize / 2;
        int y1 = pe.gy1 * cellSize + cellSize / 2;
        int x2 = pe.gx2 * cellSize + cellSize / 2;
        int y2 = pe.gy2 * cellSize + cellSize / 2;

        int midx = (x1 + x2) / 2;
        int midy = (y1 + y2) / 2;

        // --- اضافه شد: تشخیص جهت‌گیری قطعه ---
        bool isVertical = (pe.gx1 == pe.gx2);

        // --- هایلایت کردن قطعه انتخاب شده ---
        bool isSel = (selectedIndex.has_value() && *selectedIndex == i);
        if (isSel) {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 150); // رنگ قرمز نیمه‌شفاف
            // رسم یک مستطیل بزرگتر دور قطعه برای نمایش بهتر انتخاب
            if (isVertical) {
                SDL_FRect selRect = { (float)x1 - 10, (float)std::min(y1, y2), 20.0f, (float)abs(y2 - y1) };
                SDL_RenderFillRect(ren, &selRect);
            } else {
                SDL_FRect selRect = { (float)std::min(x1, x2), (float)y1 - 10, (float)abs(x2 - x1), 20.0f };
                SDL_RenderFillRect(ren, &selRect);
            }
        }

        // --- رسم هر قطعه با توجه به جهت‌گیری ---
        if (pe.kind == "Resistor") {
            SDL_SetRenderDrawColor(ren, 200, 0, 0, 255);
            if (isVertical) {
                // رسم عمودی مقاومت
                int len = 15; // طول هر بخش زیگزاگ
                SDL_RenderLine(ren, x1, y1, midx - 10, midy - len);
                SDL_RenderLine(ren, midx - 10, midy - len, midx + 10, midy);
                SDL_RenderLine(ren, midx + 10, midy, midx - 10, midy + len);
                SDL_RenderLine(ren, midx - 10, midy + len, x2, y2);
            } else {
                // رسم افقی (کد قبلی شما)
                int len = 15;
                SDL_RenderLine(ren, x1, y1, midx - len, midy - 10);
                SDL_RenderLine(ren, midx - len, midy - 10, midx, midy + 10);
                SDL_RenderLine(ren, midx, midy + 10, midx + len, midy - 10);
                SDL_RenderLine(ren, midx + len, midy - 10, x2, y2);
            }

        } else if (pe.kind == "Capacitor") {
            SDL_SetRenderDrawColor(ren, 0, 0, 200, 255);
            if (isVertical) {
                // رسم عمودی خازن (صفحات افقی)
                SDL_RenderLine(ren, midx - 15, midy - 5, midx + 15, midy - 5);
                SDL_RenderLine(ren, midx - 15, midy + 5, midx + 15, midy + 5);
                // اتصال به نودها
                SDL_RenderLine(ren, x1, y1, midx, midy - 5);
                SDL_RenderLine(ren, midx, midy + 5, x2, y2);
            } else {
                // رسم افقی (صفحات عمودی - کد قبلی شما)
                SDL_RenderLine(ren, midx - 5, midy - 15, midx - 5, midy + 15);
                SDL_RenderLine(ren, midx + 5, midy - 15, midx + 5, midy + 15);
                // اتصال به نودها
                SDL_RenderLine(ren, x1, y1, midx - 5, midy);
                SDL_RenderLine(ren, midx + 5, midy, x2, y2);
            }

        } else if (pe.kind == "Inductor") {
            SDL_SetRenderDrawColor(ren, 0, 150, 0, 255);
            int radius = 8;
            int offset = 10;
            // اتصال به نودها
            SDL_RenderLine(ren, x1, y1, isVertical ? midx : midx - (2*offset + radius), isVertical ? midy - (2*offset + radius) : midy);
            SDL_RenderLine(ren, isVertical ? midx : midx + (2*offset + radius), isVertical ? midy + (2*offset + radius) : midy, x2, y2);
            // سه نیم‌دایره
            for (int i = -1; i <= 1; i++) {
                int cx = isVertical ? midx : midx + i * offset;
                int cy = isVertical ? midy + i * offset : midy;
                for (int a = 0; a < 180; a += 10) {
                    int px = cx + int(radius * cos(a * M_PI / 180.0) * (isVertical ? 1 : 0) + radius * sin(a * M_PI / 180.0) * (isVertical ? 0 : 1));
                    int py = cy + int(radius * sin(a * M_PI / 180.0) * (isVertical ? 1 : 1) - radius * cos(a * M_PI / 180.0) * (isVertical ? 1 : 0));
                    if (isVertical) px = cx + int(radius * sin(a * M_PI / 180.0));
                    else py = cy - int(radius * cos(a * M_PI / 180.0));
                    SDL_RenderPoint(ren, px, py);
                }
            }

        } else if (pe.kind == "VoltageSource" || pe.kind == "CurrentSource") {
    int radius = 15;
    // دایره برای هر دو
    if(pe.kind == "VoltageSource") SDL_SetRenderDrawColor(ren, 255, 165, 0, 255);
    else SDL_SetRenderDrawColor(ren, 128, 0, 128, 255);
    for (int a = 0; a < 360; a += 5) {
        int px = midx + int(radius * cos(a * M_PI / 180.0));
        int py = midy + int(radius * sin(a * M_PI / 180.0));
        SDL_RenderPoint(ren, px, py);
    }

    // خط اتصال به نودها
    SDL_RenderLine(ren, x1, y1, isVertical ? midx : midx - radius, isVertical ? midy - radius : midy);
    SDL_RenderLine(ren, isVertical ? midx : midx + radius, isVertical ? midy + radius : midy, x2, y2);

    if (pe.kind == "VoltageSource") {
        // --- نماد واضح برای منبع ولتاژ ---
        int sign_len = 4;
        // علامت مثبت (+)
        SDL_RenderLine(ren, isVertical ? midx - sign_len : midx - 8, isVertical ? midy - 8 : midy - sign_len, isVertical ? midx + sign_len : midx - 8, isVertical ? midy - 8 : midy + sign_len);
        SDL_RenderLine(ren, isVertical ? midx : midx - 8, isVertical ? midy - 8 : midy, isVertical ? midx : midx - 8, isVertical ? midy - 8 : midy); // خط افقی +
        // علامت منفی (-)
        SDL_RenderLine(ren, isVertical ? midx - sign_len : midx + 8, isVertical ? midy + 8 : midy - sign_len, isVertical ? midx + sign_len : midx + 8, isVertical ? midy + 8 : midy + sign_len);
    } else { // CurrentSource
        // --- نماد واضح برای منبع جریان (فلش) ---
        if (isVertical) {
            // فلش عمودی
            SDL_RenderLine(ren, midx, midy - 8, midx, midy + 8); // بدنه فلش
            SDL_RenderLine(ren, midx, midy + 8, midx - 5, midy + 3); // سر فلش
            SDL_RenderLine(ren, midx, midy + 8, midx + 5, midy + 3); // سر فلش
        } else {
            // فلش افقی
            SDL_RenderLine(ren, midx - 8, midy, midx + 8, midy); // بدنه فلش
            SDL_RenderLine(ren, midx + 8, midy, midx + 3, midy - 5); // سر فلش
            SDL_RenderLine(ren, midx + 8, midy, midx + 3, midy + 5); // سر فلش
        }
    }

        } else if (pe.kind == "Diode") {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            if (isVertical) {
                // مثلث عمودی
                SDL_RenderLine(ren, midx - 10, midy - 10, midx + 10, midy - 10);
                SDL_RenderLine(ren, midx - 10, midy - 10, midx, midy + 5);
                SDL_RenderLine(ren, midx + 10, midy - 10, midx, midy + 5);
                // خط عمود
                SDL_RenderLine(ren, midx - 10, midy + 5, midx + 10, midy + 5);
                // خط به نودها
                SDL_RenderLine(ren, x1, y1, midx, midy - 10);
                SDL_RenderLine(ren, midx, midy + 5, x2, y2);
            } else {
                // مثلث افقی (کد قبلی شما)
                SDL_RenderLine(ren, midx - 10, midy - 10, midx - 10, midy + 10);
                SDL_RenderLine(ren, midx - 10, midy - 10, midx + 5, midy);
                SDL_RenderLine(ren, midx - 10, midy + 10, midx + 5, midy);
                // خط عمود
                SDL_RenderLine(ren, midx + 5, midy - 10, midx + 5, midy + 10);
                // خط به نودها
                SDL_RenderLine(ren, x1, y1, midx - 10, midy);
                SDL_RenderLine(ren, midx + 5, midy, x2, y2);
            }

        } else if (pe.kind == "Ground") {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            // زمین همیشه یک شکل دارد و به نود اول وصل می‌شود
            int gx = pe.gx1 * cellSize + cellSize / 2;
            int gy = pe.gy1 * cellSize + cellSize / 2;
            SDL_RenderLine(ren, gx, gy, gx, gy + 15); // خط عمودی به سمت پایین
            SDL_RenderLine(ren, gx - 10, gy + 15, gx + 10, gy + 15);
            SDL_RenderLine(ren, gx - 6, gy + 20, gx + 6, gy + 20);
            SDL_RenderLine(ren, gx - 3, gy + 25, gx + 3, gy + 25);

        } else if (pe.kind == "Wire") {
            SDL_SetRenderDrawColor(ren, 100, 100, 100, 255);
            SDL_RenderLine(ren, x1, y1, x2, y2);
        }

        // --- کد نمایش متن و کادر ویرایش (بدون تغییر) ---
        if (!editing || !isSel) {
            // نمایش اسم و مقدار در حالت عادی
            renderText(ren, pe.name, midx + 10, midy - 25, false);
            if (!pe.valueStr.empty()) {
                renderText(ren, pe.valueStr, midx + 10, midy + 10, false);
            }
        }
    }

    // رندر کردن کادر ویرایش متن (این بخش بدون تغییر از قبل باقی می‌ماند)
    if (editing && selectedIndex.has_value()) {
        const auto& pe = staged.at(*selectedIndex);
        int mid_x = (pe.gx1 * cellSize + pe.gx2 * cellSize) / 2 + cellSize / 2;
        int mid_y = (pe.gy1 * cellSize + pe.gy2 * cellSize) / 2 + cellSize / 2;
        SDL_FRect editRect = { (float)mid_x - 50, (float)mid_y + 20, 100.0f, 25.0f };

        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderFillRect(ren, &editRect);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderRect(ren, &editRect);

        if (!inputBuffer.empty()) {
            renderText(ren, inputBuffer, (int)editRect.x + 5, (int)editRect.y + 5, true);
        }

        Uint32 ticks = SDL_GetTicks();
        if (ticks % 1000 < 500) {
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


    void CircuitGrid::commitToModel(Graph* g, NodeManager* nm) {
    for (auto& pe : staged) {
        if (pe.kind == "Wire") {
            nm->connect(pe.n1, pe.n2);
            continue;
        }

        int n1 = nm->getOrCreateNodeId(pe.n1);
        int n2 = nm->getOrCreateNodeId(pe.n2);

        if (pe.kind == "Resistor") {
            g->addElement(new Resistor(pe.name, n1, n2, pe.value));
        } else if (pe.kind == "Capacitor") {
            g->addElement(new Capacitor(pe.name, n1, n2, pe.value));
        } else if (pe.kind == "Inductor") {
            g->addElement(new Inductor(pe.name, n1, n2, pe.value));
        } else if (pe.kind == "VoltageSource") {
            g->addElement(new VoltageSource(pe.name, n1, n2, pe.value));
        } else if (pe.kind == "CurrentSource") {
            g->addElement(new CurrentSource(pe.name, n1, n2, pe.value));
        } else if (pe.kind == "Diode") {
            std::string model = pe.valueStr.empty() ? "default" : pe.valueStr;
            g->addElement(new Diode(pe.name, n1, n2, model));
        } else if (pe.kind == "Ground") {
            nm->assignNodeAsGND(pe.n1); // نود n1 زمین شود
        }
    }
}


} // namespace View
