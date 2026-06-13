#include "Game.hpp"
#include "TextureManager.hpp"
#include "MonsterDB.hpp"
#include "DropTable.hpp"
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <optional>
#include <cmath>
#include <chrono>

// ============================================================
//  Constructor / Destructor
// ============================================================
Game::Game()
    : m_window(sf::VideoMode({WINDOW_W, WINDOW_H}), "Dungeon and Stone",
               sf::Style::None)
    , m_tileMap(MAP_COLS, MAP_ROWS, TILE_SIZE)
    , m_fog(MAP_COLS, MAP_ROWS)
    , m_player(nullptr)
{
    m_window.setFramerateLimit(60);
    m_fontLoaded = m_font.openFromFile("assets/fonts/font.ttf");
    if (!m_fontLoaded) std::cerr << "[Game] Warning: font not loaded\n";

    TextureManager::instance().loadAll();
    MonsterDB::instance().load("assets/data/monsters.json");
    DropTable::instance().loadItems("assets/data/items.json");
    SkillDB::instance().load("assets/data/skills.json");
    RaceDB::instance().load("assets/data/races.json");
    std::cerr << "[RaceDB] loaded: " << RaceDB::instance().getAll().size() << " races\n";

    m_gameView.setSize({(float)GAME_VIEW_W*1.5f,(float)GAME_VIEW_H*1.5f});
    m_uiView.setSize({(float)WINDOW_W,(float)WINDOW_H});
    m_uiView.setCenter({(float)WINDOW_W/2.f,(float)WINDOW_H/2.f});
    applyLetterbox();

    newDungeon(false);
}

Game::~Game() { delete m_player; clearEnemies(); }

// ============================================================
//  Stats Helpers
// ============================================================
int Game::getItemLevelTotal() const
{
    int total = 0;
    const EquipSlot slots[] = {
        EquipSlot::Head, EquipSlot::Body, EquipSlot::Arms,
        EquipSlot::Legs, EquipSlot::Feet,
        EquipSlot::MainHand, EquipSlot::OffHand
    };
    for (auto slot : slots)
    {
        const Item* item = m_equipment.getItem(slot);
        if (item) total += item->value;
    }
    return total;
}

void Game::drainMentality()
{
    // ระบบ drain mentality ถูกปิดไว้
}

// ============================================================
//  recalcAllStats – รวมศูนย์คำนวณสเตตทั้งหมด
// ============================================================
void Game::recalcAllStats()
{
    if (!m_player) return;
    Stats& base = m_player->getStats();
    
    int oldMaxHp   = base.maxHp;
    int oldMaxMana = base.maxMana;
    
    StatBonus bonus = m_equipment.getTotalBonus() + m_coreSlots.getTotalBonus();

    int atkPercentBonus = 0, defPercentBonus = 0, magicPercentBonus = 0, hpPercentBonus = 0;
    for (const auto& sk : m_player->getSkills())
    {
        if (sk.data.type == SkillType::Passive ||
            (sk.data.type == SkillType::ActiveBuff && sk.buffActive))
        {
            atkPercentBonus += sk.data.effect.atkPct;
            defPercentBonus += sk.data.effect.defPct;
            magicPercentBonus += sk.data.effect.matkPct;
            hpPercentBonus += sk.data.effect.hpPct;

        }
    }

    // ใช้ baseMaxHp และ baseMaxMana แทน maxHp/maxMana
    m_finalStats.maxHp   = base.baseMaxHp + bonus.hp;
    m_finalStats.maxHp += m_finalStats.maxHp * hpPercentBonus / 100;
    m_finalStats.atk     = base.maxAtk + bonus.atk;
    m_finalStats.atk    += m_finalStats.atk * atkPercentBonus / 100;

    m_finalStats.def     = base.maxDef + bonus.def;
    m_finalStats.def    += m_finalStats.def * defPercentBonus / 100;

    m_finalStats.dodge   = base.maxDodge + bonus.dodge;
    m_finalStats.maxMana = base.baseMaxMana + bonus.mana;
    m_finalStats.matk    = base.maxMagicDmg + bonus.matk;
    m_finalStats.matk   += m_finalStats.matk * magicPercentBonus / 100;
    m_finalStats.mdef    = base.maxMagicRes + bonus.magicRes;
    m_finalStats.spd     = base.maxSpd + bonus.spd;
    m_finalStats.maxStamina  = 100;
    m_finalStats.staminaRegen = 1;
    m_finalStats.itemLevel = getItemLevelTotal();

    m_finalStats.body = (m_finalStats.maxHp * 0.4f + m_finalStats.atk * 0.3f +
                              m_finalStats.def * 0.2f + m_finalStats.dodge * 0.1f);
    m_finalStats.mentality = (m_finalStats.maxMana * 0.4f + m_finalStats.matk * 0.3f +
                                   m_finalStats.mdef * 0.3f);
    float bonusPct = 0.30f + (base.level - 1) * 0.05f;
    m_finalStats.battleIndex = (m_finalStats.body + m_finalStats.mentality +
                                     m_finalStats.itemLevel * bonusPct);

    if (m_finalStats.maxHp > oldMaxHp)
        base.hp += (m_finalStats.maxHp - oldMaxHp);
    if (base.hp > m_finalStats.maxHp) base.hp = m_finalStats.maxHp;
    
    if (m_finalStats.maxMana > oldMaxMana)
        base.mana += (m_finalStats.maxMana - oldMaxMana);
    if (base.mana > m_finalStats.maxMana) base.mana = m_finalStats.maxMana;

    // อัปเดต base stats — ไม่แตะ baseMaxHp และ baseMaxMana!
    base.maxHp        = m_finalStats.maxHp;
    base.maxMana      = m_finalStats.maxMana;
    base.body         = m_finalStats.body;
    base.maxMentality = m_finalStats.mentality;
    // reset mentality เฉพาะตอนที่ยังไม่ถูก drain (ไม่ทับค่าที่กำลัง drain อยู่)
    if (base.mentality > m_finalStats.mentality) base.mentality = m_finalStats.mentality;
    base.itemLevel    = m_finalStats.itemLevel;
    base.battleIndex  = m_finalStats.battleIndex;
}

void Game::refreshStats()
{
    if (!m_player) return;

    // เก็บ finalStats เก่าก่อน recalc
    int oldBody        = m_finalStats.body;
    int oldMentality   = m_finalStats.mentality;
    int oldBattle      = m_finalStats.battleIndex;

    recalcAllStats();

    int db = m_finalStats.body        - oldBody;
    int dm = m_finalStats.mentality   - oldMentality;
    int di = m_finalStats.battleIndex - oldBattle;

    if (db != 0 || dm != 0 || di != 0)
    {
        m_deltaBody        = db;
        m_deltaMentality   = dm;
        m_deltaBattleIndex = di;
        m_deltaTimer = 180;
    }
}

// ============================================================
//  AP Helpers
// ============================================================
// ============================================================
//  recalcSpeed – คำนวณ spd สุทธิของผู้เล่น (ใหม่)
//
//  ระบบใหม่: spdCounter สะสมทุกเทิร์น ถ้า >= 100 → ขยับได้
//  spd=0  → +100/turn (ขยับทุกเทิร์น, ปกติ)
//  spd=+1 → +200/turn (เร็ว 2x — ขยับได้ 2 ครั้งต่อเทิร์นศัตรู)
//  spd=+2 → +300/turn (เร็ว 3x)
//  spd=-1 → +50/turn  (ช้า 2x — ขยับได้ทุก 2 เทิร์น)
//  spd=-2 → +33/turn  (ช้า 3x)
//  spd=-3 → +25/turn  (ช้า 4x)
// ============================================================
void Game::recalcSpeed()
{
    if (!m_player) return;
    Stats& s = m_player->getStats();

    // รวม spd จาก equipment + passive/active buff skills
    int spd = m_finalStats.spd;
    for (const auto& sk : m_player->getSkills())
    {
        if (!sk.buffActive && !sk.isPassive()) continue;
        spd += sk.data.effect.speedBonus;
    }
    s.spd = std::clamp(spd, -4, 4);  // cap ที่ -4 ถึง +4

    // คำนวณ baseSpeed ต่อเทิร์น (เหมือนสูตรในมอน)
    int baseSpeed = 100;
    if (s.spd >= 0) baseSpeed = 100 + s.spd * 100;
    else            baseSpeed = std::max(10, 100 / (1 - s.spd));
    s.speedPerTurn = baseSpeed;

    // Stamina regen จาก base + equipment bonus (ขยายได้ทีหลัง)
    s.staminaRegen = m_finalStats.staminaRegen;
    if (s.maxStamina == 0) s.maxStamina = m_finalStats.maxStamina;
    if (s.stamina > s.maxStamina) s.stamina = s.maxStamina;
}

void Game::regenStamina()
{
    if (!m_player) return;
    Stats& s = m_player->getStats();
    s.stamina = std::min(s.maxStamina, s.stamina + s.staminaRegen);
}


// ============================================================
//  Skill Helpers
// ============================================================
int Game::getScaledDamage(const SkillEffect& effect) const
{
    if (!m_player) return 0;
    const Stats& s = m_player->getStats();

    int baseStat = 0;
    if (effect.scalingStat == "atk")
        baseStat = getBuffedAtk();
    else if (effect.scalingStat == "def")
        baseStat = getBuffedDef();
    else if (effect.scalingStat == "dodge")
        baseStat = s.maxDodge + m_equipment.getTotalDodgeBonus();
    else if (effect.scalingStat == "magic_dmg")
        baseStat = getBuffedMagicDmg();
    else
        baseStat = getBuffedAtk();

    return std::max(1, baseStat * effect.damagePct / 100);
}

void Game::useSkillBuff(const std::string& skillId)
{
    if (!m_player) return;
    SkillInstance* sk = m_player->findSkill(skillId);
    if (!sk || sk->data.type != SkillType::ActiveBuff) return;
    if (!sk->isReady())
    {
        addLog("  "+sk->data.name+" cooldown: "+std::to_string(sk->cooldownLeft)+" turns",
               sf::Color(160,160,160));
        return;
    }
    sk->buffActive   = true;
    sk->durationLeft = sk->data.duration;
    sk->cooldownLeft = sk->data.cooldown;
    addLog("  "+sk->data.name+" activated! ATK +"+std::to_string(sk->data.effect.atkPct)+
           "% for "+std::to_string(sk->data.duration)+" turns", sf::Color(255,200,50));
    recalcSpeed();
}

// ============================================================
//  executeSkill
// ============================================================
void Game::executeSkill(int hotbarIdx)
{
    if (!m_player) return;
    const std::string& id = m_player->getHotbar(hotbarIdx);
    if (id.empty()) { addLog("  Slot "+std::to_string(hotbarIdx+1)+" empty."); return; }

    SkillInstance* sk = m_player->findSkill(id);
    if (!sk) return;

    Stats& s = m_player->getStats();

    if (s.mana < sk->data.effect.manaCost)
    {
        addLog("  Not enough mana! Need " + std::to_string(sk->data.effect.manaCost), sf::Color(100,100,220));
        return;
    }

    if (s.stamina < sk->data.effect.staminaCost)
    {
        addLog("  Not enough stamina! Need " + std::to_string(sk->data.effect.staminaCost) +
               " (have " + std::to_string(s.stamina) + ")", sf::Color(220,160,50));
        return;
    }

    if (!sk->isReady())
    {
        addLog("  "+sk->data.name+" cooldown: "+std::to_string(sk->cooldownLeft)+" turns", sf::Color(160,160,160));
        return;
    }

    switch (sk->data.type)
    {
    case SkillType::ActiveBuff:
        s.mana    -= sk->data.effect.manaCost;
        s.stamina -= sk->data.effect.staminaCost;
        sk->buffActive = true;
        sk->durationLeft = sk->data.duration;
        sk->cooldownLeft = sk->data.cooldown;
        addLog("  "+sk->data.name+" activated!", sf::Color(255,200,50));
        recalcAllStats();
        recalcSpeed();
        break;

    case SkillType::ActiveHeal:
        s.mana    -= sk->data.effect.manaCost;
        s.stamina -= sk->data.effect.staminaCost;
        {
            int heal = sk->data.effect.healFlat + s.maxHp * sk->data.effect.healPct / 100;
            s.hp = std::min(s.maxHp, s.hp + heal);
            sk->cooldownLeft = sk->data.cooldown;
            addLog("  "+sk->data.name+" +"+std::to_string(heal)+" HP", sf::Color(80,220,120));
        }
        break;

    case SkillType::ActiveRanged:
    case SkillType::ActiveWarp:
        s.mana -= sk->data.effect.manaCost;
        // stamina หักตอน confirm ยิงจริง (fireRangedAt / executeWarp) ไม่หักตอนเข้า targeting
        m_ui.targeting.active = true;
        m_ui.targeting.skillId = id;
        m_ui.targeting.targetCol = m_player->getCol();
        m_ui.targeting.targetRow = m_player->getRow();
        if (sk->data.type == SkillType::ActiveRanged)
            addLog(" [Targeting] "+sk->data.name+"", sf::Color(255,220,50));
        else
            addLog(" [Warp] "+sk->data.name+"", sf::Color(100,200,255));
        break;

    case SkillType::ActiveAoe:
        s.mana    -= sk->data.effect.manaCost;
        s.stamina -= sk->data.effect.staminaCost;
        executeAoe(sk);
        sk->cooldownLeft = sk->data.cooldown;
        break;

    case SkillType::Passive:
        addLog("  "+sk->data.name+" is passive.", sf::Color(120,200,120));
        break;
    }
}

