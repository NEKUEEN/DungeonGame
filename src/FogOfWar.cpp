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
//  markVisible helper
// ============================================================
inline void FogOfWar::markVisible(int col, int row)
{
    if (col < 0 || col >= m_cols || row < 0 || row >= m_rows) return;
    m_grid[row][col] = FogState::Visible;
}

// ============================================================
//  Shadowcasting 1 octant
//  transform: col' = col*cx + row*cy, row' = col*rx + row*ry
// ============================================================
void FogOfWar::castOctant(int px, int py,
                           int cx, int cy, int rx, int ry,
                           int radius, const TileMap& map)
{
    // stack-based iterative shadowcasting (Bjorn Bergstrom algorithm)
    struct Shadow { float lo, hi; };
    std::vector<Shadow> shadows;

    for (int row = 1; row <= radius; ++row)
    {
        bool anyVisible = false;

        for (int col = 0; col <= row; ++col)
        {
            int mc = px + col*cx + row*cy;
            int mr = py + col*rx + row*ry;

            if (mc < 0 || mc >= m_cols || mr < 0 || mr >= m_rows) continue;

            float lo = (float)col       / (float)(row + 1);
            float hi = (float)(col + 1) / (float)row;

            // check ว่า tile นี้ถูก shadow บังไหม
            bool shadowed = false;
            for (auto& sh : shadows)
            {
                if (sh.lo <= lo && sh.hi >= hi)
                { shadowed = true; break; }
            }

            if (!shadowed)
            {
                markVisible(mc, mr);
                anyVisible = true;

                if (map.getTile(mc, mr) == TileType::Wall)
                {
                    // เพิ่ม shadow ใหม่
                    Shadow ns{lo, hi};
                    // merge กับ shadow ที่ overlap
                    std::vector<Shadow> merged;
                    for (auto& sh : shadows)
                    {
                        if (sh.hi < ns.lo || sh.lo > ns.hi)
                            merged.push_back(sh);
                        else
                        {
                            ns.lo = std::min(ns.lo, sh.lo);
                            ns.hi = std::max(ns.hi, sh.hi);
                        }
                    }
                    merged.push_back(ns);
                    shadows = merged;

                    // ถ้า shadow ครอบ 0-1 ทั้งหมดแล้ว หยุด row นี้
                    for (auto& sh : shadows)
                        if (sh.lo <= 0.f && sh.hi >= 1.f)
                            return;
                }
            }
        }
        if (!anyVisible) break;
    }
}

void FogOfWar::compute(int playerCol, int playerRow, int radius,
                       const TileMap& map)
{
    for (auto& row : m_grid)
        for (auto& cell : row)
            if (cell == FogState::Visible)
                cell = FogState::Explored;

    markVisible(playerCol, playerRow);

    // 8 octants: transform {cx, cy, rx, ry}
    const int T[8][4] = {
        { 1,  0,  0,  1},  // E-SE
        { 0,  1,  1,  0},  // SE-S
        { 0, -1,  1,  0},  // SW-S
        {-1,  0,  0,  1},  // W-SW
        {-1,  0,  0, -1},  // W-NW
        { 0, -1, -1,  0},  // NW-N
        { 0,  1, -1,  0},  // NE-N
        { 1,  0,  0, -1},  // E-NE
    };

    // ทางโล่ง → radius ใหญ่ขึ้น shadowcasting จัดการกำแพงเอง
    int r = radius * 1;

    for (auto& t : T)
        castOctant(playerCol, playerRow, t[0], t[1], t[2], t[3], r, map);
}

FogState FogOfWar::getState(int col, int row) const
{
    if (col < 0 || col >= m_cols || row < 0 || row >= m_rows)
        return FogState::Hidden;
    return m_grid[row][col];
}

bool FogOfWar::isVisible(int col, int row) const
{ return getState(col, row) == FogState::Visible; }

bool FogOfWar::isExplored(int col, int row) const
{
    auto s = getState(col, row);
    return s == FogState::Explored || s == FogState::Visible;
}

void FogOfWar::render(sf::RenderWindow& window, int tileSize)
{
    // ใช้ VertexArray วาดครั้งเดียว แทน draw call ทีละ tile
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    va.resize(m_rows * m_cols * 6);

    int idx = 0;
    float ts = (float)tileSize;

    for (int row = 0; row < m_rows; ++row)
    for (int col = 0; col < m_cols; ++col)
    {
        sf::Color color;
        switch (m_grid[row][col])
        {
            case FogState::Hidden:   color = sf::Color(0, 0, 0, 255); break;
            case FogState::Explored: color = sf::Color(0, 0, 0, 160); break;
            case FogState::Visible:  color = sf::Color(0, 0, 0, 0);   break;
        }

        if (color.a == 0) { idx += 6; continue; } // Visible ไม่ต้องวาด

        float x = (float)(col * tileSize);
        float y = (float)(row * tileSize);

        sf::Vector2f tl(x,    y);
        sf::Vector2f tr(x+ts, y);
        sf::Vector2f bl(x,    y+ts);
        sf::Vector2f br(x+ts, y+ts);

        va[idx+0].position = tl; va[idx+0].color = color;
        va[idx+1].position = tr; va[idx+1].color = color;
        va[idx+2].position = bl; va[idx+2].color = color;
        va[idx+3].position = tr; va[idx+3].color = color;
        va[idx+4].position = br; va[idx+4].color = color;
        va[idx+5].position = bl; va[idx+5].color = color;

        idx += 6;
    }

    window.draw(va);
}
