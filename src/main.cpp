#include <iostream>
#include "App.h"

int main(const int argc, char ** argv) {
    std::cout << "Hello, World!" << std::endl;
    Ancile::Application app{"Simulator", 1000, 500};
    app.Run(argc, argv);
    app.Wait();
    return 0;
}
