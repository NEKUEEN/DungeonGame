#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <map>
#include <random>
#include "lib/json.hpp"

enum class TileType { Wall=0, Floor=1, GateDown=2, GateUp=3 };

enum class ZoneType {
    None = 0,
    Darkness,
    CrystalBright,
    DeadMan,
    BlackRock
};

class TileMap
{
public:
    TileMap(int cols, int rows, int tileSize);
    void generate();
    void render(sf::RenderWindow& window);
    TileType getTile(int col,int row) const;
    void     setTile(int col,int row,TileType type);
    bool     isWalkable(int col,int row) const;
    bool     loadFromTiled(const std::string& path);
    int      getCols() const { return m_cols; }
    int      getRows() const { return m_rows; }
    ZoneType getZone(int col, int row) const;

private:
    struct Rect { int x,y,w,h; };
    struct BSPNode { Rect area,room; int left=-1,right=-1; bool hasRoom=false; };

    void processLayers(const nlohmann::json& layers);
    void fillWalls();
    void bspSplit(int nodeIdx,int depth,int maxDepth);
    void bspCarveRooms(int nodeIdx);
    void bspConnectRooms(int nodeIdx);
    Rect getRoomOf(int nodeIdx);
    void cavePass(std::vector<std::vector<bool>>& grid,int iterations);
    void carveCave(int x,int y,int w,int h);
    void carveCorridor(int x1,int y1,int x2,int y2);
    void carveRoom(int x,int y,int w,int h);
    sf::Color tileColor(TileType type) const;

    int m_cols,m_rows,m_tileSize;
    std::vector<std::vector<TileType>> m_grid;
    std::vector<std::vector<ZoneType>> m_zoneGrid;
    std::vector<BSPNode> m_bspNodes;
    std::mt19937 m_rng{ std::random_device{}() };

    // Textures — ดึงจาก TextureManager แทนเก็บเอง
    bool m_hasTextures = false;

    // Render cache — rebuild เฉพาะตอน map เปลี่ยน
    bool m_cacheDirty = true;
    std::map<const sf::Texture*, sf::VertexArray> m_renderCache;
};
