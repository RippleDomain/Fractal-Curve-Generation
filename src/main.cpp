#include "core/App.h"
#include <iostream>

int main() 
{
    App app;

    if (!app.init(1920, 1080, "Fractal Curve Generation")) 
    {
        std::cerr << "App starting failed.\n"; return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}