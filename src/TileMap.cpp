#include "TileMap.hpp"
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <iostream>

TileMap::TileMap(int cols, int rows, int tileSize)
    : m_cols(cols), m_rows(rows), m_tileSize(tileSize)
{
    m_grid.assign(rows, std::vector<TileType>(cols, TileType::Wall));
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // โหลด textures
    m_hasTextures = true;
    if (!m_texFloor.loadFromFile("assets/textures/tile_floor.png"))
    { std::cerr<<"[TileMap] tile_floor.png not found\n"; m_hasTextures=false; }
    if (!m_texWall.loadFromFile("assets/textures/tile_wall.png"))
    { std::cerr<<"[TileMap] tile_wall.png not found\n"; m_hasTextures=false; }
    if (!m_texStairsDown.loadFromFile("assets/textures/stairs_down.png"))
        m_hasTextures=false;
    if (!m_texStairsUp.loadFromFile("assets/textures/stairs_up.png"))
        m_hasTextures=false;
}

void TileMap::generate()
{
    fillWalls();
    m_bspNodes.clear();
    BSPNode root; root.area={1,1,m_cols-2,m_rows-2};
    m_bspNodes.push_back(root);
    bspSplit(0,0,4);
    bspCarveRooms(0);
    bspConnectRooms(0);

    int numCaves=2+std::rand()%3;
    for (int i=0;i<numCaves;++i)
    {
        int cw=10+std::rand()%10, ch=10+std::rand()%10;
        int cx=2+std::rand()%(m_cols-cw-4), cy=2+std::rand()%(m_rows-ch-4);
        carveCave(cx,cy,cw,ch);
    }

    int startCol=-1,startRow=-1,endCol=-1,endRow=-1;
    float maxDist=0.f;
    float cx2=m_cols/2.f,cy2=m_rows/2.f;
    for (int row=1;row<m_rows-1;++row)
        for (int col=1;col<m_cols-1;++col)
            if (m_grid[row][col]==TileType::Floor)
            {
                if (startCol==-1){startCol=col;startRow=row;}
                float d=std::abs(col-cx2)+std::abs(row-cy2);
                if (d>maxDist){maxDist=d;endCol=col;endRow=row;}
            }
    if (startCol!=-1) setTile(startCol,startRow,TileType::StairsUp);
    if (endCol!=-1)   setTile(endCol,endRow,TileType::StairsDown);
}

void TileMap::bspSplit(int nodeIdx,int depth,int maxDepth)
{
    BSPNode& node=m_bspNodes[nodeIdx];
    if (depth>=maxDepth) return;
    int minSize=8;
    bool canH=node.area.h>=minSize*2, canV=node.area.w>=minSize*2;
    if (!canH&&!canV) return;
    bool splitH;
    if (!canH) splitH=false;
    else if (!canV) splitH=true;
    else splitH=(std::rand()%2==0);
    BSPNode l,r;
    if (splitH)
    {
        int s=node.area.y+minSize+std::rand()%(node.area.h-minSize*2+1);
        l.area={node.area.x,node.area.y,node.area.w,s-node.area.y};
        r.area={node.area.x,s,node.area.w,node.area.y+node.area.h-s};
    }
    else
    {
        int s=node.area.x+minSize+std::rand()%(node.area.w-minSize*2+1);
        l.area={node.area.x,node.area.y,s-node.area.x,node.area.h};
        r.area={s,node.area.y,node.area.x+node.area.w-s,node.area.h};
    }
    int li=(int)m_bspNodes.size(); m_bspNodes.push_back(l);
    int ri=(int)m_bspNodes.size(); m_bspNodes.push_back(r);
    m_bspNodes[nodeIdx].left=li; m_bspNodes[nodeIdx].right=ri;
    bspSplit(li,depth+1,maxDepth); bspSplit(ri,depth+1,maxDepth);
}

void TileMap::bspCarveRooms(int nodeIdx)
{
    BSPNode& node=m_bspNodes[nodeIdx];
    if (node.left==-1&&node.right==-1)
    {
        int mg=1,maxW=node.area.w-mg*2,maxH=node.area.h-mg*2;
        int minW=std::max(3,maxW/2),minH=std::max(3,maxH/2);
        if (maxW<3||maxH<3) return;
        int rw=minW+std::rand()%(maxW-minW+1);
        int rh=minH+std::rand()%(maxH-minH+1);
        int rx=node.area.x+mg+std::rand()%(node.area.w-rw-mg*2+1);
        int ry=node.area.y+mg+std::rand()%(node.area.h-rh-mg*2+1);
        carveRoom(rx,ry,rw,rh);
        node.room={rx,ry,rw,rh}; node.hasRoom=true; return;
    }
    if (node.left!=-1) bspCarveRooms(node.left);
    if (node.right!=-1) bspCarveRooms(node.right);
}

TileMap::Rect TileMap::getRoomOf(int nodeIdx)
{
    BSPNode& node=m_bspNodes[nodeIdx];
    if (node.hasRoom) return node.room;
    if (node.left!=-1) return getRoomOf(node.left);
    return {0,0,1,1};
}

