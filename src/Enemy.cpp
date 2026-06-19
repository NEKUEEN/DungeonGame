#include "Enemy.hpp"
#include "TileMap.hpp"
#include "MonsterDB.hpp"
#include "SkillDB.hpp"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <functional>
#include "StatusEffect.hpp"
#include "TextureManager.hpp"

// ============================================================
//  Constructor
// ============================================================
Enemy::Enemy(const std::string& monsterId, int col, int row,
             int tileSize, int floor)
    : m_id(monsterId), m_col(col), m_row(row), m_tileSize(tileSize)
{
    const MonsterData* data = MonsterDB::instance().get(monsterId);
    if (data)
    {
        m_name   = data->name;
        m_family = data->family;
        if      (data->rank == "Boss")  m_rank = EnemyRank::Boss;
        else if (data->rank == "Elite") m_rank = EnemyRank::Elite;
        else                            m_rank = EnemyRank::Normal;

        if      (data->aiType == "ranged") m_aiType = EnemyAIType::Ranged;
        else if (data->aiType == "caster") m_aiType = EnemyAIType::Caster;
        else if (data->aiType == "coward") m_aiType = EnemyAIType::Coward;
        else                               m_aiType = EnemyAIType::Melee;

        m_preferredRange = data->preferredRange;
        m_alertRange     = data->alertRange;
        m_spd            = data->spd;

        m_slashResist  = data->slashResist;
        m_pierceResist = data->pierceResist;
        m_bluntResist  = data->bluntResist;
        m_cleaveResist = data->cleaveResist;

        float scale = 1.f + (floor - 1) * 0.1f;
        m_maxHp   = (int)(data->hp      * scale);
        m_attack  = (int)(data->attack  * scale);
        m_defense = (int)(data->defense * scale);
        m_exp     = data->exp;
        m_hp      = m_maxHp;

        for (const auto& sid : data->skills)
        {
            const SkillData* sd = SkillDB::instance().get(sid);
            if (sd) { SkillInstance inst; inst.data = *sd; m_skills.push_back(inst); }
        }
        loadSprite(data->sprite);
    }
    else
    {
        std::cerr << "[Enemy] Not found: " << monsterId << "\n";
        m_name = "Unknown"; m_maxHp = 10; m_hp = 10;
        m_attack = 3; m_defense = 0; m_exp = 1;
    }
}

void Enemy::loadSprite(const std::string& spriteName)
{
    std::string path = "assets/textures/" + spriteName;
    if (m_texture.loadFromFile(path))
    {
        m_sprite = new sf::Sprite(m_texture);
        auto sz  = m_texture.getSize();
        float sc = (float)m_tileSize / std::max(sz.x, sz.y);
        m_sprite->setScale({sc, sc});
        m_hasSprite = true;
    }
}

SkillInstance* Enemy::findSkill(const std::string& id)
{
    for (auto& sk : m_skills) if (sk.data.id == id) return &sk;
    return nullptr;
}

bool Enemy::isTileOccupied(int c, int r, const std::vector<Enemy*>& others) const
{
    for (auto* o : others)
        if (o != this && !o->isDead() && o->m_col == c && o->m_row == r)
            return true;
    return false;
}

