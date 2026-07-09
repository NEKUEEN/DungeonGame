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

// offset → octant indices ที่อนุญาต
// T[8] = 8 octant ตามลำดับใน array เดิม
// {1,0} = ขวา → octant 0,1,6,7 (ฝั่งขวา)
// {-1,0} = ซ้าย → octant 2,3,4,5 (ฝั่งซ้าย)
// {0,1} = ล่าง → octant 1,2,3,4 (ฝั่งล่าง)
// {0,-1} = บน → octant 0,5,6,7 (ฝั่งบน)

struct VirtualPlayer {
    int dc, dr;
    std::vector<int> octants; // index ของ T ที่ยิงได้
};

VirtualPlayer vplayers[] = {
    {0,  0, {0,1,2,3,4,5,6,7}}, // ตัวกลาง ยิงทุก octant
    {1,  0, {0,1,6,7}},          // ขวา
    {-1, 0, {2,3,4,5}},          // ซ้าย
    {0,  1, {1,2,3,4}},          // ล่าง
    {0, -1, {0,5,6,7}},          // บน
    {1,  1, {0,1}},   // ขวา-ล่าง
    {-1, 1, {2,3}},   // ซ้าย-ล่าง
    {1, -1, {6,7}},   // ขวา-บน
    {-1,-1, {4,5}},   // ซ้าย-บน
};

const int T[8][4] = {
    { 1, 0, 0, 1}, { 0, 1, 1, 0},
    { 0,-1, 1, 0}, {-1, 0, 0, 1},
    {-1, 0, 0,-1}, { 0,-1,-1, 0},
    { 0, 1,-1, 0}, { 1, 0, 0,-1},
};

for (auto& vp : vplayers)
{
    int vc = playerCol + vp.dc;
    int vr = playerRow + vp.dr;
    if (map.getTile(vc, vr) == TileType::Wall) continue;
    markVisible(vc, vr);
    for (int idx : vp.octants)
        castOctant(vc, vr, T[idx][0], T[idx][1], T[idx][2], T[idx][3], radius, map);
}
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

// ============================================================
//  render – cull ตาม gameView เหมือน TileMap::render() (ก่อนหน้านี้
//  loop ทั้ง m_rows*m_cols ทุกเฟรมไม่ว่าแมพจะใหญ่แค่ไหน → ที่แมพ 500x500
//  คือ 250,000 เซลล์ x 6 vertex ทุกเฟรม ทั้งที่มองเห็นจริงแค่ ~21x21 ไทล์
//  รอบ player นี่คือสาเหตุหลักที่ fps ร่วงเวลาแมพใหญ่ ไม่ใช่ TileMap
//  ซึ่ง cull ถูกอยู่แล้ว
// ============================================================
void FogOfWar::render(sf::RenderWindow& window, int tileSize, const sf::View& gameView)
{
    float ts = (float)tileSize;

    sf::Vector2f center = gameView.getCenter();
    sf::Vector2f size   = gameView.getSize();

    int minCol = std::max(0, (int)((center.x - size.x/2) / ts) - 1);
    int maxCol = std::min(m_cols-1, (int)((center.x + size.x/2) / ts) + 1);
    int minRow = std::max(0, (int)((center.y - size.y/2) / ts) - 1);
    int maxRow = std::min(m_rows-1, (int)((center.y + size.y/2) / ts) + 1);

    if (minCol > maxCol || minRow > maxRow) return;  // view นอกแมพทั้งหมด (กันไว้เฉยๆ)

    int visCols = maxCol - minCol + 1;
    int visRows = maxRow - minRow + 1;

    m_va.setPrimitiveType(sf::PrimitiveType::Triangles);
    m_va.resize((size_t)visCols * visRows * 6);  // ขนาดตาม viewport ไม่ใช่ทั้งแมพ

    int idx = 0;

    for (int row = minRow; row <= maxRow; ++row)
    for (int col = minCol; col <= maxCol; ++col)
    {
        sf::Color color;
        switch (m_grid[row][col])
        {
            case FogState::Hidden:   color = sf::Color(0, 0, 0, 255); break;
            case FogState::Explored: color = sf::Color(0, 0, 0, 160); break;
            case FogState::Visible:  color = sf::Color(0, 0, 0, 0);   break;
        }

        float x = (float)(col * tileSize);
        float y = (float)(row * tileSize);

        sf::Vector2f tl(x,    y);
        sf::Vector2f tr(x+ts, y);
        sf::Vector2f bl(x,    y+ts);
        sf::Vector2f br(x+ts, y+ts);

        m_va[idx+0].position = tl; m_va[idx+0].color = color;
        m_va[idx+1].position = tr; m_va[idx+1].color = color;
        m_va[idx+2].position = bl; m_va[idx+2].color = color;
        m_va[idx+3].position = tr; m_va[idx+3].color = color;
        m_va[idx+4].position = br; m_va[idx+4].color = color;
        m_va[idx+5].position = bl; m_va[idx+5].color = color;

        idx += 6;
    }

    window.draw(m_va);
}
