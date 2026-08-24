#include "Core/Engine.h"
#include <iostream>

int main()
{
    Fluid::Engine engine;

    std::cout << "Starting Fluids Simulation... " << std::endl;
    if (!engine.Initialize(1280, 720, "Fluid Simulation ECS (DOD)"))
        return -1;

    engine.Run();

    return 0;
}