bool Enemy::hasLineOfSight(int x0, int y0, int x1, int y1,
                            const TileMap& map) const
{
    int dx = std::abs(x1-x0), dy = std::abs(y1-y0);
    int sx = x0<x1?1:-1, sy = y0<y1?1:-1;
    int err = dx-dy, cx = x0, cy = y0;
    while (true)
    {
        if (cx==x1 && cy==y1) return true;
        if (!(cx==x0&&cy==y0) && map.getTile(cx,cy)==TileType::Wall)
            return false;
        int e2 = 2*err;
        if (e2>-dy){err-=dy; cx+=sx;}
        if (e2< dx){err+=dx; cy+=sy;}
    }
}
// ============================================================
//  A*  — 8-directional, obstacle = Wall
//  คืน {nextCol, nextRow} หรือ {-1,-1} ถ้าหาไม่เจอ
// ============================================================
std::pair<int,int> Enemy::astar(int fc, int fr, int tc, int tr,
                                 const TileMap& map) const
{
    if (fc == tc && fr == tr) return {-1, -1};

    // encode pos
    auto enc = [&](int c, int r){ return r * 200 + c; };

    struct Node { int c, r; float f; };
    auto cmp = [](const Node& a, const Node& b){ return a.f > b.f; };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> open(cmp);

    std::unordered_map<int, float> gCost;
    std::unordered_map<int, int>   parent; // value = encoded parent

    auto h = [&](int c, int r){
        return (float)(std::abs(c - tc) + std::abs(r - tr));
    };

    int startKey = enc(fc, fr);
    gCost[startKey] = 0;
    parent[startKey] = -1;
    open.push({fc, fr, h(fc, fr)});

    const int dx[] = {0,0,1,-1, 1, 1,-1,-1};
    const int dy[] = {1,-1,0,0, 1,-1, 1,-1};
    const float cost[] = {1,1,1,1, 1.4f,1.4f,1.4f,1.4f};

    // จำกัด node ที่ explore เพื่อประหยัด CPU
    int visited = 0;
    while (!open.empty() && visited < 400)
    {
        auto cur = open.top(); open.pop();
        visited++;

        if (cur.c == tc && cur.r == tr)
        {
            // trace back หา step แรก
            int key = enc(tc, tr);
            while (true)
            {
                int pk = parent[key];
                if (pk == startKey || pk == -1) break;
                key = pk;
            }
            // key = first step (หรือ goal ถ้าติดกัน)
            int nc = key % 200;
            int nr = key / 200;
            if (key == enc(tc, tr) && parent[key] == startKey)
                return {tc, tr};
            return {nc, nr};
        }

        float g = gCost[enc(cur.c, cur.r)];
        for (int i = 0; i < 8; ++i)
        {
            int nc = cur.c + dx[i];
            int nr = cur.r + dy[i];
            if (!map.isWalkable(nc, nr)) continue;
            int nk = enc(nc, nr);
            float ng = g + cost[i];
            if (gCost.find(nk) == gCost.end() || ng < gCost[nk])
            {
                gCost[nk] = ng;
                parent[nk] = enc(cur.c, cur.r);
                open.push({nc, nr, ng + h(nc, nr)});
            }
        }
    }
    return {-1, -1}; // ไปไม่ได้
}

bool Enemy::stepToward(int toC, int toR, const TileMap& map,
                        const std::vector<Enemy*>& others)
{
    auto [nc, nr] = astar(m_col, m_row, toC, toR, map);
    if (nc == -1) return false;
    if (isTileOccupied(nc, nr, others)) return false;
    m_col = nc; m_row = nr;
    return true;
}

