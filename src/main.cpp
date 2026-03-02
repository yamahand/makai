#include "engine/core/Game.hpp"
#include <iostream>

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        makai::Game game;
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
