#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>

class Button {
public:
    Button() = default;
    Button(int x, int y, int w, int h, std::string text);

    bool handleEvent(const SDL_Event& e);
    void render(SDL_Renderer* renderer);
    void setFont(TTF_Font* f) { font = f; }

private:
    SDL_FRect rect{0,0,0,0};
    std::string buttonText;

    SDL_Color colorNormal{200,200,200,255};
    SDL_Color colorHover{220,220,220,255};
    SDL_Color colorPressed{180,180,180,255};

    enum class State { NORMAL, HOVER, PRESSED };
    State currentState = State::NORMAL;

    TTF_Font* font = nullptr;
};