void TileMap::bspConnectRooms(int nodeIdx)
{
    BSPNode& node=m_bspNodes[nodeIdx];
    if (node.left==-1||node.right==-1) return;
    bspConnectRooms(node.left); bspConnectRooms(node.right);
    TileMap::Rect l=getRoomOf(node.left),r=getRoomOf(node.right);
    int lx=l.x+l.w/2,ly=l.y+l.h/2,rx=r.x+r.w/2,ry=r.y+r.h/2;
    int mx=lx+(rx-lx)/2+(std::rand()%5-2);
    int my=ly+(ry-ly)/2+(std::rand()%5-2);
    carveCorridor(lx,ly,mx,my); carveCorridor(mx,my,rx,ry);
}

void TileMap::carveCave(int x,int y,int w,int h)
{
    std::vector<std::vector<bool>> grid(h,std::vector<bool>(w,false));
    for (int r=0;r<h;++r) for (int c=0;c<w;++c) grid[r][c]=(std::rand()%100<45);
    cavePass(grid,4);
    for (int r=0;r<h;++r) for (int c=0;c<w;++c)
        if (grid[r][c]){int mc=x+c,mr=y+r;if(mc>0&&mc<m_cols-1&&mr>0&&mr<m_rows-1)m_grid[mr][mc]=TileType::Floor;}
}

void TileMap::cavePass(std::vector<std::vector<bool>>& grid,int iterations)
{
    int h=(int)grid.size(),w=(int)grid[0].size();
    for (int it=0;it<iterations;++it)
    {
        auto next=grid;
        for (int r=1;r<h-1;++r) for (int c=1;c<w-1;++c)
        {
            int nb=0;
            for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx)
                if (!(dx==0&&dy==0)&&grid[r+dy][c+dx]) nb++;
            next[r][c]=nb>=5?true:nb<=2?false:grid[r][c];
        }
        grid=next;
    }
}

void TileMap::fillWalls()
{ for (auto&r:m_grid) for (auto&c:r) c=TileType::Wall; }

void TileMap::carveRoom(int x,int y,int w,int h)
{
    for (int r=y;r<y+h&&r<m_rows-1;++r)
        for (int c=x;c<x+w&&c<m_cols-1;++c)
            if (r>0&&c>0) m_grid[r][c]=TileType::Floor;
}

void TileMap::carveCorridor(int x1,int y1,int x2,int y2)
{
    int cx=x1,cy=y1;
    while(cx!=x2){if(cx>=0&&cx<m_cols&&cy>=0&&cy<m_rows)m_grid[cy][cx]=TileType::Floor;cx+=(x2>x1)?1:-1;}
    while(cy!=y2){if(cx>=0&&cx<m_cols&&cy>=0&&cy<m_rows)m_grid[cy][cx]=TileType::Floor;cy+=(y2>y1)?1:-1;}
    if(cx>=0&&cx<m_cols&&cy>=0&&cy<m_rows)m_grid[cy][cx]=TileType::Floor;
}

// ============================================================
//  Render  –  ใช้ texture ถ้ามี
// ============================================================
void TileMap::render(sf::RenderWindow& window)
{
    for (int row=0;row<m_rows;++row)
    for (int col=0;col<m_cols;++col)
    {
        TileType type=m_grid[row][col];
        float px=(float)(col*m_tileSize), py=(float)(row*m_tileSize);
        float ts=(float)m_tileSize;

        if (m_hasTextures)
        {
            sf::Texture* tex=nullptr;
            switch(type)
            {
                case TileType::Floor:      tex=&m_texFloor;      break;
                case TileType::Wall:       tex=&m_texWall;       break;
                case TileType::StairsDown: tex=&m_texStairsDown; break;
                case TileType::StairsUp:   tex=&m_texStairsUp;   break;
            }
            if (tex)
            {
                sf::Sprite spr(*tex);
                auto sz=tex->getSize();
                spr.setScale({ts/sz.x, ts/sz.y});
                spr.setPosition({px,py});
                window.draw(spr);
                continue;
            }
        }

        // Fallback สี
        sf::RectangleShape tile({ts-1.f,ts-1.f});
        tile.setFillColor(tileColor(type));
        tile.setPosition({px,py});
        window.draw(tile);
    }
}

TileType TileMap::getTile(int col,int row) const
{ if(col<0||col>=m_cols||row<0||row>=m_rows)return TileType::Wall; return m_grid[row][col]; }

void TileMap::setTile(int col,int row,TileType type)
{ if(col<0||col>=m_cols||row<0||row>=m_rows)return; m_grid[row][col]=type; }

bool TileMap::isWalkable(int col,int row) const
{ TileType t=getTile(col,row); return t==TileType::Floor||t==TileType::StairsDown||t==TileType::StairsUp; }

sf::Color TileMap::tileColor(TileType type) const
{
    switch(type)
    {
        case TileType::Floor:      return sf::Color(80,60,45);
        case TileType::Wall:       return sf::Color(35,28,22);
        case TileType::StairsDown: return sf::Color(200,160,40);
        case TileType::StairsUp:   return sf::Color(80,180,220);
        default:                   return sf::Color(20,20,20);
    }
}
