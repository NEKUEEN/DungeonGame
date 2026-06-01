#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

// ============================================================
//  FogOfWar  –  ระบบความมืด
//  แต่ละ tile มี 3 สถานะ:
//  Hidden   = ไม่เคยเห็น (มืดสนิท)
//  Explored = เคยเห็นแล้ว (มืดครึ่ง)
//  Visible  = เห็นอยู่ตอนนี้ (สว่าง)
// ============================================================
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
    bool hasLineOfSight(int x0, int y0, int x1, int y1,
                        const class TileMap& map) const;

    int m_cols, m_rows;
    std::vector<std::vector<FogState>> m_grid;
};