// ============================================================
//  updateAI
// ============================================================
bool Enemy::updateAI(int pc, int pr,
                     const TileMap& map,
                     const std::vector<Enemy*>& others)
{
    if (isDead()) return false;

    // Stun → skip turn
    if (hasStatus(StatusType::Stun)) return false;

// Slow → ทำให้ spdCounter ลดก่อน act
    if (hasStatus(StatusType::Slow)) {
        m_spdCounter -= 50;
    if (m_spdCounter < 0) m_spdCounter = 0;
    }

    float dist = std::hypot((float)(pc - m_col), (float)(pr - m_row));

    // ── 1. Alert check: ต้องเห็นกันก่อน ──
    if (!m_alerted)
    {
        if (dist <= (float)m_alertRange)
        {
            m_alerted      = true;
            m_lastKnownCol = pc;
            m_lastKnownRow = pr;
            m_searching    = false;
        }
        else return false;  // ยังไม่เคยเห็น → sleep
    }

    

    // ── 2. อัพเดท last known ถ้ายังเห็นอยู่ ──
    if (dist <= (float)(m_alertRange + 3))
    {
        m_lastKnownCol = pc;
        m_lastKnownRow = pr;
        m_searching    = false;
    }

    // ── 3. Skill check (Ranged/Caster) ──
    if (m_aiType == EnemyAIType::Ranged || m_aiType == EnemyAIType::Caster)
{
    bool hasLOS = hasLineOfSight(m_col, m_row, pc, pr, map);

    // ไม่มี LOS → ตามหาก่อนเลย ไม่ต้องเช็คอย่างอื่น
    if (!hasLOS)
        return stepToward(m_lastKnownCol, m_lastKnownRow, map, others);

    // มี LOS → ลองยิง
    m_attackTimer++;
    if (m_attackTimer >= m_attackInterval)
{
    m_attackTimer = 0;
    for (auto& sk : m_skills)
    {
        if (!sk.isReady()) continue;
        if ((sk.data.type == SkillType::ActiveRanged ||
             sk.data.type == SkillType::ActiveAoe)
            && dist <= (float)sk.data.effect.range)
        {
            m_pendingSkill = PendingSkill{sk.data.id, pc, pr};
            sk.cooldownLeft = sk.data.cooldown;
            return true;
        }
    }
}

    // มี LOS แต่ไกลเกิน range → เข้าใกล้
    if (dist > (float)(m_preferredRange + 2))
        return stepToward(m_lastKnownCol, m_lastKnownRow, map, others);

    // มี LOS อยู่ใน range แต่ skill cooldown → รอ/strafe
    return false;
}
    
    // ── 4. Coward ──
    if (m_aiType == EnemyAIType::Coward && m_hp < m_maxHp * 3 / 10)
    {
        // หนีออกจาก player โดย A* ไปทิศตรงข้าม
        int fleeC = m_col + (m_col - pc);
        int fleeR = m_row + (m_row - pr);
        return stepToward(fleeC, fleeR, map, others);
    }

    // ── 5. Melee — ถ้าอยู่ติดกันแล้ว Game จัดการ combat ──
    int dx = std::abs(m_col - pc), dy = std::abs(m_row - pr);
    if (dx <= 1 && dy <= 1) return false;

    // ── 6. เดินหา lastKnown ด้วย A* ──
    if (m_lastKnownCol != -1)
    {
        // ถึง lastKnown แล้ว แต่ยังไม่เจอ player → search mode
        int dlc = std::abs(m_col - m_lastKnownCol);
        int dlr = std::abs(m_row - m_lastKnownRow);
        if (dlc <= 1 && dlr <= 1 && dist > (float)(m_alertRange + 3))
        {
            m_searching = true;
        }

        if (m_searching)
        {
            // วนเดิน 8 ทิศสุ่มๆ รอบๆ lastKnown
            m_searchTimer++;
            if (m_searchTimer > 20)
            {
                // หยุด search หลัง 20 turn
                m_alerted   = false;
                m_searching = false;
                m_searchTimer = 0;
                return false;
            }
            const int sdx[] = {0,0,1,-1,1,1,-1,-1};
            const int sdy[] = {1,-1,0,0,1,-1,1,-1};
            // ลองเดินสุ่มสักทิศ
            static thread_local std::mt19937 rng{std::random_device{}()};
            int start = std::uniform_int_distribution<int>(0,7)(rng);
            for (int i = 0; i < 8; ++i)
            {
                int idx = (start + i) % 8;
                int nc = m_col + sdx[idx];
                int nr = m_row + sdy[idx];
                if (map.isWalkable(nc, nr) && !isTileOccupied(nc, nr, others))
                {
                    m_col = nc; m_row = nr;
                    return true;
                }
            }
            return false;
        }

        return stepToward(m_lastKnownCol, m_lastKnownRow, map, others);
    }

    return false;
}

// ============================================================
//  Render
// ============================================================
void Enemy::render(sf::RenderWindow& window)
{
    if (isDead()) return;
    float px = (float)(m_col * m_tileSize), py = (float)(m_row * m_tileSize);
    float ts = (float)m_tileSize;

    if (m_hasSprite && m_sprite)
    {
        m_sprite->setColor(rankColor());
        m_sprite->setPosition({px, py});
        window.draw(*m_sprite);
    }
    else
    {
        float sz = m_rank==EnemyRank::Boss?0.85f:m_rank==EnemyRank::Elite?0.72f:0.6f;
        sf::RectangleShape body({ts*sz, ts*(sz+0.05f)});
        body.setFillColor(sf::Color(100,140,80));
        body.setOutlineColor(rankColor());
        body.setOutlineThickness(m_rank==EnemyRank::Normal?1.f:2.f);
        body.setPosition({px+ts*(1.f-sz)/2.f, py+ts*0.08f});
        window.draw(body);
    }
    drawHPBar(window);
}