void Game::executeAoe(SkillInstance* sk)
{
    if (!m_player || !sk) return;
    int pc = m_player->getCol();
    int pr = m_player->getRow();
    int radius = sk->data.effect.aoeRadius;
    int baseAtk = getBuffedAtk();
    int dmg = std::max(1, baseAtk * sk->data.effect.damagePct / 100);
    int hits = 0;

    for (auto* e : m_enemies)
    {
        if (e->isDead()) continue;
        int dx = e->getCol() - pc, dy = e->getRow() - pr;
        if (dx*dx + dy*dy <= radius*radius)
        {
            e->takeDamage(dmg);
            addLog("  "+sk->data.name+" hits "+e->getName()+" for "+std::to_string(dmg)+"!",
                   sf::Color(255,180,50));
            hits++;
        }
    }
    if (hits == 0)
        addLog("  "+sk->data.name+"  no targets in range.", sf::Color(140,140,140));

    {
        Stats& ps = m_player->getStats();
        ps.spdCounter -= 100;
        ps.spdCounter += ps.speedPerTurn;
        if (ps.spdCounter > 300) ps.spdCounter = 300;
    }

    processTurn(); tryRespawnEnemies();
    m_fog.compute(m_player->getCol(), m_player->getRow(), VIEW_RADIUS, m_tileMap);
}

void Game::executeWarp(int col, int row)
{
    if (!m_tileMap.isWalkable(col, row))
    {
        addLog("  Cannot warp there!");
        return;
    }
    m_player->setPos(col, row);
    SkillInstance* sk = m_player->findSkill(m_ui.targeting.skillId);
    if (sk)
    {
        sk->cooldownLeft = sk->data.cooldown;
        // หัก stamina ตอน confirm warp จริง
        m_player->getStats().stamina -= sk->data.effect.staminaCost;
    }
    addLog("  Warped!", sf::Color(100,200,255));
    m_fog.compute(col, row, VIEW_RADIUS, m_tileMap);
    updateCamera();

    {
        Stats& ps = m_player->getStats();
        ps.spdCounter -= 100;
        ps.spdCounter += ps.speedPerTurn;
        if (ps.spdCounter > 300) ps.spdCounter = 300;
    }

    processTurn();
    tryRespawnEnemies();
}

// ============================================================
//  Line Helper
// ============================================================
std::vector<sf::Vector2i> Game::getLine(int x0, int y0, int x1, int y1) const
{
    std::vector<sf::Vector2i> tiles;
    int dx = std::abs(x1-x0), dy = std::abs(y1-y0);
    int sx = x0<x1?1:-1, sy = y0<y1?1:-1;
    int err = dx-dy;
    int cx = x0, cy = y0;
    while (true)
    {
        tiles.push_back({cx, cy});
        if (cx==x1 && cy==y1) break;
        int e2 = 2*err;
        if (e2 > -dy){ err -= dy; cx += sx; }
        if (e2 <  dx){ err += dx; cy += sy; }
    }
    return tiles;
}

// ============================================================
//  Targeting Mode
// ============================================================
void Game::enterTargetingMode()
{
    SkillInstance* sk = m_player->findSkill("stone_throw");
    if (!sk || !sk->isReady()) {
        addLog("  Stone Throw not ready", sf::Color(160,160,160));
        return;
    }
    m_ui.targeting.active = true;
    m_ui.targeting.skillId = "stone_throw";
    m_ui.targeting.targetCol = m_player->getCol();
    m_ui.targeting.targetRow = m_player->getRow();
    addLog(" [Targeting] Move cursor: arrow keys | Enter=fire | Esc=cancel", sf::Color(255,220,50));
}

void Game::moveTargetCursor(int dc, int dr)
{
    if (!m_player) return;
    SkillInstance* sk = m_player->findSkill(m_ui.targeting.skillId);
    if (!sk) return;

    int nc = m_ui.targeting.targetCol + dc;
    int nr = m_ui.targeting.targetRow + dr;
    float dist = std::sqrt((float)((nc - m_player->getCol())*(nc - m_player->getCol()) + 
                                   (nr - m_player->getRow())*(nr - m_player->getRow())));
    if (dist > sk->data.effect.range) {
        addLog("  Out of range (max " + std::to_string(sk->data.effect.range) + " tiles)", sf::Color(220,100,50));
        return;
    }
    nc = std::clamp(nc, 0, m_mapCols-1);
    nr = std::clamp(nr, 0, m_mapRows-1);
    m_ui.targeting.targetCol = nc;
    m_ui.targeting.targetRow = nr;
}

void Game::confirmTarget()
{
    if (!m_player) return;
    
    SkillInstance* sk = m_player->findSkill(m_ui.targeting.skillId);
    if (!sk)
    {
        cancelTargeting();
        return;
    }
    
    m_ui.targeting.active = false;
    
    if (sk->data.type == SkillType::ActiveRanged)
    {
        fireRangedAt(m_ui.targeting.targetCol, m_ui.targeting.targetRow);
    }
    else if (sk->data.type == SkillType::ActiveWarp)
    {
        executeWarp(m_ui.targeting.targetCol, m_ui.targeting.targetRow);
    }
}

void Game::cancelTargeting()
{
    m_ui.targeting.active = false;
    m_ui.targeting.skillId.clear();
    addLog("  Targeting cancelled.", sf::Color(140,140,140));
}

void Game::fireRangedAt(int targetCol, int targetRow)
{
    if (!m_player) return;
    
    SkillInstance* sk = m_player->findSkill(m_ui.targeting.skillId);
    if (!sk) return;

    // หัก stamina ตอน confirm ยิงจริง
    Stats& ps = m_player->getStats();
    ps.stamina -= sk->data.effect.staminaCost;

    sk->cooldownLeft = sk->data.cooldown;

    int pc = m_player->getCol();
    int pr = m_player->getRow();
    int projDmg = getScaledDamage(sk->data.effect);

    auto line = getLine(pc, pr, targetCol, targetRow);
    bool hit = false;

    for (int i = 1; i < (int)line.size(); ++i)
    {
        int tx = line[i].x;
        int ty = line[i].y;

        if (m_tileMap.getTile(tx, ty) == TileType::Wall) break;

        float dist = std::sqrt((float)((tx-pc)*(tx-pc) + (ty-pr)*(ty-pr)));
        if (dist > sk->data.effect.range) break;

        for (auto* e : m_enemies)
        {
            if (!e->isDead() && e->getCol() == tx && e->getRow() == ty)
            {
                e->takeDamage(projDmg);
                addLog("  " + sk->data.name + " hits " + e->getName() + " for " + std::to_string(projDmg) + "!",
                       sf::Color(180,220,255));

                if (e->isDead())
                {
                    //addLog("  "+e->getName()+" dead! +"+std::to_string(e->getExp())+" EXP", sf::Color(220,80,80));
                    
                    auto dropped = DropTable::instance().roll(e->getId());
                    for (const auto& itemId : dropped)
                    {
                        const ItemData* idata = DropTable::instance().getItem(itemId);
                        if (!idata) continue;
                        Item drop;
                        drop.id = idata->id;
                        drop.type = (idata->type=="Core") ? ItemType::Core : ItemType::Material;
                        drop.name = idata->name;
                        drop.desc = idata->desc;
                        drop.value = idata->value;
                        drop.hpBonus = idata->hpBonus;
                        drop.atkBonus = idata->atkBonus;
                        drop.defBonus = idata->defBonus;
                        drop.dodgeBonus = idata->dodgeBonus;
                        drop.manaBonus = idata->manaBonus;
                        drop.magicDmgBonus = idata->magicDmgBonus;
                        drop.magicResBonus = idata->magicResBonus;
                        drop.spdBonus = idata->spdBonus;
                        drop.spriteName = idata->sprite;
                        drop.stackable = idata->stackable;
                        drop.col = e->getCol();
                        drop.row = e->getRow();
                        m_mapItems.push_back(drop);
                        addLog("  Dropped: "+idata->name, sf::Color(180,220,255));
                    }

                    const std::string& eid = e->getId();
                        bool isFirstKill = (m_firstKillDone.find(eid) == m_firstKillDone.end());
                        int expGain = isFirstKill ? e->getExp() : 0;

                    if (isFirstKill) {
                        m_firstKillDone.insert(eid);
                        addLog("  [FIRST KILL] " + e->getName() + "! +" + std::to_string(expGain) + " EXP",
                        sf::Color(255, 220, 50));
}

                    if (expGain > 0 && m_player->addExp(expGain))
                    {
                        m_ui.levelUpFlash = true;
                        m_ui.levelUpTimer = 120;
                        int lv = m_player->getStats().level;
                        m_coreSlots.setSlotCount(lv);
                        refreshStats();
                        recalcSpeed();
                        addLog("  *** LEVEL UP! Level "+std::to_string(lv)+" ***", sf::Color(255,255,50));
                    }
                    onEnemyKilled(e);
                }
                hit = true;
                break;
            }
        }
        if (hit) break;
    }

    if (!hit)
        addLog("  " + sk->data.name + " hits nothing.", sf::Color(140,140,140));

    // หัก spdCounter เหมือนขยับ
    {
        Stats& ps = m_player->getStats();
        ps.spdCounter -= 100;
        ps.spdCounter += ps.speedPerTurn;
        if (ps.spdCounter > 300) ps.spdCounter = 300;
    }

    m_turnCount++;
    m_player->onTurnPassed();
    processTurn();
    tryRespawnEnemies();
    m_fog.compute(m_player->getCol(), m_player->getRow(), VIEW_RADIUS, m_tileMap);
}

