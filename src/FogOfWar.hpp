#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

enum class FogState { Hidden, Explored, Visible };

class FogOfWar
{
public:
    FogOfWar(int cols, int rows);
    void reset();
    void compute(int playerCol, int playerRow, int radius,
                 const class TileMap& map);
    FogState getState(int col, int row) const;
    bool isVisible(int col, int row) const;
    bool isExplored(int col, int row) const;
    void render(sf::RenderWindow& window, int tileSize);

private:
    void castOctant(int px, int py,
                    int cx, int cy, int rx, int ry,
                    int radius, const class TileMap& map);
    void markVisible(int col, int row);

    int m_cols, m_rows;
    std::vector<std::vector<FogState>> m_grid;
};
