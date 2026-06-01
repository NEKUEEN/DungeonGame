#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

enum class TileType { Wall=0, Floor=1, StairsDown=2, StairsUp=3 };

class TileMap
{
public:
    TileMap(int cols, int rows, int tileSize);
    void generate();
    void render(sf::RenderWindow& window);
    TileType getTile(int col,int row) const;
    void     setTile(int col,int row,TileType type);
    bool     isWalkable(int col,int row) const;
    int getCols() const { return m_cols; }
    int getRows() const { return m_rows; }

private:
    struct Rect { int x,y,w,h; };
    struct BSPNode { Rect area,room; int left=-1,right=-1; bool hasRoom=false; };

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
    std::vector<BSPNode> m_bspNodes;

    // Textures
    bool       m_hasTextures = false;
    sf::Texture m_texFloor, m_texWall, m_texStairsDown, m_texStairsUp;
};