// ============================================================
//  Dungeon Setup
// ============================================================
void Game::newDungeon(bool keepPlayer)
{
    if (m_dungeonFloor == 1)
    {
        if (!m_tileMap.loadFromTiled("assets/data/map_floor1.tmj"))
        {
            m_tileMap.generate();
        }
    }
    else
        m_tileMap.generate();

    m_mapCols = m_tileMap.getCols();
    m_mapRows = m_tileMap.getRows();
    m_fog = FogOfWar(m_mapCols, m_mapRows);

    m_fog.reset();
    m_playerDead = false;

    if (!keepPlayer)
    {
        delete m_player; m_player=nullptr;
        m_dungeonFloor=1;
        m_inventory=Inventory();
        m_equipment=Equipment();
        m_coreSlots=CoreSlots();
        m_familyKillCount.clear();
        m_bossActive.clear();
        m_firstKillDone.clear();
    }

    std::vector<std::pair<int,int>> floorTiles;
    for (int row=1;row<m_mapRows-1;++row)
        for (int col=1;col<m_mapCols-1;++col)
            if (m_tileMap.isWalkable(col,row))
                floorTiles.push_back({col,row});

    if (!floorTiles.empty())
    {
        auto [col,row] = floorTiles[
            std::uniform_int_distribution<int>(0,(int)floorTiles.size()-1)(m_rng)
        ];
        delete m_player;
        m_player = new Player(col, row, TILE_SIZE);
        if (!m_selectedRace.empty()) {
        const RaceData* race = RaceDB::instance().get(m_selectedRace);
        if (race) {
            Stats& s = m_player->getStats();
            s.baseMaxHp   += race->statBonus.hp;
            s.hp           = s.baseMaxHp;
            s.baseMaxMana += race->statBonus.mana;
            s.mana         = s.baseMaxMana;
            s.maxAtk      += race->statBonus.atk;
            s.maxDef      += race->statBonus.def;
            s.maxDodge    += race->statBonus.dodge;
            s.maxMagicDmg += race->statBonus.matk;
            s.maxMagicRes += race->statBonus.mdef;
            s.maxSpd      += race->statBonus.spd;
        // ใส่ start skill
        m_player->addCoreSkill(race->startSkill);
    if (!race->sprite.empty())
        m_player->setSprite(race->sprite);
        }
}
    }

    clearEnemies(); spawnEnemies(8+m_dungeonFloor);
    m_mapItems.clear(); spawnItems();
    m_respawnTimer=0;

    if (m_player)
    {
        recalcAllStats();
        recalcSpeed();
        m_coreSlots.setSlotCount(m_player->getStats().level);
        m_fog.compute(m_player->getCol(), m_player->getRow(), VIEW_RADIUS, m_tileMap);
    }

    updateCamera();
    m_ui.activePanel = UIState::Panel::Inventory;
    m_ui.selectedInvSlot = 0;
    m_ui.selectedEquipSlot = 0;
    m_ui.selectedCoreSlot = 0;
    m_ui.levelUpFlash = false;
    m_ui.targeting.reset();

    if (keepPlayer)
        addLog("  Floor "+std::to_string(m_dungeonFloor)+" - Deeper...", sf::Color(180,140,60));
    else
        addLog("  Floor 1", sf::Color(100,220,100));
}

void Game::applyLetterbox()
{
    auto winSize = m_window.getSize();
    float winW = (float)winSize.x;
    float winH = (float)winSize.y;
    float scale = std::min(winW / (float)WINDOW_W, winH / (float)WINDOW_H);
    float viewW = WINDOW_W * scale;
    float viewH = WINDOW_H * scale;
    float offX  = (winW - viewW) / 2.f;
    float offY  = (winH - viewH) / 2.f;

    m_gameView.setViewport(sf::FloatRect(
        {offX/winW, offY/winH},
        {viewW/winW * ((float)GAME_VIEW_W/WINDOW_W),
         viewH/winH * ((float)GAME_VIEW_H/WINDOW_H)}
    ));
    m_uiView.setViewport(sf::FloatRect(
        {offX/winW, offY/winH},
        {viewW/winW, viewH/winH}
    ));
}


void Game::updateCamera()
{
    if (!m_player) return;
    float px=(float)(m_player->getCol()*TILE_SIZE)+TILE_SIZE/2.f;
    float py=(float)(m_player->getRow()*TILE_SIZE)+TILE_SIZE/2.f;
    float hw=m_gameView.getSize().x/2.f,hh=m_gameView.getSize().y/2.f;
    px=std::clamp(px,hw,(float)(m_mapCols*TILE_SIZE)-hw);
    py=std::clamp(py,hh,(float)(m_mapRows*TILE_SIZE)-hh);
    m_gameView.setCenter({px,py});
}

// ============================================================
//  Enemy Spawn
// ============================================================
void Game::spawnEnemies(int count){ for(int i=0;i<count;++i) spawnEnemy(m_dungeonFloor); }

void Game::spawnEnemy(int floor)
{
    std::uniform_int_distribution<int> colDist(1, m_mapCols - 2);
    std::uniform_int_distribution<int> rowDist(1, m_mapRows - 2);

    for (int att = 0; att < 100; ++att)
    {
        int col = colDist(m_rng);
        int row = rowDist(m_rng);
        if (!m_tileMap.isWalkable(col, row)) continue;

        if (m_player)
        {
            int dx = col - m_player->getCol();
            int dy = row - m_player->getRow();
            if (dx*dx + dy*dy < 36) continue;
        }

        std::string id = pickRandomMonster(floor);
        if (id.empty()) return;

        m_enemies.push_back(new Enemy(id, col, row, TILE_SIZE, floor));
        return;
    }
}

std::string Game::pickRandomMonster(int floor)
{
    float wNormal = std::max(30.f, 60.f - (floor - 1) * 3.f);
    float wRare   = 25.f;
    float wElite  = std::min(40.f, 12.f + (floor - 1) * 2.f);
    float total   = wNormal + wRare + wElite;

    float roll = m_fDist(m_rng) * total;

    std::string rankStr;
    if      (roll < wNormal)          rankStr = "Normal";
    else if (roll < wNormal + wRare)  rankStr = "Rare";
    else                              rankStr = "Elite";

    auto ids = MonsterDB::instance().getByRank(rankStr);
    if (ids.empty()) ids = MonsterDB::instance().getByRank("Normal");
    if (ids.empty()) return "";

    std::uniform_int_distribution<int> pick(0, (int)ids.size() - 1);
    return ids[pick(m_rng)];
}

void Game::spawnBoss(const std::string& family)
{
    auto bossIds = MonsterDB::instance().getByRank("Boss");
    std::string bossId;
    for (const auto& bid : bossIds)
    {
        const MonsterData* d = MonsterDB::instance().get(bid);
        if (d && d->family == family) { bossId = bid; break; }
    }
    if (bossId.empty())
    {
        addLog("  [" + family + "] Boss not defined in JSON!", sf::Color(200,100,50));
        return;
    }

    std::uniform_int_distribution<int> colDist(1, m_mapCols - 2);
    std::uniform_int_distribution<int> rowDist(1, m_mapRows - 2);

    for (int att = 0; att < 300; ++att)
    {
        int col = colDist(m_rng);
        int row = rowDist(m_rng);
        if (!m_tileMap.isWalkable(col, row)) continue;
        if (m_player)
        {
            int dx = col - m_player->getCol();
            int dy = row - m_player->getRow();
            if (dx*dx + dy*dy < 64) continue;
        }
        m_enemies.push_back(new Enemy(bossId, col, row, TILE_SIZE, m_dungeonFloor));
        m_bossActive[family] = true;
        addLog("  *** " + family + " BOSS APPEARS! ***", sf::Color(255, 100, 50));
        addLog("  The slaughter has summoned a guardian!", sf::Color(255, 160, 50));
        return;
    }
}

void Game::onEnemyKilled(Enemy* enemy)
{
    if (!enemy) return;
    if (enemy->getRank() == EnemyRank::Boss)
    {
        m_bossActive[enemy->getFamily()] = false;
        addLog("  Boss defeated! The " + enemy->getFamily() +
               " threat fades...", sf::Color(255, 200, 80));
        return;
    }
    const std::string& fam = enemy->getFamily();
    m_familyKillCount[fam]++;
    int count = m_familyKillCount[fam];

    if (count % 10 == 0 && count < BOSS_KILL_THRESHOLD)
    {
        addLog("  " + fam + " kills: " + std::to_string(count) +
               "/" + std::to_string(BOSS_KILL_THRESHOLD),
               sf::Color(200, 160, 60));
    }
    if (count >= BOSS_KILL_THRESHOLD && !m_bossActive[fam])
    {
        spawnBoss(fam);
    }
}

void Game::tryRespawnEnemies()
{
    m_respawnTimer++;
    if (m_respawnTimer<RESPAWN_TURNS) return;
    m_respawnTimer=0;
    int cur=(int)m_enemies.size();
    if (cur>=MAX_ENEMIES) return;
    int toSpawn=std::min(2,MAX_ENEMIES-cur);
    for (int i=0;i<toSpawn;++i) spawnEnemy(m_dungeonFloor);
}

void Game::spawnItems()
{
    std::uniform_int_distribution<int> colDist(1, m_mapCols - 2);
    std::uniform_int_distribution<int> rowDist(1, m_mapRows - 2);
    std::uniform_int_distribution<int> typeDist(0, 9);
    std::uniform_int_distribution<int> armorDist(0, 4);

    int count = 10 + std::uniform_int_distribution<int>(0,7)(m_rng);
    int att = 0;

    while ((int)m_mapItems.size() < count && att < 300)
    {
        att++;
        int col = colDist(m_rng);
        int row = rowDist(m_rng);
        if (!m_tileMap.isWalkable(col, row)) continue;

        Item item;
        std::optional<std::string> itemId;
        int r = typeDist(m_rng);

        if      (r < 3) itemId = DropTable::instance().getRandomItemIdByType("Food");
        else if (r < 5) itemId = DropTable::instance().getRandomItemIdByType("Potion");
        else if (r < 7) itemId = DropTable::instance().getRandomItemIdByType("Weapon");
        else if (r < 8) itemId = DropTable::instance().getRandomItemIdByType("OffWeapon");
        else
        {
            const char* armorTypes[] = {"Helmet","BodyArmor","Gloves","Greaves","Boots"};
            itemId = DropTable::instance().getRandomItemIdByType(
                armorTypes[armorDist(m_rng)]);
        }

        if (itemId)
        {
            const ItemData* idata = DropTable::instance().getItem(*itemId);
            if (idata)
            {
                item.id   = idata->id;
                item.type = idata->type=="Core"      ? ItemType::Core      :
                            idata->type=="Material"  ? ItemType::Material  :
                            idata->type=="Food"      ? ItemType::Food      :
                            idata->type=="Potion"    ? ItemType::Potion    :
                            idata->type=="Helmet"    ? ItemType::Helmet    :
                            idata->type=="BodyArmor" ? ItemType::BodyArmor :
                            idata->type=="Gloves"    ? ItemType::Gloves    :
                            idata->type=="Greaves"   ? ItemType::Greaves   :
                            idata->type=="Boots"     ? ItemType::Boots     :
                            idata->type=="Weapon"    ? ItemType::Weapon    :
                            idata->type=="OffWeapon" ? ItemType::OffWeapon :
                                                       ItemType::Material;
                item.name          = idata->name;
                item.desc          = idata->desc;
                item.value         = idata->value;
                item.hpBonus       = idata->hpBonus;
                item.atkBonus      = idata->atkBonus;
                item.defBonus      = idata->defBonus;
                item.dodgeBonus    = idata->dodgeBonus;
                item.manaBonus     = idata->manaBonus;
                item.magicDmgBonus = idata->magicDmgBonus;
                item.magicResBonus = idata->magicResBonus;
                item.spdBonus      = idata->spdBonus;
                item.spriteName    = idata->sprite;
                item.stackable     = idata->stackable;
            }
        }

        if (item.name.empty())
        {
            if      (r < 3) item = Item::makeFood();
            else if (r < 5) item = Item::makePotion();
            else if (r < 7) item = Item::makeWeapon();
            else if (r < 8) item = Item::makeOffWeapon();
            else            item = Item::makeArmor();
        }

        item.col = col;
        item.row = row;
        m_mapItems.push_back(item);
    }
}

void Game::clearEnemies(){for(auto*e:m_enemies)delete e;m_enemies.clear();}

// ============================================================
//  Main Loop
// ============================================================
void Game::run(){while(m_window.isOpen()){processEvents();update();render();}}

void Game::update()
{
    if (m_ui.levelUpFlash)
    {
        m_ui.levelUpTimer--;
        if (m_ui.levelUpTimer <= 0) m_ui.levelUpFlash = false;
    }
    if (m_deltaTimer > 0) m_deltaTimer--;
}

