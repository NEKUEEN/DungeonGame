#include "Game.hpp"

int main()
{
    try {
        Game game;
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        system("pause");
    }
    return 0;
}
