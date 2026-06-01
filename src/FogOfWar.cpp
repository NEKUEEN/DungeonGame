#include "FogOfWar.hpp"
#include "TileMap.hpp"
#include <cmath>
#include <algorithm>

FogOfWar::FogOfWar(int cols, int rows)
    : m_cols(cols), m_rows(rows)
{
    m_grid.assign(rows, std::vector<FogState>(cols, FogState::Hidden));
}

void FogOfWar::reset()
{
    for (auto& row : m_grid)
        for (auto& cell : row)
            cell = FogState::Hidden;
}

// ============================================================
//  Compute  –  คำนวณ visibility รอบ player
// ============================================================
void FogOfWar::compute(int playerCol, int playerRow, int radius,
                       const TileMap& map)
{
    // รีเซ็ต Visible → Explored ก่อน
    for (auto& row : m_grid)
        for (auto& cell : row)
            if (cell == FogState::Visible)
                cell = FogState::Explored;

    // cast rays จาก player ไปรอบๆ
    for (int row = playerRow - radius; row <= playerRow + radius; ++row)
    {
        for (int col = playerCol - radius; col <= playerCol + radius; ++col)
        {
            if (col < 0 || col >= m_cols || row < 0 || row >= m_rows) continue;

            int dx = col - playerCol;
            int dy = row - playerRow;
            if (dx*dx + dy*dy > radius*radius) continue;

            if (hasLineOfSight(playerCol, playerRow, col, row, map))
                m_grid[row][col] = FogState::Visible;
        }
    }
}

// ============================================================
//  Line of Sight  –  Bresenham's line algorithm
// ============================================================
bool FogOfWar::hasLineOfSight(int x0, int y0, int x1, int y1,
                               const TileMap& map) const
{
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    int cx = x0, cy = y0;
    while (true)
    {
        if (cx == x1 && cy == y1) return true;

        // ถ้าชนกำแพง (ยกเว้น tile เริ่มต้น) = บัง LOS
        if (!(cx == x0 && cy == y0) && map.getTile(cx, cy) == TileType::Wall)
            return false;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 <  dx) { err += dx; cy += sy; }
    }
}

// ============================================================
//  Getters
// ============================================================
FogState FogOfWar::getState(int col, int row) const
{
    if (col < 0 || col >= m_cols || row < 0 || row >= m_rows)
        return FogState::Hidden;
    return m_grid[row][col];
}

bool FogOfWar::isVisible(int col, int row) const
{
    return getState(col, row) == FogState::Visible;
}

bool FogOfWar::isExplored(int col, int row) const
{
    auto s = getState(col, row);
    return s == FogState::Explored || s == FogState::Visible;
}

// ============================================================
//  Render  –  วาด overlay ทับ map
// ============================================================
void FogOfWar::render(sf::RenderWindow& window, int tileSize)
{
    sf::RectangleShape tile({(float)tileSize, (float)tileSize});

    for (int row = 0; row < m_rows; ++row)
    {
        for (int col = 0; col < m_cols; ++col)
        {
            tile.setPosition({(float)(col * tileSize), (float)(row * tileSize)});

            switch (m_grid[row][col])
            {
                case FogState::Hidden:
                    tile.setFillColor(sf::Color(0, 0, 0, 255));  // มืดสนิท
                    window.draw(tile);
                    break;
                case FogState::Explored:
                    tile.setFillColor(sf::Color(0, 0, 0, 160));  // มืดครึ่ง
                    window.draw(tile);
                    break;
                case FogState::Visible:
                    // สว่าง ไม่วาดอะไรทับ
                    break;
            }
        }
    }
}