// ============================================================
//  Input
// ============================================================
void Game::processEvents()
{
    while (const std::optional event = m_window.pollEvent())
    {
        if (event->is<sf::Event::Closed>()) m_window.close();
        if (m_inRaceSelect)
    {
        if (const auto* key = event->getIf<sf::Event::KeyPressed>())
        {
            int& sel = m_ui.selectedRace;
            int total = (int)RaceDB::instance().getAll().size(); // ไม่นับ ???
            if (key->code == sf::Keyboard::Key::A && sel % 3 > 0)       sel--;
            if (key->code == sf::Keyboard::Key::D && sel % 3 < 2 && sel+1 < total) sel++;
            if (key->code == sf::Keyboard::Key::W && sel - 3 >= 0)       sel -= 3;
            if (key->code == sf::Keyboard::Key::S && sel + 3 < total)    sel += 3;
            if (key->code == sf::Keyboard::Key::Enter)
            {
                auto& races = RaceDB::instance().getAll();
                if (sel < (int)races.size())
                {
                    m_selectedRace = races[sel].id;
                    m_inRaceSelect = false;
                    newDungeon(false);
                }
            }
        }
        return;
    }
        if (const auto* key = event->getIf<sf::Event::KeyPressed>())
        {
            // F11 toggle fullscreen
            if (key->code == sf::Keyboard::Key::F11)
            {
                m_isFullscreen = !m_isFullscreen;
                if (m_isFullscreen)
                    m_window.create(sf::VideoMode::getDesktopMode(), "Dungeon and Stone", sf::Style::None);
                else
                    m_window.create(sf::VideoMode({WINDOW_W, WINDOW_H}), "Dungeon and Stone", sf::Style::Titlebar | sf::Style::Close);
                m_window.setFramerateLimit(60);
                applyLetterbox();
                return;
            }
            if (m_playerDead)
            {
                if (key->code==sf::Keyboard::Key::R)
                m_inRaceSelect = true;
                m_ui.selectedRace = 0;
                break;
                if (key->code==sf::Keyboard::Key::Q) m_window.close();
                return;
            }

            if (m_ui.targeting.active)
            {
                switch(key->code)
                {
                    case sf::Keyboard::Key::K: moveTargetCursor(0,-1);  break;
                    case sf::Keyboard::Key::J: moveTargetCursor(0, 1);  break;
                    case sf::Keyboard::Key::H: moveTargetCursor(-1, 0); break;
                    case sf::Keyboard::Key::L: moveTargetCursor(1, 0);  break;
                    case sf::Keyboard::Key::Y: moveTargetCursor(-1,-1); break;
                    case sf::Keyboard::Key::U: moveTargetCursor(1,-1);  break;
                    case sf::Keyboard::Key::B: moveTargetCursor(-1, 1); break;
                    case sf::Keyboard::Key::N: moveTargetCursor(1,  1); break;
                    case sf::Keyboard::Key::Num1: 
                    case sf::Keyboard::Key::Num2:
                    case sf::Keyboard::Key::Num3:
                    case sf::Keyboard::Key::Num4:
                    case sf::Keyboard::Key::Num5:
                    case sf::Keyboard::Key::Num6:
                    case sf::Keyboard::Key::Num7:
                    case sf::Keyboard::Key::Num8:
                    case sf::Keyboard::Key::Num9:   
                    confirmTarget(); break;
                    case sf::Keyboard::Key::Escape:
                    case sf::Keyboard::Key::F:     cancelTargeting(); break;
                    default: break;
                }
                return;
            }

            switch(key->code)
            {
                case sf::Keyboard::Key::Escape: m_window.close();    break;
                
                case sf::Keyboard::Key::K:  handlePlayerMove(0,-1);  break;
                
                case sf::Keyboard::Key::J:  handlePlayerMove(0, 1);  break;
                
                case sf::Keyboard::Key::H:  handlePlayerMove(-1, 0); break;
                
                case sf::Keyboard::Key::L:  handlePlayerMove(1,  0); break;
                case sf::Keyboard::Key::Y:  handlePlayerMove(-1,-1); break;
                case sf::Keyboard::Key::U:  handlePlayerMove(1,-1);  break;
                case sf::Keyboard::Key::B:  handlePlayerMove(-1, 1); break;
                case sf::Keyboard::Key::N:  handlePlayerMove(1,  1); break;

                case sf::Keyboard::Key::Num1: executeSkill(0); break;
                case sf::Keyboard::Key::Num2: executeSkill(1); break;
                case sf::Keyboard::Key::Num3: executeSkill(2); break;
                case sf::Keyboard::Key::Num4: executeSkill(3); break;
                case sf::Keyboard::Key::Num5: executeSkill(4); break;
                case sf::Keyboard::Key::Num6: executeSkill(5); break;
                case sf::Keyboard::Key::Num7: executeSkill(6); break;
                case sf::Keyboard::Key::Num8: executeSkill(7); break;
                case sf::Keyboard::Key::Num9: executeSkill(8); break;

                case sf::Keyboard::Key::R:
                    m_inRaceSelect = true;
                    m_ui.selectedRace = 0;
                    break;
                case sf::Keyboard::Key::G:  tryPickupItem();          break;
                case sf::Keyboard::Key::D:
                    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift))
                        //(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift))
                    dropSelectedItem();
                    else if(m_ui.isPanelOpen(UIState::Panel::Inventory) && m_ui.selectedInvSlot < MAX_INVENTORY-1)
                        m_ui.selectedInvSlot++;
                    break;
                case sf::Keyboard::Key::E:
                    m_ui.togglePanel(UIState::Panel::Equipment);
                    break;
                case sf::Keyboard::Key::M:
                    if (m_ui.activePanel == UIState::Panel::Stats)
                        m_ui.activePanel = UIState::Panel::Inventory;
                    else
                        m_ui.activePanel = UIState::Panel::Stats;
                    break;
                case sf::Keyboard::Key::C:
                    m_ui.togglePanel(UIState::Panel::Cores);
                    break;
                //case sf::Keyboard::Key::LBracket:
                    //if (m_ui.isPanelOpen(UIState::Panel::Cores) && m_ui.selectedCoreSlot > 0)
                        //m_ui.selectedCoreSlot--;
                    //else if (m_ui.isPanelOpen(UIState::Panel::Equipment) && m_ui.selectedEquipSlot > 0)
                        //m_ui.selectedEquipSlot--;
                    //break;
                //case sf::Keyboard::Key::RBracket:
                    //if (m_ui.isPanelOpen(UIState::Panel::Cores) && m_ui.selectedCoreSlot < m_coreSlots.getSlotCount()-1)
                        //m_ui.selectedCoreSlot++;
                    //else if (m_ui.isPanelOpen(UIState::Panel::Equipment) && m_ui.selectedEquipSlot < 6)
                        //m_ui.selectedEquipSlot++;
                    //break;
                case sf::Keyboard::Key::Period: tryDescendStairs(); break;
                case sf::Keyboard::Key::Comma:  tryAscendStairs();  break;
                case sf::Keyboard::Key::Space:  waitTurn();         break;
                case sf::Keyboard::Key::X:
                    if (m_ui.isPanelOpen(UIState::Panel::Cores))
                        unequipCore();
                    else if (m_ui.isPanelOpen(UIState::Panel::Equipment))
                        unequipSelected();
                    break;
                case sf::Keyboard::Key::Enter:
                    if (m_ui.isPanelOpen(UIState::Panel::Cores))
                        equipCore();
                    else if (m_ui.isPanelOpen(UIState::Panel::Equipment))
                        unequipSelected();
                    else
                        useOrEquipSelected();
                    break;
                case sf::Keyboard::Key::W:
                    if (m_ui.isPanelOpen(UIState::Panel::Cores) && m_ui.selectedCoreSlot > 0)
                        m_ui.selectedCoreSlot--;
                    else if (m_ui.isPanelOpen(UIState::Panel::Equipment) && m_ui.selectedEquipSlot > 0)
                        m_ui.selectedEquipSlot--;
                    else if (m_ui.isPanelOpen(UIState::Panel::Inventory) && m_ui.selectedInvSlot >= 5)
                        m_ui.selectedInvSlot -= 5;
                    break;

                case sf::Keyboard::Key::S:
                    if (m_ui.isPanelOpen(UIState::Panel::Cores) && m_ui.selectedCoreSlot < m_coreSlots.getSlotCount()-1)
                        m_ui.selectedCoreSlot++;
                    else if (m_ui.isPanelOpen(UIState::Panel::Equipment) && m_ui.selectedEquipSlot < 6)
                        m_ui.selectedEquipSlot++;
                    else if (m_ui.isPanelOpen(UIState::Panel::Inventory) && m_ui.selectedInvSlot < MAX_INVENTORY-5)
                        m_ui.selectedInvSlot += 5;
                    break;

                case sf::Keyboard::Key::A:
                    if (m_ui.isPanelOpen(UIState::Panel::Inventory) && m_ui.selectedInvSlot > 0)
                        m_ui.selectedInvSlot--;
                    break;

                
                //case sf::Keyboard::Key::Tab:
                    //m_ui.selectedInvSlot = (m_ui.selectedInvSlot + 1) % MAX_INVENTORY;
                    //break;
            }
        }
    }
}

void Game::handleInventoryInput(sf::Keyboard::Key){}

// ============================================================
//  Stairs / Wait
// ============================================================
void Game::tryDescendStairs()
{
    if (!m_player) return;
    if (m_tileMap.getTile(m_player->getCol(),m_player->getRow())!=TileType::GateDown)
    {addLog("  No gate here.");return;}
    m_dungeonFloor++;
    Stats saved=m_player->getStats();
    auto savedSkills=m_player->getSkills();
    auto savedHotbar = std::vector<std::string>();
    for (int i=0;i<9;++i) savedHotbar.push_back(m_player->getHotbar(i));
    newDungeon(true);
    if (m_player)
    {
        for (int row=1;row<m_mapRows-1;++row)
            for (int col=1;col<m_mapCols-1;++col)
                if (m_tileMap.getTile(col,row)==TileType::GateUp)
                { m_player->setPos(col,row); goto descend_placed; }
        descend_placed:
        m_player->getStats()=saved;
        m_player->getSkills()=savedSkills;
        for (int i=0;i<9;++i) m_player->setHotbar(i,savedHotbar[i]);
    }
    addLog("  Entered floor "+std::to_string(m_dungeonFloor)+" through the gate!",sf::Color(255,220,50));
}

void Game::tryAscendStairs()
{
    if (!m_player) return;
    if (m_tileMap.getTile(m_player->getCol(),m_player->getRow())!=TileType::GateUp)
    {addLog("  No gate here.");return;}
    if (m_dungeonFloor<=1){addLog("  Already on floor 1.");return;}
    m_dungeonFloor--;
    Stats saved=m_player->getStats();
    auto savedSkills=m_player->getSkills();
    auto savedHotbar = std::vector<std::string>();
    for (int i=0;i<9;++i) savedHotbar.push_back(m_player->getHotbar(i));
    newDungeon(true);
    if (m_player)
    {
        for (int row=1;row<m_mapRows-1;++row)
            for (int col=1;col<m_mapCols-1;++col)
                if (m_tileMap.getTile(col,row)==TileType::GateDown)
                { m_player->setPos(col,row); goto ascend_placed; }
        ascend_placed:
        m_player->getStats()=saved;
        m_player->getSkills()=savedSkills;
        for (int i=0;i<9;++i) m_player->setHotbar(i,savedHotbar[i]);
    }
    addLog("  Returned to floor "+std::to_string(m_dungeonFloor)+" through the gate!",sf::Color(100,220,255));
}

void Game::waitTurn()
{
    if (!m_player) return;

    for (auto* e : m_enemies)
    {
        if (m_fog.isVisible(e->getCol(), e->getRow()))
        {
            addLog("  Can't wait - enemy in sight!", sf::Color(220, 80, 80));
            return;
        }
    }
    // เติม spdCounter เต็ม (100) เหมือนขยับ 1 เทิร์น
    Stats& ps = m_player->getStats();
    ps.spdCounter = std::max(ps.spdCounter, 100);

    m_turnCount++; m_player->onTurnPassed();
    processTurn(); tryRespawnEnemies();
    m_fog.compute(m_player->getCol(), m_player->getRow(), VIEW_RADIUS, m_tileMap);
    addLog("  You wait.", sf::Color(140,140,140));
}