void Enemy::drawHPBar(sf::RenderWindow& window)
{
    float px=(float)(m_col*m_tileSize), py=(float)(m_row*m_tileSize), ts=(float)m_tileSize;
    float bw=ts-4.f, bh=3.f, bx=px+2.f, by=py+ts-5.f;
    sf::RectangleShape bg({bw,bh}); bg.setFillColor(sf::Color(60,0,0)); bg.setPosition({bx,by}); window.draw(bg);
    float r=std::clamp((float)m_hp/m_maxHp,0.f,1.f);
    sf::Color c=m_rank==EnemyRank::Boss?sf::Color(255,150,0):m_rank==EnemyRank::Elite?sf::Color(180,80,220):sf::Color(200,60,60);
    sf::RectangleShape bar({bw*r,bh}); bar.setFillColor(c); bar.setPosition({bx,by}); window.draw(bar);
    if (m_alerted)
    {
        sf::RectangleShape dot({3.f,3.f});
        dot.setFillColor(m_searching ? sf::Color(255,200,0) : sf::Color(255,50,50));
        dot.setPosition({bx+bw-4.f, by-4.f});
        window.draw(dot);
    }

    // ── Debuff icons เหนือ HP bar ──
    if (m_statusEffects.empty()) return;

    struct DebuffInfo { StatusType type; const char* key; sf::Color fallback; };
    const DebuffInfo debuffs[] = {
        { StatusType::Bleed,  "debuff_e_bleed",  sf::Color(200,  0,  0) },
        { StatusType::Poison, "debuff_e_poison", sf::Color( 80, 180, 60) },
        { StatusType::Burn,   "debuff_e_burn",   sf::Color(255, 140,  0) },
        { StatusType::Stun,   "debuff_e_stun",   sf::Color(255, 230,  0) },
        { StatusType::Slow,   "debuff_e_slow",   sf::Color( 80, 160, 255) },
    };

    const float SZ  = 10.f;
    const float GAP = 2.f;
    float ix = bx;
    float iy = by - SZ - 3.f;

    auto& tm = TextureManager::instance();

    for (const auto& info : debuffs)
    {
        if (!hasStatus(info.type)) continue;

        const sf::Texture* tex = tm.get(info.key);
        if (tex)
        {
            sf::Sprite spr(*tex);
            auto sz = tex->getSize();
            float sc = SZ / std::max((float)sz.x, (float)sz.y);
            spr.setScale({sc, sc});
            spr.setPosition({ix, iy});
            window.draw(spr);
        }
        else
        {
            sf::RectangleShape icon({SZ, SZ});
            icon.setFillColor(info.fallback);
            icon.setPosition({ix, iy});
            window.draw(icon);
        }
        ix += SZ + GAP;
    }
}

void Enemy::applyStatus(const StatusEffect& se)
{
    // ถ้ามีอยู่แล้ว → refresh duration (ไม่ stack)
    for (auto& existing : m_statusEffects)
        if (existing.type == se.type)
        { existing.duration = std::max(existing.duration, se.duration);
          existing.power    = se.power; return; }
    m_statusEffects.push_back(se);
}

void Enemy::tickStatusEffects(int& hpDelta, std::string& effectName)
{
    hpDelta = 0;
    effectName = "";
    for (auto& se : m_statusEffects) {
        switch(se.type) {
            case StatusType::Bleed:
                hpDelta -= se.power;
                effectName = "bleed";
                break;
            case StatusType::Poison:
                hpDelta -= se.power;
                effectName = "poison";
                break;
            case StatusType::Burn:
                hpDelta -= se.power;
                effectName = "burn";
                break;
            case StatusType::Regen:
                hpDelta += se.power;
                break;
                // เพิ่ม case ใน switch
            case StatusType::Stun:
                effectName = "stun";
                break;
            case StatusType::Slow:
                effectName = "slow";
                break;
            default: break;
        }
    }
    // ส่วนที่เหลือเหมือนเดิม...
    m_hp += hpDelta;
    m_hp  = std::clamp(m_hp, 0, m_maxHp);

    // tick และลบ effect ที่หมดอายุ
    m_statusEffects.erase(
        std::remove_if(m_statusEffects.begin(), m_statusEffects.end(),
            [](StatusEffect& se){ return !se.tick(); }),
        m_statusEffects.end());
}

bool Enemy::hasStatus(StatusType type) const {
    for (const auto& se : m_statusEffects)
        if (se.type == type && se.isActive()) return true;
    return false;
}

sf::Color Enemy::rankColor() const
{
    switch(m_rank)
    {
        case EnemyRank::Boss:  return sf::Color(255,200,80);
        case EnemyRank::Elite: return sf::Color(200,100,255);
        default:               return sf::Color::White;
    }
}

// ── patch: replace drawHPBar ──