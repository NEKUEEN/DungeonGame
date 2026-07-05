#include "TileMap.hpp"
#include "TextureManager.hpp"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>
#include "lib/json.hpp"

static std::string basenameNoExt(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    std::string fname = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = fname.find_last_of('.');
    if (dot != std::string::npos) fname = fname.substr(0, dot);
    return fname;
}

TileMap::TileMap(int cols, int rows, int tileSize)
    : m_cols(cols), m_rows(rows), m_tileSize(tileSize)
{
    m_grid.assign(rows, std::vector<TileType>(cols, TileType::Wall));
    m_zoneGrid.assign(rows, std::vector<ZoneType>(cols, ZoneType::None));
    m_hasTextures = true;
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

    auto randi = [&](int lo, int hi) -> int {   // inclusive [lo,hi]
        return std::uniform_int_distribution<int>(lo,hi)(m_rng);
    };

    int numCaves = 2 + randi(0,2);
    for (int i=0;i<numCaves;++i)
    {
        int cw = 10 + randi(0,9);
        int ch = 10 + randi(0,9);
        int cx = 2  + randi(0, m_cols-cw-5);
        int cy = 2  + randi(0, m_rows-ch-5);
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
    if (startCol!=-1) setTile(startCol,startRow,TileType::GateUp);
    if (endCol!=-1)   setTile(endCol,endRow,TileType::GateDown);
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
    else splitH=(std::uniform_int_distribution<int>(0,1)(m_rng)==0);
    BSPNode l,r;
    if (splitH)
    {
        int s=node.area.y+minSize+std::uniform_int_distribution<int>(0,node.area.h-minSize*2)(m_rng);
        l.area={node.area.x,node.area.y,node.area.w,s-node.area.y};
        r.area={node.area.x,s,node.area.w,node.area.y+node.area.h-s};
    }
    else
    {
        int s=node.area.x+minSize+std::uniform_int_distribution<int>(0,node.area.w-minSize*2)(m_rng);
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
        int rw=minW+std::uniform_int_distribution<int>(0,maxW-minW)(m_rng);
        int rh=minH+std::uniform_int_distribution<int>(0,maxH-minH)(m_rng);
        int rx=node.area.x+mg+std::uniform_int_distribution<int>(0,node.area.w-rw-mg*2)(m_rng);
        int ry=node.area.y+mg+std::uniform_int_distribution<int>(0,node.area.h-rh-mg*2)(m_rng);
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
    int mx=lx+(rx-lx)/2+std::uniform_int_distribution<int>(-2,2)(m_rng);
    int my=ly+(ry-ly)/2+std::uniform_int_distribution<int>(-2,2)(m_rng);
    carveCorridor(lx,ly,mx,my); carveCorridor(mx,my,rx,ry);
}

void TileMap::carveCave(int x,int y,int w,int h)
{
    std::vector<std::vector<bool>> grid(h,std::vector<bool>(w,false));
    std::uniform_int_distribution<int> pct(0,99);
    for (int r=0;r<h;++r) for (int c=0;c<w;++c) grid[r][c]=(pct(m_rng)<45);
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
void TileMap::render(sf::RenderWindow& window, const sf::View& gameView)
{
    auto& tm = TextureManager::instance();
    float ts = (float)m_tileSize;

    // คำนวณ tile ที่อยู่ใน view
    sf::Vector2f center = gameView.getCenter();
    sf::Vector2f size   = gameView.getSize();

    int minCol = std::max(0, (int)((center.x - size.x/2) / ts) - 1);
    int maxCol = std::min(m_cols-1, (int)((center.x + size.x/2) / ts) + 1);
    int minRow = std::max(0, (int)((center.y - size.y/2) / ts) - 1);
    int maxRow = std::min(m_rows-1, (int)((center.y + size.y/2) / ts) + 1);

    // build vertex array เฉพาะ tile ที่เห็น
    std::map<const sf::Texture*, sf::VertexArray> frameCache;

    auto getTexKey = [&](TileType type, ZoneType zone) -> const char* {
        switch(type) {
            case TileType::Floor:
                if (zone == ZoneType::Darkness)      return "darkness_floor";
                if (zone == ZoneType::CrystalBright) return "crystalBright_floor";
                if (zone == ZoneType::DeadMan)       return "deadMan_floor";
                if (zone == ZoneType::BlackRock)     return "blackRock_floor";
                return "tile_floor";
            case TileType::Wall:
                if (zone == ZoneType::CrystalBright) return "crystalBright_wall";
                if (zone == ZoneType::Darkness) return "darkness_wall";
                if (zone == ZoneType::DeadMan) return "brick1";
                if (zone == ZoneType::DeadMan) return "brick2";
                if (zone == ZoneType::DeadMan) return "WALL1";
                return "tile_wall";
            case TileType::GateDown: return "gate_down";
            case TileType::GateUp:   return "gate_up";
        }
        return nullptr;
    };

    for (int row = minRow; row <= maxRow; ++row)
    for (int col = minCol; col <= maxCol; ++col)
    {
        const char* key = getTexKey(m_grid[row][col], m_zoneGrid[row][col]);
        if (!key) continue;
        const sf::Texture* tex = tm.get(key);
        if (!tex) continue;

        float x = (float)(col * m_tileSize);
        float y = (float)(row * m_tileSize);
        auto sz = tex->getSize();
        float tw = (float)sz.x, th = (float)sz.y;

        auto& va = frameCache[tex];
        if (va.getPrimitiveType() != sf::PrimitiveType::Triangles)
            va.setPrimitiveType(sf::PrimitiveType::Triangles);

        size_t base = va.getVertexCount();
        va.resize(base + 6);

        va[base+0].position = {x,    y};    va[base+0].texCoords = {0,  0};
        va[base+1].position = {x+ts, y};    va[base+1].texCoords = {tw, 0};
        va[base+2].position = {x,    y+ts}; va[base+2].texCoords = {0,  th};
        va[base+3].position = {x+ts, y};    va[base+3].texCoords = {tw, 0};
        va[base+4].position = {x+ts, y+ts}; va[base+4].texCoords = {tw, th};
        va[base+5].position = {x,    y+ts}; va[base+5].texCoords = {0,  th};
    }

    sf::RenderStates states;
    for (auto& [tex, va] : frameCache)
    {
        states.texture = tex;
        window.draw(va, states);
    }
    
    // ลบ cache เก่าออก ไม่ต้องการแล้ว
    m_cacheDirty = false;
}



TileType TileMap::getTile(int col,int row) const
{ if(col<0||col>=m_cols||row<0||row>=m_rows)return TileType::Wall; return m_grid[row][col]; }

void TileMap::setTile(int col,int row,TileType type)
{ if(col<0||col>=m_cols||row<0||row>=m_rows)return; m_grid[row][col]=type; m_cacheDirty=true; }

bool TileMap::isWalkable(int col,int row) const
{ TileType t=getTile(col,row); return t==TileType::Floor||t==TileType::GateDown||t==TileType::GateUp; }

ZoneType TileMap::getZone(int col, int row) const
{
    if (col<0||col>=m_cols||row<0||row>=m_rows) return ZoneType::None;
    return m_zoneGrid[row][col];
}

sf::Color TileMap::tileColor(TileType type) const
{
    switch(type)
    {
        case TileType::Floor:      return sf::Color(80,60,45);
        case TileType::Wall:       return sf::Color(35,28,22);
        case TileType::GateDown: return sf::Color(200,160,40);
        case TileType::GateUp:   return sf::Color(80,180,220);
        default:                   return sf::Color(20,20,20);
    }
    
}

// ============================================================
//  Base64 decode helper (สำหรับ Tiled .tmj ที่ใช้ encoding=base64)
// ============================================================
static std::vector<uint32_t> decodeBase64Tiles(const std::string& b64)
{
    // base64 alphabet
    static const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::vector<uint8_t> bytes;
    bytes.reserve(b64.size() * 3 / 4);

    int val = 0, valb = -8;
    for (unsigned char c : b64)
    {
        if (c == '=' || c == '\n' || c == '\r') continue;
        auto pos = chars.find((char)c);
        if (pos == std::string::npos) continue;
        val = (val << 6) + (int)pos;
        valb += 6;
        if (valb >= 0)
        {
            bytes.push_back((uint8_t)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    // Tiled เก็บ tile ID เป็น little-endian uint32
    std::vector<uint32_t> tiles(bytes.size() / 4);
    for (size_t i = 0; i < tiles.size(); ++i)
        tiles[i] = (uint32_t)bytes[i*4]
                 | ((uint32_t)bytes[i*4+1] << 8)
                 | ((uint32_t)bytes[i*4+2] << 16)
                 | ((uint32_t)bytes[i*4+3] << 24);
    return tiles;
}

// ============================================================
//  processLayers — recursive รองรับ group layer จาก Tiled
// ============================================================
void TileMap::processLayers(const nlohmann::json& layers)
{
    for (const auto& layer : layers)
    {
        std::string type = layer["type"].get<std::string>();

        if (type == "group")
        {
            if (layer.contains("layers"))
                processLayers(layer["layers"]);
            continue;
        }

        if (type == "objectgroup")
        {
            for (const auto& obj : layer["objects"])
            {
                std::string objName = obj.contains("name") ? obj["name"].get<std::string>() : "";
                float objX = obj["x"].get<float>();
                float objY = obj["y"].get<float>();
                // tile object (มี gid) ใน Tiled ใช้ y เป็นขอบล่าง ต้องลบ height ออกก่อน
                if (obj.contains("gid") && obj.contains("height"))
                    objY -= obj["height"].get<float>();
                int col = (int)(objX / m_tileSize);
                int row = (int)(objY / m_tileSize);

                std::string tsName;
                if (obj.contains("gid"))
                    tsName = tilesetNameForGid((uint32_t)obj["gid"].get<int>());
                std::string tsLower = tsName;
                std::transform(tsLower.begin(), tsLower.end(), tsLower.begin(), ::tolower);

                if (objName == "gate_up" || tsLower.find("gateup") != std::string::npos)
                    setTile(col, row, TileType::GateUp);
                else if (objName == "gate_down" || tsLower.find("gatedown") != std::string::npos)
                    setTile(col, row, TileType::GateDown);
            }
            continue;
        }

        if (type != "tilelayer") continue;

        std::string name = layer["name"].get<std::string>();
        int w = layer["width"].get<int>();

        bool isFloor = (name.rfind("floor", 0) == 0 || name.find("_floor") != std::string::npos);
        bool isWall  = (name.rfind("wall",  0) == 0 || name.find("_wall")  != std::string::npos);

        // map layer name → ZoneType
        ZoneType zone = ZoneType::None;
        if      (name.find("darkness")      != std::string::npos) zone = ZoneType::Darkness;
        else if (name.find("crystalBright") != std::string::npos) zone = ZoneType::CrystalBright;
        else if (name.find("deadMan")       != std::string::npos) zone = ZoneType::DeadMan;
        else if (name.find("blackRock")     != std::string::npos) zone = ZoneType::BlackRock;

        std::vector<uint32_t> tiles;
        std::string encoding = (layer.contains("encoding") && layer["encoding"].is_string())
                               ? layer["encoding"].get<std::string>() : "";

        if (encoding == "base64")
            tiles = decodeBase64Tiles(layer["data"].get<std::string>());
        else
        {
            const auto& data = layer["data"];
            tiles.reserve(data.size());
            for (const auto& v : data)
                tiles.push_back((uint32_t)v.get<int>());
        }

        for (int i = 0; i < (int)tiles.size(); ++i)
        {
            uint32_t tileId = tiles[i] & 0x0FFFFFFF;
            int col = i % w;
            int row = i / w;
            if (col >= m_cols || row >= m_rows) continue;
            if (tileId == 0) continue;

            std::string tsName = tilesetNameForGid(tileId);
            std::string tsLower = tsName;
            std::transform(tsLower.begin(), tsLower.end(), tsLower.begin(), ::tolower);

            bool determined = false;
            TileType tType = TileType::Floor;
            ZoneType tZone = zone;

            if (tsLower.find("wall") != std::string::npos)
            {
                tType = TileType::Wall; determined = true;
            }
            else if (tsLower.find("gatedown") != std::string::npos)
            {
                tType = TileType::GateDown; determined = true;
            }
            else if (tsLower.find("gateup") != std::string::npos)
            {
                tType = TileType::GateUp; determined = true;
            }
            else if (tsLower.find("floor") != std::string::npos)
            {
                tType = TileType::Floor; determined = true;
                if      (tsLower.find("deadman")      != std::string::npos) tZone = ZoneType::DeadMan;
                else if (tsLower.find("darkness")      != std::string::npos) tZone = ZoneType::Darkness;
                else if (tsLower.find("crystalbright") != std::string::npos) tZone = ZoneType::CrystalBright;
                else if (tsLower.find("blackrock")     != std::string::npos) tZone = ZoneType::BlackRock;
            }

            if (determined)
            {
                setTile(col, row, tType);
                if (tZone != ZoneType::None) m_zoneGrid[row][col] = tZone;
            }
            else if (isFloor)
            {
                setTile(col, row, TileType::Floor);
                if (zone != ZoneType::None) m_zoneGrid[row][col] = zone;
            }
            else if (isWall)
            {
                setTile(col, row, TileType::Wall);
                if (zone != ZoneType::None) m_zoneGrid[row][col] = zone;
            }
        }
    }
}

std::string TileMap::tilesetNameForGid(uint32_t gid) const
{
    for (const auto& r : m_tilesetRanges)
        if (gid >= (uint32_t)r.firstgid) return r.name;
    return "";
}

bool TileMap::loadFromTiled(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;
    auto j = nlohmann::json::parse(f);

    m_cols = j["width"].get<int>();
    m_rows = j["height"].get<int>();
    m_grid.assign(m_rows, std::vector<TileType>(m_cols, TileType::Wall));
    m_zoneGrid.assign(m_rows, std::vector<ZoneType>(m_cols, ZoneType::None));

    m_tilesetRanges.clear();
    if (j.contains("tilesets"))
    {
        for (const auto& ts : j["tilesets"])
        {
            int fg = (ts.contains("firstgid") && ts["firstgid"].is_number())
                     ? ts["firstgid"].get<int>() : 1;
            std::string src = (ts.contains("source") && ts["source"].is_string())
                     ? ts["source"].get<std::string>() : "";
            m_tilesetRanges.push_back({fg, basenameNoExt(src)});
        }
        std::sort(m_tilesetRanges.begin(), m_tilesetRanges.end(),
            [](const TilesetRange& a, const TilesetRange& b){ return a.firstgid > b.firstgid; });
    }

    processLayers(j["layers"]);

    // วาง Gate อัตโนมัติถ้าไม่มี objectgroup
    bool hasGateUp = false, hasGateDown = false;
    for (int r = 0; r < m_rows && (!hasGateUp || !hasGateDown); ++r)
        for (int c = 0; c < m_cols && (!hasGateUp || !hasGateDown); ++c)
        {
            if (m_grid[r][c] == TileType::GateUp)   hasGateUp   = true;
            if (m_grid[r][c] == TileType::GateDown)  hasGateDown = true;
        }

    if (!hasGateUp || !hasGateDown)
    {
        int upC = -1, upR = -1, dnC = -1, dnR = -1;
        float maxDist = 0.f;
        float cx = m_cols / 2.f, cy = m_rows / 2.f;
        for (int r = 1; r < m_rows - 1; ++r)
            for (int c = 1; c < m_cols - 1; ++c)
                if (m_grid[r][c] == TileType::Floor)
                {
                    if (upC == -1) { upC = c; upR = r; }
                    float d = std::abs(c - cx) + std::abs(r - cy);
                    if (d > maxDist) { maxDist = d; dnC = c; dnR = r; }
                }
        if (!hasGateUp  && upC != -1) setTile(upC, upR, TileType::GateUp);
        if (!hasGateDown && dnC != -1) setTile(dnC, dnR, TileType::GateDown);
    }

    return true;
}