// ============================================================
//  Inventory / Item
// ============================================================
void Game::tryPickupItem()
{
    if (!m_player) return;
    int pc=m_player->getCol(),pr=m_player->getRow();
    for (int i=0;i<(int)m_mapItems.size();++i)
        if (m_mapItems[i].col==pc&&m_mapItems[i].row==pr)
        {
            if (m_inventory.isFull()){addLog("  Inventory full!",sf::Color(220,120,50));return;}
            Item item=m_mapItems[i]; item.col=-1; item.row=-1;
            m_inventory.addItem(item);
            m_mapItems.erase(m_mapItems.begin()+i);
            addLog("  Picked up: "+item.name,sf::Color(180,220,100));
            return;
        }
    addLog("  Nothing here.");
}

void Game::useOrEquipSelected()
{
    Item* item = m_inventory.getItem(m_ui.selectedInvSlot);
    if (!item||!m_player) return;
    Stats& s=m_player->getStats();
    if (item->isUsable())
    {
        if(item->type==ItemType::Food)
        {s.hunger=std::min(100,s.hunger+item->value);
         addLog("  Ate "+item->name+". Hunger +"+std::to_string(item->value),sf::Color(200,180,80));}
        else
        {s.hp=std::min(s.maxHp,s.hp+item->value);
         addLog("  Used "+item->name+". HP +"+std::to_string(item->value),sf::Color(80,180,220));}
        m_inventory.removeItem(m_ui.selectedInvSlot);
    }
    else if (item->isCore()) equipCore();
    else if (item->isMaterial()) addLog("  "+item->name+" is a trade item.",sf::Color(160,160,160));
    else if (item->isEquipment())
    {
        EquipSlot slot=Equipment::itemToSlot(*item);
        if (m_equipment.hasItem(slot))
        {
            Item old=m_equipment.unequip(slot);
            if (!m_inventory.isFull()) m_inventory.addItem(old);
            else{addLog("  Inventory full!",sf::Color(220,120,50));return;}
        }
        Item toEquip=*item;
        m_inventory.removeItem(m_ui.selectedInvSlot);
        m_equipment.equip(toEquip,slot);
        addLog("  Equipped "+toEquip.name+" ["+Equipment::slotName(slot)+"] +"+
               std::to_string(toEquip.value),sf::Color(180,220,180));
        
        refreshStats();
        recalcSpeed();
    }
}

void Game::unequipSelected()
{
    if (!m_player) return;
    const EquipSlot slots[] = {
        EquipSlot::Head, EquipSlot::Body, EquipSlot::Arms,
        EquipSlot::Legs, EquipSlot::Feet,
        EquipSlot::MainHand, EquipSlot::OffHand
    };
    if (m_ui.selectedEquipSlot < 0 || m_ui.selectedEquipSlot >= 7) return;
    EquipSlot slot = slots[m_ui.selectedEquipSlot];
    if (!m_equipment.hasItem(slot)) { addLog("  Nothing to unequip."); return; }
    if (m_inventory.isFull()) { addLog("  Inventory full!"); return; }
    Item item = m_equipment.unequip(slot);
    m_inventory.addItem(item);
    addLog("  Unequipped "+item.name, sf::Color(180,180,180));
    refreshStats();
    recalcSpeed();
}

void Game::equipCore()
{
    Item* item = m_inventory.getItem(m_ui.selectedInvSlot);
    if (!item||!item->isCore()){addLog("  Not a core item!");return;}
    int freeSlot=-1;
    for (int i=0;i<m_coreSlots.getSlotCount();++i)
        if (!m_coreSlots.hasCore(i)){freeSlot=i;break;}
    if (freeSlot==-1){addLog("  All core slots full!",sf::Color(220,120,50));return;}
    Item core=*item;
    m_inventory.removeItem(m_ui.selectedInvSlot);
    m_coreSlots.equip(freeSlot,core);
    addLog("  Equipped "+core.name+" in core slot "+std::to_string(freeSlot+1),sf::Color(100,200,255));

    const ItemData* idata = DropTable::instance().getItem(core.id);
    if (idata)
        for (const auto& skillId : idata->coreSkills)
        {
            if (m_player->addCoreSkill(skillId))
                addLog("  Skill unlocked: "+skillId, sf::Color(100,220,255));
            else
                addLog("  Hotbar full — "+skillId+" not assigned", sf::Color(180,120,50));
        }
    int abilityCount = 0;
    for (const auto& sk : m_player->getSkills())
        if (sk.fromCore) abilityCount++;
    m_player->getStats().ability = abilityCount;
    refreshStats();
    recalcSpeed();
}

void Game::unequipCore()
{
    if (!m_coreSlots.hasCore(m_ui.selectedCoreSlot))
    {addLog("  No core in slot "+std::to_string(m_ui.selectedCoreSlot+1));return;}
    if (m_inventory.isFull()){addLog("  Inventory full!");return;}
    Item core=m_coreSlots.unequip(m_ui.selectedCoreSlot);
    m_inventory.addItem(core);
    addLog("  Removed "+core.name+" from core slot "+std::to_string(m_ui.selectedCoreSlot+1),sf::Color(160,160,160));

    const ItemData* idata = DropTable::instance().getItem(core.id);
    if (idata)
        for (const auto& skillId : idata->coreSkills)
        {
            m_player->removeCoreSkill(skillId);
            addLog("  Skill lost: "+skillId, sf::Color(160,100,100));
        }
    int abilityCount = 0;
    for (const auto& sk : m_player->getSkills())
        if (sk.fromCore) abilityCount++;
    m_player->getStats().ability = abilityCount;
    refreshStats();
    recalcSpeed();
}

void Game::dropSelectedItem()
{
    Item* item = m_inventory.getItem(m_ui.selectedInvSlot);
    if (!item||!m_player) return;
    Item d=*item; d.col=m_player->getCol(); d.row=m_player->getRow();
    m_mapItems.push_back(d);
    addLog("  Dropped: "+d.name,sf::Color(160,160,160));
    m_inventory.removeItem(m_ui.selectedInvSlot);
}

// ============================================================
//  Movement & Combat
// ============================================================
void Game::handlePlayerMove(int dc, int dr)
{
    if (!m_player) return;

    // เช็ค spdCounter: ต้องมี >= 100 จึงขยับได้
    Stats& ms = m_player->getStats();
    if (ms.spdCounter < 100)
    {
        addLog("  Too slow to act! ("+std::to_string(ms.spdCounter)+"/100)", sf::Color(160,120,80));
        return;
    }

    int tc=m_player->getCol()+dc, tr=m_player->getRow()+dr;

    for (auto*e:m_enemies)
        if (!e->isDead()&&e->getCol()==tc&&e->getRow()==tr)
        {
            playerAttack(e);
            ms.spdCounter -= 100;
            ms.spdCounter += ms.speedPerTurn;
            if (ms.spdCounter > 300) ms.spdCounter = 300;
            processTurn(); tryRespawnEnemies(); recalcSpeed();
            auto t1 = std::chrono::high_resolution_clock::now();
m_fog.compute(m_player->getCol(),m_player->getRow(),VIEW_RADIUS,m_tileMap);
auto t2 = std::chrono::high_resolution_clock::now();
auto ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count();
//addLog("fog: " + std::to_string(ms2) + "ms", sf::Color(100,255,100));
            updateCamera(); return;
        }

    bool moved=m_player->tryMove(dc,dr,m_tileMap);
    if (!moved) return;

    ms.spdCounter -= 100;
    // สะสม counter สำหรับ action ถัดไป (spd=0 → +100, spd=-1 → +50 ฯลฯ)
    ms.spdCounter += ms.speedPerTurn;
    if (ms.spdCounter > 300) ms.spdCounter = 300;

    m_turnCount++; m_player->onTurnPassed();
    m_fog.compute(m_player->getCol(),m_player->getRow(),VIEW_RADIUS,m_tileMap);

    processTurn(); tryRespawnEnemies(); recalcSpeed();
    updateCamera();

    int pc=m_player->getCol(),pr=m_player->getRow();
    for (auto& it:m_mapItems)
        if (it.col==pc&&it.row==pr)
        {addLog("  "+it.name+" here. [G] pick up.",sf::Color(200,200,80));break;}

    TileType tile=m_tileMap.getTile(pc,pr);
    if (tile==TileType::GateDown) addLog("  Gate ahead! [.] enter.",sf::Color(255,220,50));
    if (tile==TileType::GateUp)   addLog("  Gate! [,] return.",  sf::Color(100,220,255));

    int hunger=m_player->getStats().hunger;
    if(hunger==50) addLog("  You feel hungry.",      sf::Color(220,180,50));
    if(hunger==20) addLog("  Very hungry!",          sf::Color(220,100,50));
    if(hunger== 0) addLog("  Starving! HP draining!",sf::Color(220,50,50));
}

void Game::playerAttack(Enemy* enemy)
{
    if (!m_player) return;
    int atk=getBuffedAtk();
    int dmg=std::max(1,atk+std::rand()%4-enemy->getDefense());
    enemy->takeDamage(dmg);

    bool powered = false;
    for (const auto& sk : m_player->getSkills())
        if (sk.buffActive && sk.data.type == SkillType::ActiveBuff && sk.data.effect.atkPct > 0)
            powered = true;

    std::string msg="  You hit "+enemy->getName()+" for "+std::to_string(dmg)+"!";
    if (powered) msg+=" [POWERED]";
    addLog(msg,sf::Color(255,200,50));

    if (enemy->isDead())
    {
        //addLog("  "+enemy->getName()+" dead! +"+std::to_string(enemy->getExp())+" EXP",sf::Color(220,80,80));
        auto dropped=DropTable::instance().roll(enemy->getId());
        for (const auto& itemId:dropped)
        {
            const ItemData* idata=DropTable::instance().getItem(itemId);
            if (!idata) continue;
            Item drop;
            drop.id=(idata->id);
            drop.type=(idata->type=="Core")?ItemType::Core:ItemType::Material;
            drop.name=idata->name; drop.desc=idata->desc; drop.value=idata->value;
            drop.hpBonus=idata->hpBonus; drop.atkBonus=idata->atkBonus;
            drop.defBonus=idata->defBonus; drop.dodgeBonus=idata->dodgeBonus;
            drop.manaBonus=idata->manaBonus; drop.magicDmgBonus=idata->magicDmgBonus;
            drop.magicResBonus=idata->magicResBonus; drop.spdBonus=idata->spdBonus;
            drop.spriteName=idata->sprite; drop.stackable=idata->stackable;
            drop.col=enemy->getCol(); drop.row=enemy->getRow();
            m_mapItems.push_back(drop);
            addLog("  Dropped: "+idata->name,sf::Color(180,220,255));
        }
        const std::string& eid = enemy->getId();
            bool isFirstKill = (m_firstKillDone.find(eid) == m_firstKillDone.end());
            int expGain = isFirstKill ? enemy->getExp() : 0;

        if (isFirstKill) {
            m_firstKillDone.insert(eid);
        addLog("  [FIRST KILL] " + enemy->getName() + "! +" + std::to_string(expGain) + " EXP",
           sf::Color(255, 220, 50));
        }

            if (expGain > 0 && m_player->addExp(expGain))
        {
            m_ui.levelUpFlash = true;
            m_ui.levelUpTimer = 120;
            int lv=m_player->getStats().level;
            m_coreSlots.setSlotCount(lv);
            refreshStats();
            recalcSpeed();
            addLog("  *** LEVEL UP! Level "+std::to_string(lv)+" ***",sf::Color(255,255,50));
        }
        onEnemyKilled(enemy);
    }
}

void Game::enemyAttack(Enemy* enemy)
{
    if (!m_player) return;
    Stats& s=m_player->getStats();
    int def=getBuffedDef();
    int dodge=s.maxDodge+m_equipment.getTotalDodgeBonus();
    float dodgeChance=std::min(0.45f,0.06f+dodge*0.05f);
    if ((std::rand()%100)<(int)(dodgeChance*100))
    {addLog("  You dodged "+enemy->getName()+"!",sf::Color(150,220,150));return;}
    int dmg=std::max(1,enemy->getAttack()-def+std::rand()%3);
    s.hp-=dmg;
    addLog("  "+enemy->getName()+" hits you for "+std::to_string(dmg)+"!",sf::Color(220,80,80));
    if (s.hp<=0)
    {
        s.hp=0;
        m_playerDead=true;
        addLog("  *** YOU DIED *** [R] restart",sf::Color(255,50,50));
    }
}

