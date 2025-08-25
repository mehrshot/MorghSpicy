#include "Button.h"
#include <utility>

Button::Button(int x, int y, int w, int h, std::string text)
// Added explicit (float) casts to prevent narrowing error
        : rect{ (float)x, (float)y, (float)w, (float)h }, buttonText(std::move(text)) {}

bool Button::handleEvent(const SDL_Event& e) {
    bool clicked = false;
    if (e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        float mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);

        bool isInside = (mouseX >= rect.x && mouseX <= rect.x + rect.w &&
                         mouseY >= rect.y && mouseY <= rect.y + rect.h);

        if (!isInside) {
            currentState = State::NORMAL;
        } else {
            if (e.type == SDL_EVENT_MOUSE_MOTION) {
                if (currentState != State::PRESSED) {
                    currentState = State::HOVER;
                }
            } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                currentState = State::PRESSED;
            } else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
                if (currentState == State::PRESSED) {
                    clicked = true;
                    currentState = State::HOVER;
                }
            }
        }
    }
    return clicked;
}

void Button::render(SDL_Renderer* renderer) {
    SDL_Color currentColor;
    switch (currentState) {
        case State::HOVER:
            currentColor = colorHover;
            break;
        case State::PRESSED:
            currentColor = colorPressed;
            break;
        case State::NORMAL:
        default:
            currentColor = colorNormal;
            break;
    }

    SDL_SetRenderDrawColor(renderer, currentColor.r, currentColor.g, currentColor.b, currentColor.a);
    SDL_RenderFillRect(renderer, &rect);
    if (!buttonText.empty() && font) {
        SDL_Surface* textSurface = TTF_RenderText_Solid( font, buttonText.c_str(), {0, 0, 0, 255}); // Black text
        if (textSurface) {
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
            if (textTexture) {
                // Center the text inside the button
                SDL_FRect textRect;
                textRect.w = (float)textSurface->w;
                textRect.h = (float)textSurface->h;
                textRect.x = rect.x + (rect.w - textRect.w) / 2.0f;
                textRect.y = rect.y + (rect.h - textRect.h) / 2.0f;

                SDL_RenderTexture(renderer, textTexture, nullptr, &textRect);
                SDL_DestroyTexture(textTexture);
            }
            SDL_DestroySurface(textSurface);
        }
    }
}