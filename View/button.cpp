#include "Button.h"
#include <utility>

Button::Button(int x, int y, int w, int h, std::string text)
        : rect{(float)x,(float)y,(float)w,(float)h}, buttonText(std::move(text)) {}

bool Button::handleEvent(const SDL_Event& e) {
    bool clicked = false;
    if (e.type == SDL_EVENT_MOUSE_MOTION ||
        e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        float mx, my;
        SDL_GetMouseState(&mx, &my);
        bool inside = (mx >= rect.x && mx <= rect.x + rect.w &&
                       my >= rect.y && my <= rect.y + rect.h);
        if (!inside) {
            currentState = State::NORMAL;
        } else {
            if (e.type == SDL_EVENT_MOUSE_MOTION) currentState = State::HOVER;
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) currentState = State::PRESSED;
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) { currentState = State::HOVER; clicked = true; }
        }
    }
    return clicked;
}

void Button::render(SDL_Renderer* renderer) {
    SDL_Color col = colorNormal;
    if (currentState == State::HOVER) col = colorHover;
    else if (currentState == State::PRESSED) col = colorPressed;

    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
    SDL_RenderFillRect(renderer, &rect);

    if (!buttonText.empty() && font) {
        SDL_Surface* surf = TTF_RenderText_Solid(font, buttonText.c_str(), (size_t)buttonText.size(), SDL_Color{0,0,0,255});
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_FRect dst;
                dst.w = (float)surf->w;
                dst.h = (float)surf->h;
                dst.x = rect.x + (rect.w - dst.w) * 0.5f;
                dst.y = rect.y + (rect.h - dst.h) * 0.5f;
                SDL_RenderTexture(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_DestroySurface(surf);
        }
    }
}