// ============================================================
//  processTurn – จัดการเทิร์นของศัตรู
// ============================================================
void Game::processTurn()
{
    if (!m_player) return;

    regenStamina();

    // tick cooldown + buff duration ของ player skills
    for (auto& sk : m_player->getSkills())
        sk.tick();

    int pc = m_player->getCol();
    int pr = m_player->getRow();

    for (auto* e : m_enemies)
    {
        if (e->isDead()) continue;

        // ── มอน spd check: ช้า = skip turn ──
        if (!e->tickSpeed()) continue;

        if (!m_fog.isVisible(e->getCol(), e->getRow()) && !e->isAlerted()) continue;

        int dx = std::abs(e->getCol() - pc);
        int dy = std::abs(e->getRow() - pr);

        if (dx <= 1 && dy <= 1 && (dx + dy) > 0)
            {
                enemyAttack(e);
        }
        else
        {
            e->updateAI(pc, pr, m_tileMap, m_enemies);

            if (e->hasPendingSkill())
            {
                auto ps = e->consumePendingSkill();
                SkillInstance* sk = e->findSkill(ps.skillId);
                if (!sk) continue;

                if (sk->data.type == SkillType::ActiveRanged)
                {
                    int dmg = std::max(1, e->getAttack() * sk->data.effect.damagePct / 100);
                    auto line = getLine(e->getCol(), e->getRow(), ps.targetCol, ps.targetRow);
                    bool blocked = false;
                    for (int i = 1; i < (int)line.size(); ++i)
                    {
                        if (m_tileMap.getTile(line[i].x, line[i].y) == TileType::Wall)
                        { blocked = true; break; }
                        if (line[i].x == pc && line[i].y == pr)
                        {
                            Stats& s = m_player->getStats();
                            int def = getBuffedDef();
                            int actualDmg = std::max(1, dmg - def);
                            s.hp -= actualDmg;
                            addLog("  " + e->getName() + " uses " + sk->data.name +
                                   " -> " + std::to_string(actualDmg) + " dmg!",
                                   sf::Color(220, 100, 80));
                            if (s.hp <= 0) { s.hp = 0; m_playerDead = true; addLog("  *** YOU DIED *** [R] restart", sf::Color(255,50,50)); }
                            break;
                        }
                    }
                    if (blocked)
                        addLog("  " + e->getName() + "'s " + sk->data.name + " was blocked.",
                               sf::Color(120, 120, 120));
                }
                else if (sk->data.type == SkillType::ActiveAoe)
                {
                    int radius = sk->data.effect.aoeRadius;
                    int dmg    = std::max(1, e->getAttack() * sk->data.effect.damagePct / 100);
                    int dxP = pc - e->getCol(), dyP = pr - e->getRow();
                    if (dxP*dxP + dyP*dyP <= radius*radius)
                    {
                        Stats& s = m_player->getStats();
                        int def = getBuffedDef();
                        int actualDmg = std::max(1, dmg - def);
                        s.hp -= actualDmg;
                        addLog("  " + e->getName() + " uses " + sk->data.name +
                               " -> " + std::to_string(actualDmg) + " dmg!",
                               sf::Color(220, 100, 80));
                        if (s.hp <= 0) { s.hp = 0; m_playerDead = true; addLog("  *** YOU DIED *** [R] restart", sf::Color(255,50,50)); }
                    }
                }
            }
        }
        e->tickSkills();
    }

    m_enemies.erase(
        std::remove_if(m_enemies.begin(), m_enemies.end(),
            [](Enemy* e) { bool d = e->isDead(); if (d) delete e; return d; }),
        m_enemies.end());
        recalcAllStats();
}

// ============================================================
//  Render – ฟังก์ชันวาดทั้งหมด
// ============================================================
void Game::render()
{
    m_window.clear(sf::Color(8,8,8));
    if (m_inRaceSelect) { renderRaceSelect(); m_window.display(); return; }

    m_window.clear(sf::Color(0,0,0));
    m_window.setView(m_gameView);
    m_tileMap.render(m_window, m_gameView);
    renderItems();
    for (auto*e:m_enemies)
        if (m_fog.isVisible(e->getCol(),e->getRow())) e->render(m_window);
    if (m_player) m_player->render(m_window);
    m_fog.render(m_window,TILE_SIZE);

    if (m_ui.targeting.active) renderTargeting();

    m_window.setView(m_uiView);
    renderStatusPanel();
    renderRightPanel();
    renderLogPanel();
    if (m_ui.activePanel == UIState::Panel::Stats) renderStatsOverlay();
    renderHotbar();
    if (m_ui.levelUpFlash) renderLevelUpEffect();
    if (m_playerDead) renderDeathScreen();

    // ใน render() ก่อน m_window.display()
    if (m_fontLoaded)
    {
        static sf::Clock fpsClock;
        static int fpsCount = 0;
        static int fpsDisplay = 0;
        fpsCount++;
        if (fpsClock.getElapsedTime().asSeconds() >= 1.f)
        {
            fpsDisplay = fpsCount;
            fpsCount = 0;
            fpsClock.restart();
        }
        sf::Text fpsTxt(m_font, "FPS: " + std::to_string(fpsDisplay), 10);
        fpsTxt.setFillColor(sf::Color(100, 255, 100));
        fpsTxt.setPosition({4.f, 4.f});
        m_window.draw(fpsTxt);
    }
    m_window.display();
}

// ============================================================
//  Hotbar Render
// ============================================================
void Game::renderHotbar()
{
    if (!m_player || !m_fontLoaded) return;

    const float SZ  = 22.f;
    const float GAP = 4.f;
    const int   N   = 9;
    float totalW = N * (SZ + GAP) - GAP;
    float startX = 8.f;
    float startY = 568.f;

    for (int i = 0; i < N; ++i)
    {
        float sx = startX + i * (SZ + GAP);
        const std::string& id = m_player->getHotbar(i);
        const SkillInstance* sk = id.empty() ? nullptr : m_player->findSkill(id);

        sf::RectangleShape slot({SZ, SZ});
        slot.setFillColor(sf::Color(8, 8, 8, 200));
        slot.setOutlineColor(sf::Color(120, 120, 120));
        slot.setOutlineThickness(1.f);
        slot.setPosition({sx, startY});
        m_window.draw(slot);

        sf::Text numTxt(m_font, std::to_string(i+1), 8);
        numTxt.setFillColor(sf::Color(120, 120, 120));
        numTxt.setPosition({sx + 2.f, startY + 1.f});
        m_window.draw(numTxt);

        if (sk)
        {
            if (!sk->isReady())
            {
                float ratio = sk->data.cooldown > 0
                    ? (float)sk->cooldownLeft / sk->data.cooldown : 1.f;
                sf::RectangleShape cd({SZ, SZ * ratio});
                cd.setFillColor(sf::Color(0, 0, 0, 160));
                cd.setPosition({sx, startY + SZ * (1.f - ratio)});
                m_window.draw(cd);

                sf::Text cdTxt(m_font, std::to_string(sk->cooldownLeft), 9);
                cdTxt.setFillColor(sf::Color(220, 100, 100));
                cdTxt.setPosition({sx + SZ/2.f - 5.f, startY + SZ/2.f - 6.f});
                m_window.draw(cdTxt);
            }
            else if (sk->buffActive)
            {
                sf::RectangleShape activeGlow({SZ, SZ});
                activeGlow.setFillColor(sf::Color(255, 200, 50, 40));
                activeGlow.setOutlineColor(sf::Color(255, 200, 50));
                activeGlow.setOutlineThickness(2.f);
                activeGlow.setPosition({sx, startY});
                m_window.draw(activeGlow);
            }

            std::string label = sk->data.name.substr(0, 4);
            sf::Text nameTxt(m_font, label, 7);
            nameTxt.setFillColor(sk->isReady() ? sf::Color(200, 200, 200) : sf::Color(100, 100, 100));
            nameTxt.setPosition({sx + 2.f, startY + SZ - 10.f});
            m_window.draw(nameTxt);
        }
    }
}

// ============================================================
//  Targeting Render
// ============================================================
void Game::renderTargeting()
{
    if (!m_ui.targeting.active) return;
    int pc = m_player->getCol();
    int pr = m_player->getRow();
    float ts = (float)TILE_SIZE;

    SkillInstance* sk = m_player->findSkill(m_ui.targeting.skillId);
    int maxRange = sk ? sk->data.effect.range : 6;

    auto line = getLine(pc, pr, m_ui.targeting.targetCol, m_ui.targeting.targetRow);

    bool blocked = false;
    for (int i = 1; i < (int)line.size(); ++i)
    {
        int tx = line[i].x;
        int ty = line[i].y;
        float px = tx * ts;
        float py = ty * ts;

        int ddx = tx-pc, ddy = ty-pr;
        float dist = std::sqrt((float)(ddx*ddx+ddy*ddy));
        bool outOfRange = dist > maxRange;
        bool isWall = (m_tileMap.getTile(tx,ty)==TileType::Wall);
        if (isWall) blocked = true;

        sf::Color tileColor;
        if (blocked || outOfRange)
            tileColor = sf::Color(180,50,50,120);
        else
            tileColor = sf::Color(255,220,50,100);

        sf::RectangleShape highlight({ts, ts});
        highlight.setFillColor(tileColor);
        highlight.setPosition({px, py});
        m_window.draw(highlight);

        if (!blocked && !outOfRange)
        {
            for (auto* e : m_enemies)
            {
                if (!e->isDead() && e->getCol()==tx && e->getRow()==ty)
                {
                    sf::RectangleShape enemyHL({ts,ts});
                    enemyHL.setFillColor(sf::Color(255,50,50,160));
                    enemyHL.setOutlineColor(sf::Color(255,100,100));
                    enemyHL.setOutlineThickness(2.f);
                    enemyHL.setPosition({px,py});
                    m_window.draw(enemyHL);
                    break;
                }
            }
        }
    }

    {
        float cpx = m_ui.targeting.targetCol * ts;
        float cpy = m_ui.targeting.targetRow * ts;

        int ddx = m_ui.targeting.targetCol - pc, ddy = m_ui.targeting.targetRow - pr;
        float dist = std::sqrt((float)(ddx*ddx+ddy*ddy));
        bool cursorBlocked = blocked || dist > maxRange;

        sf::Color cursorColor = cursorBlocked
            ? sf::Color(220,50,50,220)
            : sf::Color(255,220,50,220);

        sf::RectangleShape cursor({ts, ts});
        cursor.setFillColor(sf::Color(0,0,0,0));
        cursor.setOutlineColor(cursorColor);
        cursor.setOutlineThickness(3.f);
        cursor.setPosition({cpx, cpy});
        m_window.draw(cursor);

        float cornerSz = 8.f;
        const float offsets[4][2] = {{0,0},{ts-cornerSz,0},{0,ts-cornerSz},{ts-cornerSz,ts-cornerSz}};
        for (auto& off : offsets)
        {
            sf::RectangleShape corner({cornerSz, cornerSz});
            corner.setFillColor(cursorColor);
            corner.setPosition({cpx+off[0], cpy+off[1]});
            m_window.draw(corner);
        }

        if (m_fontLoaded)
        {
            for (auto* e : m_enemies)
            {
                if (!e->isDead() && e->getCol()==m_ui.targeting.targetCol && e->getRow()==m_ui.targeting.targetRow)
                {
                    sf::Text nameTag(m_font, e->getName(), 9);
                    nameTag.setFillColor(sf::Color(255,180,180));
                    nameTag.setOutlineColor(sf::Color(0,0,0));
                    nameTag.setOutlineThickness(1.f);
                    nameTag.setPosition({cpx, cpy - 14.f});
                    m_window.draw(nameTag);
                    break;
                }
            }
        }
    }
}

