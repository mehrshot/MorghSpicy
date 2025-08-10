#if defined(_WIN32)
#include <SDL3/SDL_main.h>
#endif
#include "View/App.h"

int main(int argc, char* argv[]) {
    App app;
    return app.run();
}