void Game::renderItems()
{
    auto& tm=TextureManager::instance();
    for (const auto& item:m_mapItems)
    {
        if (!m_fog.isVisible(item.col,item.row)) continue;
        float px=(float)(item.col*TILE_SIZE),py=(float)(item.row*TILE_SIZE),ts=(float)TILE_SIZE;
        std::string path=item.spritePath();
        std::string texName=path.substr(path.rfind('/')+1);
        if (texName.find('.')!=std::string::npos) texName=texName.substr(0,texName.rfind('.'));
        const sf::Texture* tex=tm.get(texName);
        if (tex)
        {
            sf::Sprite spr(*tex);
            sf::Vector2u sz=tex->getSize();
            float scale=ts*0.6f/std::max((float)sz.x,(float)sz.y);
            spr.setScale({scale,scale});
            spr.setPosition({px+ts*0.2f,py+ts*0.2f});
            m_window.draw(spr);
        }
        else
        {
            sf::CircleShape dot(ts*0.18f);
            dot.setFillColor(item.color());
            dot.setOutlineColor(sf::Color(255,255,255,120));
            dot.setOutlineThickness(1.f);
            dot.setPosition({px+ts*0.32f,py+ts*0.32f});
            m_window.draw(dot);
        }
    }
}

void Game::renderLevelUpEffect()
{
    float alpha=std::min(180.f,(float)m_ui.levelUpTimer*1.5f);
    sf::RectangleShape flash({(float)GAME_VIEW_W,(float)GAME_VIEW_H});
    flash.setFillColor(sf::Color(255,220,0,(uint8_t)alpha));
    flash.setPosition({0.f,0.f}); m_window.draw(flash);
    if (m_fontLoaded)
    {
        sf::Text txt(m_font,"LEVEL UP!",22);
        txt.setFillColor(sf::Color(255,255,100));
        txt.setOutlineColor(sf::Color(100,60,0));
        txt.setOutlineThickness(2.f);
        txt.setPosition({GAME_VIEW_W/2.f-60.f,GAME_VIEW_H/2.f-20.f});
        m_window.draw(txt);
    }
}

// ============================================================
//  Skill Panel
// ============================================================
void Game::renderSkillPanel()
{
    if (!m_player||!m_fontLoaded) return;
    float px=(float)(WINDOW_W-RIGHT_PANEL_W);
    float y= 185.f;

    for (const auto& sk:m_player->getSkills())
    {
        if (!sk.fromCore) continue;
        std::string line=sk.data.name;
        sf::Color c=sf::Color(160,160,160);
        if (sk.data.type==SkillType::Passive)
        { line+=" [passive]"; c=sf::Color(120,200,120); }
        else if (sk.buffActive)
        { line+=" ["+std::to_string(sk.durationLeft)+"t]"; c=sf::Color(255,220,50); }
        else if (!sk.isReady())
        { line+=" [cd:"+std::to_string(sk.cooldownLeft)+"]"; c=sf::Color(180,80,80); }
        else
        { line+=" ready"; c=sf::Color(100,220,100); }

        sf::Text t(m_font,line,8);
        t.setFillColor(c);
        t.setPosition({px+10.f,y});
        y+=11.f;
    }
}

// ============================================================
//  Status Panel
// ============================================================
static std::string fmtStat(float val)
{
    float frac = val - (int)val;
    if (frac >= 0.1f)
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.1f", val);
        return buf;
    }
    return std::to_string((int)val);
}
void Game::renderStatusPanel()
{
    float px = (float)(WINDOW_W - RIGHT_PANEL_W);
    sf::RectangleShape panel({(float)RIGHT_PANEL_W, (float)STATUS_PANEL_H});
    panel.setFillColor(sf::Color(8,8,8));
    panel.setPosition({px, 0.f});
    panel.setOutlineColor(sf::Color(120, 120, 120));
    panel.setOutlineThickness(1.f);
    m_window.draw(panel);

    if (!m_fontLoaded || !m_player) return;
    const Stats& s = m_player->getStats();
    float x = px + 15.f, y = (float)STATUS_PANEL_TOP_PADDING;

    auto drawLine = [&](const std::string& label, const std::string& value,
                        sf::Color color = sf::Color::White, int size = STATUS_LINE_SIZE) {
        sf::Text t(m_font, label + " " + value, (unsigned int)size);
        t.setFillColor(color);
        t.setPosition({x, y});
        m_window.draw(t);
        y += size + STATUS_LINE_SPACING;
    };

    {
        sf::Text h(m_font, "player 1", STATUS_HEADER_SIZE);
        h.setPosition({x, y});
        m_window.draw(h);
        y += STATUS_HEADER_SIZE + STATUS_HEADER_SPACING;
    }

    auto deltaStr = [](int d) -> std::string {
        if (d == 0) return "";
        return d > 0 ? " (NEW+" + std::to_string(d) + ")" : " (NEW" + std::to_string(d) + ")";
    };
    bool showDelta = m_deltaTimer > 0;
    sf::Color deltaColor = sf::Color(100,255,150);

    drawLine("level:", std::to_string(s.level));
    drawLine("body:", fmtStat(m_finalStats.body) + (showDelta && m_deltaBody != 0 ? deltaStr(m_deltaBody) : ""));
    drawLine("mentality:", fmtStat(m_finalStats.mentality) + (showDelta && m_deltaMentality != 0 ? deltaStr(m_deltaMentality) : ""));
    drawLine("ability:", std::to_string(s.ability));
    drawLine("item level:", std::to_string(m_finalStats.itemLevel));
    drawLine("Comprehensive Battle Index", "");
    drawLine(":", fmtStat(m_finalStats.battleIndex) + (showDelta && m_deltaBattleIndex != 0 ? deltaStr(m_deltaBattleIndex) : ""));

    renderSkillPanel();
}
//showDelta && m_deltaBattleIndex != 0 ? deltaStr(m_deltaBattleIndex) : ""));
void Game::renderStatsOverlay()
{
    if (!m_player || !m_fontLoaded) return;
    const Stats& s = m_player->getStats();

    sf::RectangleShape dim({(float)WINDOW_W, (float)WINDOW_H});
    dim.setFillColor(sf::Color(0,0,0,180));
    m_window.draw(dim);

    float pw = 300.f, ph = 350.f;
    float px = (WINDOW_W - pw) / 2.f;
    float py = (WINDOW_H - ph) / 2.f;
    sf::RectangleShape panel({pw, ph});
    panel.setFillColor(sf::Color(12,12,18));
    panel.setOutlineColor(sf::Color(120, 120, 120));
    panel.setOutlineThickness(1.f);
    panel.setPosition({px, py});
    m_window.draw(panel);

    float x = px + 20.f, y = py + 16.f;
    int sz = 11;

    auto line = [&](const std::string& label, const std::string& val, sf::Color col = sf::Color(200,200,200)) {
        sf::Text t(m_font, label + val, sz);
        t.setFillColor(col);
        t.setPosition({x, y});
        m_window.draw(t);
        y += sz + 8.f;
    };

    line("Level:       ", std::to_string(s.level));
    line("HP:          ", std::to_string(s.hp) + "/" + std::to_string(m_finalStats.maxHp));
    line("ATK:         ", std::to_string(m_finalStats.atk));
    line("DEF:         ", std::to_string(m_finalStats.def));
    line("Dodge:       ", std::to_string(m_finalStats.dodge));

    // Stamina bar
    {
        std::string stam = std::to_string(s.stamina) + "/" + std::to_string(s.maxStamina)
                         + "  (+" + std::to_string(s.staminaRegen) + "/turn)";
        sf::Color stCol = s.stamina < s.maxStamina / 3 ? sf::Color(255,120,50)
                        : s.stamina < s.maxStamina * 2 / 3 ? sf::Color(255,200,80)
                        : sf::Color(80,220,140);
        line("Stamina:     ", stam, stCol);
    }

    // Speed — แสดง modifier + counter
    {
        std::string spdStr = (m_finalStats.spd >= 0 ? "+" : "") + std::to_string(m_finalStats.spd);
        spdStr += "  [" + std::to_string(s.spdCounter) + "/100]";
        sf::Color spdCol = m_finalStats.spd > 0 ? sf::Color(100,255,150)
                         : m_finalStats.spd < 0 ? sf::Color(255,100,100)
                         : sf::Color(200,200,200);
        line("Speed:       ", spdStr, spdCol);
    }

    line("Mana:        ", std::to_string(s.mana) + "/" + std::to_string(m_finalStats.maxMana));
    line("MATK:        ", std::to_string(m_finalStats.matk));
    line("MDEF:        ", std::to_string(m_finalStats.mdef));
    line("Hunger:      ", std::to_string(s.hunger) + "/100");
    line("EXP:         ", std::to_string(s.exp) + "/" + std::to_string(s.expToNext));
}

// ============================================================
//  Right Panel
// ============================================================
void Game::renderRightPanel()
{
    float panelX = (float)(WINDOW_W - RIGHT_PANEL_W);
    float panelY = (float)STATUS_PANEL_H;

    sf::RectangleShape panel({(float)RIGHT_PANEL_W, (float)INV_GRID_H});
    panel.setFillColor(sf::Color(8, 8, 8));
    panel.setPosition({panelX, panelY});
    panel.setOutlineColor(sf::Color(120, 120, 120));
    panel.setOutlineThickness(0.f);
    m_window.draw(panel);

    sf::Vertex leftLine[2];
    leftLine[0].position = sf::Vector2f(panelX, panelY);
    leftLine[0].color = sf::Color(120, 120, 120);
    leftLine[1].position = sf::Vector2f(panelX, panelY + 250.f);
    leftLine[1].color = sf::Color(120, 120, 120);
    m_window.draw(leftLine, 2, sf::PrimitiveType::Lines);

    if (!m_fontLoaded) return;

    switch (m_ui.activePanel)
    {
        case UIState::Panel::Inventory:
        {
            const int COLS = 5, ROWS = 4;
            const float SZ = 35.f, GAP = 4.f;
            float sx0 = panelX + (RIGHT_PANEL_W - (COLS * (SZ + GAP) - GAP)) / 2.f;
            float sy0 = panelY + 10.f;

            for (int row = 0; row < ROWS; ++row)
                for (int col = 0; col < COLS; ++col)
                {
                    int idx = row * COLS + col;
                    float sx = sx0 + col * (SZ + GAP);
                    float sy = sy0 + row * (SZ + GAP);
                    bool sel = (idx == m_ui.selectedInvSlot);

                    sf::RectangleShape slot({SZ, SZ});
                    slot.setFillColor(sel ? sf::Color(60, 50, 30) : sf::Color(8, 8, 8));
                    slot.setOutlineColor(sel ? sf::Color(200, 160, 60) : sf::Color(120, 120, 120));
                    slot.setOutlineThickness(1.5f);
                    slot.setPosition({sx, sy});
                    m_window.draw(slot);

                    Item* item = m_inventory.getItem(idx);
                    if (item) {
                        auto& tm = TextureManager::instance();
                        std::string path = item->spritePath();
                        std::string name = path.substr(path.rfind('/')+1);
                        name = name.substr(0, name.rfind('.'));
                        const sf::Texture* tex = tm.get(name);
                        if (tex) {
                            sf::Sprite spr(*tex);
                            auto sz = tex->getSize();
                            float sc = SZ * 0.8f / std::max((float)sz.x, (float)sz.y);
                            spr.setScale({sc, sc});
                            spr.setPosition({sx + SZ*0.1f, sy + SZ*0.1f});
                            m_window.draw(spr);
                        } else {
                            sf::CircleShape dot(SZ * 0.28f);
                            dot.setFillColor(item->color());
                            dot.setPosition({sx + SZ*0.22f, sy + SZ*0.22f});
                            m_window.draw(dot);
                        }
                        int cnt = m_inventory.getCount(idx);
                        if (cnt > 1) {
                            sf::Text ct(m_font, "x"+std::to_string(cnt), 7);
                            ct.setFillColor(sf::Color(255,220,100));
                            ct.setPosition({sx+SZ-14.f, sy+SZ-10.f});
                            m_window.draw(ct);
                        }
                    }
                }

            Item* selItem = m_inventory.getItem(m_ui.selectedInvSlot);
            std::string info = "(empty)";
            if (selItem) {
                int cnt = m_inventory.getCount(m_ui.selectedInvSlot);
                info = selItem->name + " (" + std::to_string(selItem->value) + ")";
                if (cnt > 1) info += " x" + std::to_string(cnt);
            }
            sf::Text infoTxt(m_font, info, 9);
            infoTxt.setFillColor(sf::Color(180,160,100));
            infoTxt.setPosition({panelX+8.f, panelY + 10.f + ROWS*(SZ+GAP) + 4.f});
            m_window.draw(infoTxt);
            break;
        }

        case UIState::Panel::Cores:
        {
            m_coreSlots.render(m_window, m_font, panelX+8.f, panelY+20.f,
                               (float)RIGHT_PANEL_W, m_ui.selectedCoreSlot, true);

            StatBonus cb = m_coreSlots.getTotalBonus();
            float hy = panelY + INV_GRID_H - 55.f;
            auto dh = [&](const std::string& t, sf::Color c = sf::Color(70,70,70)) {
                sf::Text tx(m_font, t, 8);
                tx.setFillColor(c);
                tx.setPosition({panelX+8.f, hy});
                m_window.draw(tx);
                hy += 12.f;
            };
            dh("Bonus: HP+" + std::to_string(cb.hp) + " ATK+" + std::to_string(cb.atk) +
               " DEF+" + std::to_string(cb.def) + " DOD+" + std::to_string(cb.dodge) +
               " MANA+" + std::to_string(cb.mana) + " MATK+" + std::to_string(cb.matk) +
               " MRES+" + std::to_string(cb.magicRes) + " SPD+" + std::to_string(cb.spd),
               sf::Color(100,200,255));
            dh("[C] Close  [1-9] Equip core");
            dh("[X] Remove core");
            break;
        }

        case UIState::Panel::Equipment:
        {
            sf::Text title(m_font, "EQUIPMENT  []/[] select  [X]=Unequip", 9);
            title.setFillColor(sf::Color(120,160,120));
            title.setPosition({panelX+8.f, panelY+6.f});
            m_window.draw(title);

            const EquipSlot slots[] = { EquipSlot::Head, EquipSlot::Body, EquipSlot::Arms,
                                        EquipSlot::Legs, EquipSlot::Feet,
                                        EquipSlot::MainHand, EquipSlot::OffHand };
            const float SZH = 18.f, GAP = 3.f;
            float sx = panelX + 8.f;
            float sy = panelY + 20.f;

            for (int i = 0; i < 7; ++i)
            {
                bool sel = (i == m_ui.selectedEquipSlot);
                const Item* item = m_equipment.getItem(slots[i]);

                if (sel) {
                    sf::RectangleShape hl({(float)RIGHT_PANEL_W - 16.f, SZH});
                    hl.setFillColor(sf::Color(40,60,40));
                    hl.setOutlineColor(sf::Color(100,200,100));
                    hl.setOutlineThickness(1.f);
                    hl.setPosition({panelX+8.f, sy});
                    m_window.draw(hl);
                }

                sf::Text label(m_font, Equipment::slotName(slots[i]) + ":", 9);
                label.setFillColor(sel ? sf::Color(180,255,180) : sf::Color(140,120,80));
                label.setPosition({sx, sy});
                m_window.draw(label);

                sf::Text itemTxt(m_font, item ? item->name : "--", 9);
                itemTxt.setFillColor(item ? item->color() : sf::Color(60,60,60));
                itemTxt.setPosition({sx + 44.f, sy});
                m_window.draw(itemTxt);

                if (item) {
                    sf::Text val(m_font, "+" + std::to_string(item->value), 9);
                    val.setFillColor(sf::Color(180,200,100));
                    val.setPosition({sx + 160.f, sy});
                    m_window.draw(val);
                }

                sy += SZH + GAP;
            }
            break;
        }

        case UIState::Panel::Stats:
            break;
    }
}

void Game::addLog(const std::string& msg, sf::Color color)
{
    m_log.push_back({msg, color});
    while ((int)m_log.size() > LOG_MAX_LINES)
        m_log.erase(m_log.begin());
}

void Game::renderLogPanel()
{
    float panelY=(float)(WINDOW_H-LOG_PANEL_H);
    sf::RectangleShape panel({(float)WINDOW_W,(float)LOG_PANEL_H});
    panel.setFillColor(sf::Color(8, 8, 8));
    panel.setPosition({0.f,panelY});
    panel.setOutlineColor(sf::Color(120, 120, 120));
    panel.setOutlineThickness(1.f);
    m_window.draw(panel);
    if (!m_fontLoaded) return;
    float y=panelY+8.f;
    for (const auto& entry:m_log)
    {
        sf::Text t(m_font,entry.text,8);
        t.setFillColor(entry.color);
        t.setPosition({8.f,y}); m_window.draw(t);
        y+=17.f;
    }
}
void Game::renderDeathScreen()
{
    if (!m_fontLoaded) return;

    // dim overlay
    sf::RectangleShape dim({(float)WINDOW_W, (float)WINDOW_H});
    dim.setFillColor(sf::Color(8, 8, 8, 200));
    m_window.draw(dim);

    // panel
    float pw = 320.f, ph = 220.f;
    float px = (WINDOW_W - pw) / 2.f;
    float py = (WINDOW_H - ph) / 2.f;
    sf::RectangleShape panel({pw, ph});
    panel.setFillColor(sf::Color(18, 5, 5));
    panel.setOutlineColor(sf::Color(180, 40, 40));
    panel.setOutlineThickness(2.f);
    panel.setPosition({px, py});
    m_window.draw(panel);

    // YOU DIED
    sf::Text died(m_font, "YOU DIED", 32);
    died.setFillColor(sf::Color(220, 40, 40));
    died.setOutlineColor(sf::Color(80, 0, 0));
    died.setOutlineThickness(2.f);
    auto db = died.getLocalBounds();
    died.setPosition({px + (pw - db.size.x) / 2.f, py + 24.f});
    m_window.draw(died);

    // divider
    sf::RectangleShape divider({pw - 40.f, 1.f});
    divider.setFillColor(sf::Color(120, 30, 30));
    divider.setPosition({px + 20.f, py + 80.f});
    m_window.draw(divider);

    // stats summary
    if (m_player)
    {
        const Stats& s = m_player->getStats();
        float sy = py + 94.f;
        auto stat = [&](const std::string& txt, sf::Color c = sf::Color(180,140,140))
        {
            sf::Text t(m_font, txt, 11);
            t.setFillColor(c);
            auto b = t.getLocalBounds();
            t.setPosition({px + (pw - b.size.x) / 2.f, sy});
            m_window.draw(t);
            sy += 18.f;
        };
        stat("Floor " + std::to_string(m_dungeonFloor) +
             "   Level " + std::to_string(s.level) +
             "   Turns " + std::to_string(m_turnCount),
             sf::Color(160, 120, 120));
        stat("Body " + fmtStat(m_finalStats.body) +
             "   Battle Index " + fmtStat(m_finalStats.battleIndex),
             sf::Color(140, 100, 100));
    }

    // buttons
    float btnY = py + ph - 44.f;

    // [R] Restart
    {
        sf::RectangleShape btn({pw - 40.f, 26.f});
        btn.setFillColor(sf::Color(60, 10, 10));
        btn.setOutlineColor(sf::Color(180, 60, 60));
        btn.setOutlineThickness(1.5f);
        btn.setPosition({px + 20.f, btnY + -30.f});
        m_window.draw(btn);
        sf::Text t(m_font, "[R]estart", 11);
        t.setFillColor(sf::Color(220, 120, 120));
        t.setPosition({px + 110.f, btnY + -21.5f});
        m_window.draw(t);
    }

    // [Esc] Quit
    float btnY2 = btnY + 32.f;

    {
        sf::RectangleShape btn({pw - 40.f, 26.f});
        btn.setFillColor(sf::Color(20, 10, 10));
        btn.setOutlineColor(sf::Color(100, 60, 60));
        btn.setOutlineThickness(1.5f);
        btn.setPosition({px + 20.f, btnY2 + -25.f});
        m_window.draw(btn);
        sf::Text t(m_font, "[Q]uit", 11);
        t.setFillColor(sf::Color(140, 100, 100));
        t.setPosition({px + 125.f, btnY2 + -16.5f});
        m_window.draw(t);
    }
}
void Game::renderRaceSelect()
{
    if (!m_fontLoaded) return;
    auto& races = RaceDB::instance().getAll();

    // background
    sf::RectangleShape bg({(float)WINDOW_W, (float)WINDOW_H});
    bg.setFillColor(sf::Color(26, 26, 26));
    m_window.draw(bg);

    // title box
    sf::RectangleShape titleBox({320.f, 36.f});
    titleBox.setFillColor(sf::Color(30, 30, 30));
    titleBox.setOutlineColor(sf::Color(180, 180, 180));
    titleBox.setOutlineThickness(2.f);
    titleBox.setPosition({(float)WINDOW_W/2.f - 160.f, 24.f});
    m_window.draw(titleBox);

    sf::Text title(m_font, "SELECT YOUR CHARACTER", 14);
    title.setFillColor(sf::Color(220, 220, 220));
    auto tb = title.getLocalBounds();
    title.setPosition({(float)WINDOW_W/2.f - tb.size.x/2.f, 32.f});
    m_window.draw(title);

    // grid: 3 cols x 2 rows
    const float CARD_W = 160.f;
    const float CARD_H = 200.f;
    const float GAP    = 12.f;
    const int   COLS   = 3;
    const int   TOTAL  = 6; // 5 races + 1 ???

    float gridW = COLS * CARD_W + (COLS - 1) * GAP;
    float startX = (float)WINDOW_W/2.f - gridW/2.f;
    float startY = 80.f;

    auto& tm = TextureManager::instance();

    for (int i = 0; i < TOTAL; ++i)
    {
        int col = i % COLS;
        int row = i / COLS;
        float cx = startX + col * (CARD_W + GAP);
        float cy = startY + row * (CARD_H + GAP);

        bool sel     = (i == m_ui.selectedRace);
        bool isEmpty = (i >= (int)races.size()); // slot ???

        // card bg
        sf::RectangleShape card({CARD_W, CARD_H});
        card.setFillColor(sf::Color(155, 155, 155));
        card.setOutlineColor(sel ? sf::Color(255, 220, 50) : sf::Color(90, 90, 90));
        card.setOutlineThickness(sel ? 3.f : 1.5f);
        card.setPosition({cx, cy});
        m_window.draw(card);

        if (isEmpty)
        {
            // ??? slot
            sf::Text q(m_font, "???", 18);
            q.setFillColor(sf::Color(60, 60, 60));
            auto qb = q.getLocalBounds();
            q.setPosition({cx + CARD_W/2.f - qb.size.x/2.f, cy + CARD_H/2.f - 16.f});
            m_window.draw(q);
            continue;
        }

        const auto& r = races[i];

        // sprite
        const sf::Texture* tex = tm.get(r.sprite);
        if (tex)
        {
            sf::Sprite spr(*tex);
            sf::Vector2u sz = tex->getSize();
            float scale = std::min((CARD_W - 20.f) / sz.x, (CARD_H - 40.f) / sz.y);
            spr.setScale({scale, scale});
            spr.setPosition({
                cx + CARD_W/2.f - (sz.x * scale)/2.f,
                cy + 8.f
            });
            m_window.draw(spr);
        }
        else
        {
            // fallback สี่เหลี่ยมแทน sprite
            sf::RectangleShape placeholder({CARD_W - 20.f, CARD_H - 50.f});
            placeholder.setFillColor(sf::Color(70, 70, 80));
            placeholder.setPosition({cx + 10.f, cy + 8.f});
            m_window.draw(placeholder);
        }

        // ชื่อเผ่า
        sf::Text name(m_font, r.name, 10);
        name.setFillColor(sel ? sf::Color(255, 220, 50) : sf::Color(200, 200, 200));
        auto nb = name.getLocalBounds();
        name.setPosition({cx + CARD_W/2.f - nb.size.x/2.f, cy + CARD_H - 22.f});
        m_window.draw(name);
    }

    // hint
    sf::Text hint(m_font, "[WASD] Move   [Enter] Confirm", 9);
    hint.setFillColor(sf::Color(100, 100, 100));
    auto hb = hint.getLocalBounds();
    hint.setPosition({(float)WINDOW_W/2.f - hb.size.x/2.f, (float)WINDOW_H - 24.f});
    m_window.draw(hint);
}