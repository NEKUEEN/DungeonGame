#include "Game.hpp"
#include "TextureManager.hpp"
#include "MonsterDB.hpp"
#include "StatusEffect.hpp"
#include "StatusEffectDB.hpp"
#include "DropTable.hpp"
#include "Party.hpp"
#include <iostream>
#include <algorithm>
#include <optional>
#include <cmath>
#include <chrono>
#include <queue>
#include <unordered_map>

// ── ขนาดหน้าต่าง/เลย์เอาต์จริง (ปรับได้ตอนรันไทม์ ดู Game::refreshWindowMetrics) ──
// ค่าเริ่มต้นนี้ถูกแทนที่ทันทีใน constructor ด้วยความละเอียดจอจริงของเครื่อง
// (เกมเป็นฟูลสกรีนเสมอ ไม่มี F11 toggle แล้ว — ปรับอัตโนมัติแบบ DCSS)
int WINDOW_W    = BASE_WINDOW_W;
int WINDOW_H    = BASE_WINDOW_H;
int GAME_VIEW_W = BASE_WINDOW_W - RIGHT_PANEL_W;
int GAME_VIEW_H = BASE_WINDOW_H - LOG_PANEL_H;
int STATUS_PANEL_H = (BASE_WINDOW_H - LOG_PANEL_H) - INV_GRID_H;

// ── ใช้แปลง sf::Keyboard::Key (A..Z) → index 0-25 สำหรับหน้าจอเลือกสกิลแบบ DCSS ──
// เขียนแบบ explicit switch (ไม่พึ่ง underlying enum value) กันปัญหาข้าม SFML version
static int letterKeyIndex(sf::Keyboard::Key code)
{
    switch (code)
    {
        case sf::Keyboard::Key::A: return 0;
        case sf::Keyboard::Key::B: return 1;
        case sf::Keyboard::Key::C: return 2;
        case sf::Keyboard::Key::D: return 3;
        case sf::Keyboard::Key::E: return 4;
        case sf::Keyboard::Key::F: return 5;
        case sf::Keyboard::Key::G: return 6;
        case sf::Keyboard::Key::H: return 7;
        case sf::Keyboard::Key::I: return 8;
        case sf::Keyboard::Key::J: return 9;
        case sf::Keyboard::Key::K: return 10;
        case sf::Keyboard::Key::L: return 11;
        case sf::Keyboard::Key::M: return 12;
        case sf::Keyboard::Key::N: return 13;
        case sf::Keyboard::Key::O: return 14;
        case sf::Keyboard::Key::P: return 15;
        case sf::Keyboard::Key::Q: return 16;
        case sf::Keyboard::Key::R: return 17;
        case sf::Keyboard::Key::S: return 18;
        case sf::Keyboard::Key::T: return 19;
        case sf::Keyboard::Key::U: return 20;
        case sf::Keyboard::Key::V: return 21;
        case sf::Keyboard::Key::W: return 22;
        case sf::Keyboard::Key::X: return 23;
        case sf::Keyboard::Key::Y: return 24;
        case sf::Keyboard::Key::Z: return 25;
        default: return -1;
    }
}

std::vector<std::pair<int,int>> Game::findPath(int sc, int sr, int ec, int er)
{
    int COLS = m_tileMap.getCols();
    int ROWS = m_tileMap.getRows();

    using P = std::pair<int,int>;
    auto h   = [&](int c, int r){ return std::abs(c-ec)+std::abs(r-er); };
    auto key = [&](int c, int r){ return r * COLS + c; };

    std::priority_queue<std::tuple<int,int,int>, std::vector<std::tuple<int,int,int>>, std::greater<>> open;
    std::unordered_map<int,int> cost;
    std::unordered_map<int,int> came;

    open.push({0, sc, sr});
    cost[key(sc,sr)] = 0;

    const int dx[] = {0,0,1,-1,1,1,-1,-1};
    const int dy[] = {1,-1,0,0,1,-1,1,-1};

    while (!open.empty())
    {
        auto [f, c, r] = open.top(); open.pop();
        if (c==ec && r==er) break;

        for (int i = 0; i < 8; ++i)
        {
            int nc = c+dx[i], nr = r+dy[i];
            if (nc<0||nr<0||nc>=COLS||nr>=ROWS) continue;
            if (!m_tileMap.isWalkable(nc, nr)) continue;

            int newCost = cost[key(c,r)] + 1;
            int nk = key(nc,nr);
            if (!cost.count(nk) || newCost < cost[nk])
            {
                cost[nk] = newCost;
                came[nk] = key(c,r);
                open.push({newCost + h(nc,nr), nc, nr});
            }
        }
    }

    std::vector<P> path;
    int cur   = key(ec,er);
    int start = key(sc,sr);
    if (!came.count(cur)) return {};

    while (cur != start)
    {
        path.push_back({cur % COLS, cur / COLS});
        cur = came[cur];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// ============================================================
//  Constructor / Destructor
// ============================================================
Game::Game()
    : m_window(sf::VideoMode::getDesktopMode(), "Dungeon and Stone",
               sf::Style::None)
    , m_tileMap(MAP_COLS, MAP_ROWS, TILE_SIZE)
    , m_fog(MAP_COLS, MAP_ROWS)
    , m_player(nullptr)
{
    m_window.setFramerateLimit(60);
    m_fontLoaded = m_font.openFromFile("assets/fonts/font.ttf");
    if (!m_fontLoaded) std::cerr << "[Game] Warning: font not loaded\n";

    // เต็มจอเสมอ ปรับตามความละเอียดจอจริงของแต่ละเครื่องอัตโนมัติ (แบบ DCSS)
    auto desktop = sf::VideoMode::getDesktopMode();
    refreshWindowMetrics((int)desktop.size.x, (int)desktop.size.y);

    TextureManager::instance().loadAll();
    MonsterDB::instance().load("assets/data/monsters.json");
    DropTable::instance().loadItems("assets/data/items.json");
    SkillDB::instance().load("assets/data/skills.json");
    RaceDB::instance().load("assets/data/races.json");
    StatusEffectDB::instance().load("assets/data/status_effects.json");
    NPCDB::instance().load("assets/data/npcs.json");
    std::cerr << "[RaceDB] loaded: " << RaceDB::instance().getAll().size() << " races\n";

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
    m_finalStats.bleedBonus  = bonus.bleedBonus;
    m_finalStats.poisonBonus = bonus.poisonBonus;
    m_finalStats.resistBleed  = bonus.resistBleed;
    m_finalStats.resistPoison = bonus.resistPoison;
    m_finalStats.resistBurn   = bonus.resistBurn;
    m_finalStats.resistStun   = bonus.resistStun;
    m_finalStats.resistSlow   = bonus.resistSlow;
    m_finalStats.bleedDmgReduce = bonus.bleedDmgReduce;
    m_finalStats.bleedDurReduce = bonus.bleedDurReduce;
    m_finalStats.poisonDmgReduce = bonus.poisonDmgReduce;
    m_finalStats.poisonDurReduce = bonus.poisonDurReduce;
    m_finalStats.burnDmgReduce = bonus.burnDmgReduce;
    m_finalStats.burnDurReduce = bonus.burnDurReduce;
    m_finalStats.stunDurReduce = bonus.stunDurReduce;
    m_finalStats.slowDurReduce = bonus.slowDurReduce;
    m_finalStats.slashDmgBonus  = bonus.slashDmgBonus;
    m_finalStats.pierceDmgBonus = bonus.pierceDmgBonus;
    m_finalStats.bluntDmgBonus  = bonus.bluntDmgBonus;
    m_finalStats.cleaveDmgBonus = bonus.cleaveDmgBonus;


    int atkPercentBonus = 0, defPercentBonus = 0, magicPercentBonus = 0, hpPercentBonus = 0;
    for (const auto& sk : m_player->getSkills())
    {
        if (sk.data.type == SkillType::Passive  ||
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
//ตรงนี้คำนวน
    m_finalStats.body = (m_finalStats.maxHp * 0.4f + m_finalStats.atk * 0.3f +
                              m_finalStats.def * 0.2f + m_finalStats.dodge * 0.1f);
    m_finalStats.mentality = (m_finalStats.maxMana * 0.4f + 
                                m_finalStats.matk * 0.3f +
                                m_finalStats.mdef * 0.3f +
                               (m_finalStats.resistBleed  +
                                m_finalStats.resistPoison +
                                m_finalStats.resistBurn   +
                                m_finalStats.resistStun   +
                                m_finalStats.resistSlow)  * 0.02f);

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
// ── helper: แปลง spd bonus → Aut ──
static int spdToAut(int spd)
{
    if (spd >= 0) return std::max(50,  100 * 6 / (6 + spd));
    else          return std::min(200, 100 - spd * 20);
}

void Game::recalcSpeed()
{
    if (!m_player) return;
    Stats& s = m_player->getStats();

    // รวม moveSpd จาก equipment + passive/active buff skills
    int ms = m_finalStats.spd;
    //if (m_player->hasStatus(StatusType::Slow))
        //ms -= -2;
    int as = 0;
    for (const auto& sk : m_player->getSkills())
    {
        if (!sk.buffActive && !sk.isPassive()) continue;
        ms += sk.data.effect.speedBonus;
    }
    s.moveSpd = std::clamp(ms, -4, 4);
    s.atkSpd  = std::clamp(as, -4, 4);
    s.moveAut = spdToAut(s.moveSpd);

    // atkAut = baseAtkAut ของอาวุธ scale ด้วย atkSpd
    const Item* mainHand = m_equipment.getItem(EquipSlot::MainHand);
    int weaponBaseAut = mainHand ? mainHand->baseAtkAut : 100;
    if (s.atkSpd >= 0)
        s.atkAut = std::max(50,  weaponBaseAut * 6 / (6 + s.atkSpd));
    else
        s.atkAut = std::min(300, weaponBaseAut - s.atkSpd * 20);

    s.staminaRegen = m_finalStats.staminaRegen;
    if (s.maxStamina == 0) s.maxStamina = m_finalStats.maxStamina;
    if (s.stamina > s.maxStamina) s.stamina = s.maxStamina;

    // ── Weight penalty ──
    Stats& st = m_player->getStats();
    const RaceData* race = RaceDB::instance().get(m_selectedRace);
    st.strength       = race ? race->statBonus.strength   : 10;
    st.bodyWeight     = race ? race->statBonus.bodyWeight  : 70;
    st.maxCarryWeight = st.strength * 5;

    // รวม equipment weight
    int ew = 0;
    for (auto slot : {EquipSlot::Head, EquipSlot::Body, EquipSlot::Arms,
                      EquipSlot::Legs, EquipSlot::Feet, EquipSlot::MainHand,
                      EquipSlot::OffHand})
    {
        const Item* it = m_equipment.getItem(slot);
        if (it) ew += it->weight;
    }
    st.equipWeight = ew;

    // penalty จาก (bodyWeight + equipWeight) / maxCarryWeight
    float ratio = (float)(st.bodyWeight + st.equipWeight) / (float)st.maxCarryWeight;
    int weightPenalty = ratio <= 1.0f ? 0 :
                        ratio <= 1.3f ? -1 :
                        ratio <= 1.6f ? -2 : -3;

    // apply penalty เข้า moveSpd
    s.moveSpd = std::clamp(ms + weightPenalty, -4, 4);
    s.moveAut = spdToAut(s.moveSpd);
}

void Game::regenStamina()
{
    if (!m_player) return;
    Stats& s = m_player->getStats();
    s.stamina = std::min(s.maxStamina, s.stamina + s.staminaRegen);
}

// ============================================================
//  Vision / Fog — ลด radius ถ้า player อยู่ในโซนมืด (ยังไม่มีระบบไฟ)
// ============================================================
int Game::getCurrentVisionRadius() const
{
    if (!m_player) return VIEW_RADIUS;
    ZoneType zone = m_tileMap.getZone(m_player->getCol(), m_player->getRow());
    if (zone == ZoneType::Darkness) return DARK_ZONE_VIEW_RADIUS;
    return VIEW_RADIUS;
}

void Game::recomputeFog()
{
    if (!m_player) return;
    recomputeFog(m_player->getCol(), m_player->getRow());
}

void Game::recomputeFog(int col, int row)
{
    ZoneType zone = m_tileMap.getZone(col, row);
    int radius = (zone == ZoneType::Darkness) ? DARK_ZONE_VIEW_RADIUS : VIEW_RADIUS;
    m_fog.compute(col, row, radius, m_tileMap);
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
               sf::Color(225,222,208));
        return;
    }
    sk->buffActive   = true;
    sk->durationLeft = sk->data.duration;
    sk->cooldownLeft = sk->data.cooldown;
    addLog("  "+sk->data.name+" activated! ATK +"+std::to_string(sk->data.effect.atkPct)+
           "% for "+std::to_string(sk->data.duration)+" turns", sf::Color(225,222,208));
    recalcSpeed();
}

// ============================================================
//  executeSkill
// ============================================================
void Game::executeSkillById(const std::string& id)
{
    if (!m_player) return;
    if (id.empty()) { addLog("  No skill selected."); return; }

    SkillInstance* sk = m_player->findSkill(id);
    if (!sk) return;

    Stats& s = m_player->getStats();

    if (s.mana < sk->data.effect.manaCost)
    {
        addLog("  Not enough mana! Need " + std::to_string(sk->data.effect.manaCost), sf::Color(225,222,208));
        return;
    }

    if (s.stamina < sk->data.effect.staminaCost)
    {
        addLog("  Not enough stamina! Need " + std::to_string(sk->data.effect.staminaCost) +
               " (have " + std::to_string(s.stamina) + ")", sf::Color(225,222,208));
        return;
    }

    if (!sk->isReady())
    {
        addLog("  "+sk->data.name+" cooldown: "+std::to_string(sk->cooldownLeft)+" turns", sf::Color(225,222,208));
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
        addLog("  "+sk->data.name+" activated!", sf::Color(225,222,208));
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
            addLog("  "+sk->data.name+" +"+std::to_string(heal)+" HP", sf::Color(225,222,208));
        }
        break;

    case SkillType::ActiveRanged:
    case SkillType::ActiveWarp:
        s.mana -= sk->data.effect.manaCost;
        m_ui.targeting.active  = true;
        m_ui.targeting.skillId = id;
        m_ui.targeting.isArea     = false;   // ← เพิ่ม
        m_ui.targeting.areaRadius = 0;       // ← เพิ่ม

        // ── auto-target ศัตรูใกล้สุดในระยะ ──
        {
            int range = sk->data.effect.range;
            int pc = m_player->getCol();
            int pr = m_player->getRow();
            Enemy* nearest = nullptr;
            float  nearDist = 999.f;

            for (auto* e : m_enemies)
            {
                if (e->isDead()) continue;
                float d = std::sqrt((float)((e->getCol()-pc)*(e->getCol()-pc) +
                                        (e->getRow()-pr)*(e->getRow()-pr)));
                if (d <= range && d < nearDist)
                {
                    nearDist = d;
                    nearest  = e;
                }
            }

            if (nearest)
            {
                m_ui.targeting.targetCol = nearest->getCol();
                m_ui.targeting.targetRow = nearest->getRow();
            }
            else
            {
                m_ui.targeting.targetCol = pc;
            m_ui.targeting.targetRow = pr;
            }
        }

        if (sk->data.type == SkillType::ActiveRanged)
            addLog(" [Targeting] " + sk->data.name, sf::Color(225,222,208));
        else
            addLog(" [Warp] " + sk->data.name, sf::Color(225,222,208));
        break;

            case SkillType::ActiveAoe:
            s.mana -= sk->data.effect.manaCost;
            // หัก stamina ตอน confirm จริง — เหมือน Warp
            m_ui.targeting.active     = true;
            m_ui.targeting.skillId    = id;
            m_ui.targeting.isArea     = true;
            m_ui.targeting.areaRadius = sk->data.effect.aoeRadius;
            m_ui.targeting.locked     = false;
            m_ui.targeting.targetCol  = m_player->getCol();
            m_ui.targeting.targetRow  = m_player->getRow();
            addLog(" [AOE Targeting] " + sk->data.name, sf::Color(225,222,208));
            break;

            case SkillType::ActiveAoeSelf:
            s.mana -= sk->data.effect.manaCost;
            // หัก stamina ตอน confirm จริง — เหมือนกัน
            m_ui.targeting.active     = true;
            m_ui.targeting.skillId    = id;
            m_ui.targeting.isArea     = true;
            m_ui.targeting.areaRadius = sk->data.effect.aoeRadius;
            m_ui.targeting.locked     = true;     // ← ล็อกอยู่กับตัว ขยับไม่ได้
            m_ui.targeting.targetCol  = m_player->getCol();
            m_ui.targeting.targetRow  = m_player->getRow();
            addLog(" [AOE] " + sk->data.name + " — press again to release", sf::Color(225,222,208));
            break;

            case SkillType::ActiveAoeWarp:
            s.mana -= sk->data.effect.manaCost;
            // หัก stamina ตอน confirm จริง — เหมือน Warp/AOE
            m_ui.targeting.active     = true;
            m_ui.targeting.skillId    = id;
            m_ui.targeting.isArea     = true;
            m_ui.targeting.areaRadius = sk->data.effect.aoeRadius;
            m_ui.targeting.locked     = false;   // เลือกตำแหน่งที่จะโดดไปได้
            m_ui.targeting.targetCol  = m_player->getCol();
            m_ui.targeting.targetRow  = m_player->getRow();
            addLog(" [Leap] " + sk->data.name, sf::Color(225,222,208));
            break;

        case SkillType::Passive:
            addLog("  "+sk->data.name+" is passive.", sf::Color(225,222,208));
        break;
    }
}

// ============================================================
//  DCSS-style skill select / F-key system
// ============================================================
void Game::selectSkillByLetter(char letter)
{
    if (!m_player) return;
    std::string id = m_player->getSkillIdByLetter(letter);
    if (id.empty())
    {
        addLog(std::string("  Nothing on '") + letter + "'.", sf::Color(225,222,208));
        return;
    }

    m_player->setSelectedSkillId(id);
    m_ui.fMode = UIState::FMode::Skill;   // เลือกสกิลแล้ว = ตั้งใจจะใช้มัน → สลับ F เข้าโหมด Skill ให้อัตโนมัติ
    m_ui.skillSelectOpen = false;   // พับหน้าต่างทันทีหลังเลือก

    SkillInstance* sk = m_player->findSkill(id);
    addLog("  Selected: " + (sk ? sk->data.name : id) + " ["+std::string(1,letter)+"] — press F to use",
           sf::Color(225,222,208));
}

void Game::executeSelectedSkill()
{
    if (!m_player) return;
    const std::string& id = m_player->getSelectedSkillId();
    if (id.empty())
    {
        addLog("  No skill selected. Press Shift+Q to choose one.", sf::Color(225,222,208));
        return;
    }
    executeSkillById(id);
}

void Game::toggleFMode()
{
    if (m_ui.fMode == UIState::FMode::Bow)
    {
        m_ui.fMode = UIState::FMode::Skill;
        const std::string& id = m_player ? m_player->getSelectedSkillId() : std::string();
        if (id.empty())
            addLog("  [F] mode: Skill (none selected — press Shift+Q)", sf::Color(225,222,208));
        else
        {
            SkillInstance* sk = m_player->findSkill(id);
            addLog("  [F] mode: Skill (" + (sk ? sk->data.name : id) + ")", sf::Color(225,222,208));
        }
    }
    else
    {
        m_ui.fMode = UIState::FMode::Bow;
        addLog("  [F] mode: Bow", sf::Color(225,222,208));
    }
}

void Game::applyStatusOnHit(SkillInstance* sk, Enemy* e)
{
    if (!sk || !e) return;
    if (sk->data.effect.applyStatus.empty() || sk->data.effect.statusDuration <= 0) return;

    const std::string& st = sk->data.effect.applyStatus;
    StatusType stype;
    bool valid = true;

    if      (st == "bleed")  stype = StatusType::Bleed;
    else if (st == "poison") stype = StatusType::Poison;
    else if (st == "burn")   stype = StatusType::Burn;
    else if (st == "stun")   stype = StatusType::Stun;
    else if (st == "slow")   stype = StatusType::Slow;
    else valid = false;

    if (!valid) return;

    StatusEffect se;
    se.type = stype;
    int bonus = 0;
    if (st == "bleed")  bonus = m_finalStats.bleedBonus;
    if (st == "poison") bonus = m_finalStats.poisonBonus;
    if (st == "burn")   bonus = m_finalStats.burnBonus;
    se.power    = sk->data.effect.statusPower + bonus;
    se.duration = sk->data.effect.statusDuration;
    se.sourceId = sk->data.id;
    e->applyStatus(se);
    addLog("  " + e->getName() + " is " + st + "!", sf::Color(225,222,208));
}

void Game::applyKnockback(Enemy* e, int sourceCol, int sourceRow, int tiles)
{
    if (!e || tiles <= 0) return;
    if (e->isDead()) return;   // ศพไม่ผลัก

    int dx = e->getCol() - sourceCol;
    int dy = e->getRow() - sourceRow;

    // normalize เป็นทิศทางกริด 8 ทิศ (-1/0/1)
    int stepX = (dx > 0) - (dx < 0);
    int stepY = (dy > 0) - (dy < 0);
    if (stepX == 0 && stepY == 0) return;   // อยู่จุดเดียวกับศูนย์กลาง ไม่รู้จะผลักทางไหน

    int curC = e->getCol();
    int curR = e->getRow();
    int pushed = 0;

    for (int i = 0; i < tiles; ++i)
    {
        int nc = curC + stepX;
        int nr = curR + stepY;

        if (!m_tileMap.isWalkable(nc, nr)) break;
        if (m_player && m_player->getCol() == nc && m_player->getRow() == nr) break;

        bool blocked = false;
        for (auto* other : m_enemies)
        {
            if (other == e || other->isDead()) continue;
            if (other->getCol() == nc && other->getRow() == nr) { blocked = true; break; }
        }
        if (blocked) break;

        curC = nc;
        curR = nr;
        pushed++;
    }

    if (pushed > 0)
    {
        e->setPos(curC, curR);
        addLog("  " + e->getName() + " is knocked back!", sf::Color(225,222,208));
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
                   sf::Color(225,222,208));
            applyStatusOnHit(sk, e);
            if (sk->data.effect.knockbackTiles > 0)
                applyKnockback(e, pc, pr, sk->data.effect.knockbackTiles);
            hits++;
        }
    }
    if (hits == 0)
        addLog("  "+sk->data.name+"  no targets in range.", sf::Color(225,222,208));

    {
        Stats& ps = m_player->getStats();
        
        m_globalTime = ps.nextActTime;
        ps.nextActTime += ps.atkAut;
    }

    processTurn(); tryRespawnEnemies();
    recomputeFog();
}

void Game::executeAoeAt(SkillInstance* sk, int col, int row)
{
    if (!m_player || !sk) return;
    int radius  = sk->data.effect.aoeRadius;
    int baseAtk = getBuffedAtk();
    int dmg = std::max(1, baseAtk * sk->data.effect.damagePct / 100);
    int hits = 0;

    for (auto* e : m_enemies)
    {
        if (e->isDead()) continue;
        int dx = e->getCol() - col, dy = e->getRow() - row;
        if (dx*dx + dy*dy <= radius*radius)
        {
            e->takeDamage(dmg);
            addLog("  "+sk->data.name+" hits "+e->getName()+" for "+std::to_string(dmg)+"!",
                   sf::Color(225,222,208));
            applyStatusOnHit(sk, e);
            if (sk->data.effect.knockbackTiles > 0)
                applyKnockback(e, col, row, sk->data.effect.knockbackTiles);
            hits++;
        }
    }
    if (hits == 0)
        addLog("  "+sk->data.name+"  no targets in range.", sf::Color(225,222,208));

    Stats& ps = m_player->getStats();
    
    m_globalTime = ps.nextActTime;
    ps.nextActTime += ps.atkAut;

    processTurn(); tryRespawnEnemies();
    recomputeFog();
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
    addLog("  Warped!", sf::Color(225,222,208));
    recomputeFog(col, row);
    updateCamera();

    {
        Stats& ps = m_player->getStats();
        
        m_globalTime = ps.nextActTime;
        ps.nextActTime += ps.atkAut;
    }

    processTurn();
    tryRespawnEnemies();
}

void Game::executeAoeWarp(SkillInstance* sk, int col, int row)
{
    if (!m_player || !sk) return;

    if (!m_tileMap.isWalkable(col, row))
    {
        addLog("  Cannot leap there!", sf::Color(225,222,208));
        return;
    }

    m_player->setPos(col, row);
    addLog("  " + sk->data.name + " — leap!", sf::Color(225,222,208));
    updateCamera();

    executeAoeAt(sk, col, row);   // ดาเมจ + status + knockback + processTurn + fog ครบในตัว

    m_player->getStats().stamina -= sk->data.effect.staminaCost;
    sk->cooldownLeft = sk->data.cooldown;
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
void Game::moveTargetCursor(int dc, int dr)
{
    if (!m_player) return;
    if (m_ui.targeting.locked) return;   // ← ล็อกอยู่กับตัว ห้ามขยับ

    int nc = m_ui.targeting.targetCol + dc;
    int nr = m_ui.targeting.targetRow + dr;

    // ── คำนวณ range ──
    int range = 8; // default สำหรับ bow
    if (m_ui.targeting.skillId != "_bow_attack")
    {
        SkillInstance* sk = m_player->findSkill(m_ui.targeting.skillId);
        if (!sk) return;
        range = sk->data.effect.range;
    }

    float dist = std::sqrt((float)((nc - m_player->getCol())*(nc - m_player->getCol()) +
                                   (nr - m_player->getRow())*(nr - m_player->getRow())));
    if (dist > range)
    {
        addLog("  Out of range (max " + std::to_string(range) + " tiles)", sf::Color(225,222,208));
        return;
    }

    nc = std::clamp(nc, 0, m_mapCols - 1);
    nr = std::clamp(nr, 0, m_mapRows - 1);
    m_ui.targeting.targetCol = nc;
    m_ui.targeting.targetRow = nr;
}

void Game::confirmTarget()
{
    if (!m_player) return;

    if (m_ui.targeting.skillId == "_bow_attack")
    {
        m_ui.targeting.reset();
        fireBow();
        return;
    }

    SkillInstance* sk = m_player->findSkill(m_ui.targeting.skillId);
    int tc = m_ui.targeting.targetCol;
    int tr = m_ui.targeting.targetRow;
    SkillType type = sk ? sk->data.type : SkillType::Passive;

    if (!sk) { m_ui.targeting.reset(); return; }

    if (type == SkillType::ActiveRanged)
        fireRangedAt(tc, tr);
    else if (type == SkillType::ActiveWarp)
        executeWarp(tc, tr);
    else if (type == SkillType::ActiveAoe || type == SkillType::ActiveAoeSelf)
    {
        executeAoeAt(sk, tc, tr);
        m_player->getStats().stamina -= sk->data.effect.staminaCost;
        sk->cooldownLeft = sk->data.cooldown;
    }
    else if (type == SkillType::ActiveAoeWarp)
        executeAoeWarp(sk, tc, tr);

    m_ui.targeting.reset();   // เคลียร์หลังยิงเสร็จ กัน state ค้าง
}

void Game::cancelTargeting()
{
    m_ui.targeting.reset();
    addLog("  Targeting cancelled.", sf::Color(225,222,208));
}

// ============================================================
//  Bow System
// ============================================================
void Game::enterBowTargeting()
{
    if (!m_player) return;

    const Item* mainHand = m_equipment.getItem(EquipSlot::MainHand);
    if (!mainHand || mainHand->id.find("bow") == std::string::npos)
    {
        addLog("  No bow equipped!", sf::Color(225,222,208));
        return;
    }

    // เช็ค arrow ใน inventory
    bool hasArrow = false;
    for (int i = 0; i < m_inventory.size(); ++i)
    {
        Item* it = m_inventory.getItem(i);
        if (it && it->type == ItemType::Ammo) { hasArrow = true; break; }
    }

    if (!hasArrow)
    {
        addLog("  No arrows in inventory!", sf::Color(225,222,208));
        return;
    }

    m_ui.targeting.active    = true;
    m_ui.targeting.skillId   = "_bow_attack";
    m_ui.targeting.targetCol = m_player->getCol();
    m_ui.targeting.targetRow = m_player->getRow();

    addLog("  [Bow] hjkl/yubn=move  Enter=fire  Esc/F=cancel",
           sf::Color(225,222,208));
}

void Game::fireBow()
{
    if (!m_player) return;

    // หา arrow ใน inventory
    int arrowSlot = -1;
    for (int i = 0; i < m_inventory.size(); ++i)
    {
        Item* it = m_inventory.getItem(i);
        if (it && it->type == ItemType::Ammo)
        {
            arrowSlot = i;
            break;
        }
    }

    if (arrowSlot == -1)
    {
        addLog("  No arrows in inventory!", sf::Color(225,222,208));
        return;
    }

    const int BOW_RANGE = 8;
    int pc = m_player->getCol();
    int pr = m_player->getRow();
    int tc = m_ui.targeting.targetCol;
    int tr = m_ui.targeting.targetRow;

    float dist = std::sqrt((float)((tc - pc)*(tc - pc) + (tr - pr)*(tr - pr)));
    if (dist > BOW_RANGE)
    {
        addLog("  Out of range! (max " + std::to_string(BOW_RANGE) + " tiles)",
               sf::Color(225,222,208));
        return;
    }

    // หักลูกธนูจาก inventory
    m_inventory.removeItem(arrowSlot);
    int arrowLeft = m_inventory.getCount(arrowSlot); // เหลือเท่าไหร่

    int dmg  = std::max(1, getBuffedAtk() * 90 / 100);
    bool hit = false;

    auto line = getLine(pc, pr, tc, tr);

    for (int i = 1; i < (int)line.size() && !hit; ++i)
    {
        int tx = line[i].x;
        int ty = line[i].y;

        if (m_tileMap.getTile(tx, ty) == TileType::Wall) break;

        float d = std::sqrt((float)((tx - pc)*(tx - pc) + (ty - pr)*(ty - pr)));
        if (d > BOW_RANGE) break;

        for (auto* e : m_enemies)
        {
            if (e->isDead() || e->getCol() != tx || e->getRow() != ty) continue;

            e->takeDamage(dmg);
            addLog("  Arrow hits " + e->getName() + " for " + std::to_string(dmg) + "!",
                   sf::Color(225,222,208));
            hit = true;

            if (e->isDead())
                handleEnemyDeath(e);
            break;
        }
    }

    if (!hit)
        addLog("  Arrow flies into the dark.", sf::Color(225,222,208));

    Stats& ps = m_player->getStats();
    
    m_globalTime = ps.nextActTime;
    ps.nextActTime += ps.atkAut;

    processTurn();
    tryRespawnEnemies();
    recomputeFog();
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

                std::string hitLog = "  " + sk->data.name + " hits " + e->getName() + " for " + std::to_string(projDmg) + "!";
                // ── Apply status effect on hit ──
                if (!sk->data.effect.applyStatus.empty() && sk->data.effect.statusDuration > 0)
                {
                const std::string& st = sk->data.effect.applyStatus;
                StatusType stype;
                bool valid = true;

                if      (st == "bleed")  stype = StatusType::Bleed;
                else if (st == "poison") stype = StatusType::Poison;
                else if (st == "burn")   stype = StatusType::Burn;
                else if (st == "stun")   stype = StatusType::Stun;
                else if (st == "slow")   stype = StatusType::Slow;
                else valid = false;

                if (valid)
                {
                    StatusEffect se;
                    se.type     = stype;
                    int bonus = 0;
                    if (st == "bleed")  bonus = m_finalStats.bleedBonus;  // ← เพิ่มใน FinalStats ด้วย
                    if (st == "poison") bonus = m_finalStats.poisonBonus;
                    if (st == "burn")   bonus = m_finalStats.burnBonus;
                    se.power = sk->data.effect.statusPower + bonus;
                    se.duration = sk->data.effect.statusDuration;
                    se.sourceId = sk->data.id;
                    e->applyStatus(se);
                        addLog("  " + e->getName() + " is " + st + "!", sf::Color(225,222,208));
                }
            }
                        addLog(hitLog, sf::Color(225,222,208));  // ← log เดียว

                if (!e->isDead() && sk->data.effect.knockbackTiles > 0)
                    applyKnockback(e, pc, pr, sk->data.effect.knockbackTiles);

                if (e->isDead())
                    handleEnemyDeath(e);
                hit = true;
                break;
            }
        }
        if (hit) break;
    }

    if (!hit)
        addLog("  " + sk->data.name + " hits nothing.", sf::Color(225,222,208));

    // หัก spdCounter เหมือนขยับ
    {
        Stats& ps = m_player->getStats();
        
        m_globalTime = ps.nextActTime;
        ps.nextActTime += ps.atkAut;
    }

    m_turnCount++;
    m_player->onTurnPassed();
    processTurn();
    tryRespawnEnemies();
    recomputeFog();
}

// ============================================================
//  Dungeon Setup
// ============================================================
void Game::newDungeon(bool keepPlayer)
{
    // Game.cpp บรรทัด 1107-1115 แก้เป็น:

    {
        std::string path = "assets/data/50map_floor" + std::to_string(m_dungeonFloor) + ".tmj";
        if (!m_tileMap.loadFromTiled(path))
            m_tileMap.generate();
    }

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
        m_floorItemsSaved.clear();  // เกมใหม่/restart → ของเก่าที่วางไว้ไม่ควรข้ามมา
        clearEnemies();  // ล้าง enemies ของ run เดิมที่ยังไม่ตาย
        for (auto& kv : m_floorEnemiesSaved)
            for (auto* e : kv.second) delete e;
        m_floorEnemiesSaved.clear();
        m_floorNPCsSaved.clear();  // shared_ptr เอง ไม่ต้อง delete มือ
        m_log.clear();   // เคลียร์ log ทุกครั้งที่เริ่ม dungeon ใหม่ (รวมถึงตอน restart หลังตาย)
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
        // ── Debug/testing: สกิลไหนใน skills.json ตั้ง "hotbar_slot" ไม่เป็น -1 → ยัดให้ตัวละครทันที ──
        // (เทสไวๆ แบบเดิม: แค่แก้เลขในไฟล์ json ก็โผล่ที่ตัวละครเลย ไม่ต้องสร้างไอเทม core ผูกสกิล)
        for (const auto& sd : SkillDB::instance().getAll())
        {
            if (sd.hotbarSlot != -1 && !m_player->findSkill(sd.id))
                m_player->addCoreSkill(sd.id);
        }
        // ใน newDungeon() หลัง new Player
        //StatusEffect test;
        //test.type     = StatusType::Bleed;
        //test.power    = 3;
        //test.duration = 10;
        //test.sourceId = "test";
        //m_player->getStats().statusEffects.push_back(test);
    }

    // ── Enemies: ถ้าเคยมาชั้นนี้แล้วในรอบนี้ คืนตัวที่เหลือรอดกลับมาแทน spawn ใหม่ ──
    auto savedEnemiesIt = m_floorEnemiesSaved.find(m_dungeonFloor);
    if (savedEnemiesIt != m_floorEnemiesSaved.end())
    {
        m_enemies = savedEnemiesIt->second;
        m_floorEnemiesSaved.erase(savedEnemiesIt);
    }
    else
    {
        clearEnemies();  // เผื่อ m_enemies ยังไม่ว่าง (ปกติจะว่างจากตอนเซฟก่อนย้ายแล้ว)
        spawnEnemies(8+m_dungeonFloor);
    }

    // ── ของบนพื้น: ถ้าเคยมาชั้นนี้มาก่อน คืนของที่วางไว้กลับมาแทน spawn ใหม่ ──
    auto savedItemsIt = m_floorItemsSaved.find(m_dungeonFloor);
    if (savedItemsIt != m_floorItemsSaved.end())
        m_mapItems = savedItemsIt->second;
    else
    {
        m_mapItems.clear();
        spawnItems();
    }

    // ── NPC (ที่ยังไม่ recruit): เหมือนกัน คืนตัวเดิมที่ยืนอยู่กลับมา ──
    auto savedNPCsIt = m_floorNPCsSaved.find(m_dungeonFloor);
    m_npcManager.clear();
    if (savedNPCsIt != m_floorNPCsSaved.end())
    {
        for (auto& npc : savedNPCsIt->second) m_npcManager.add(npc);
        m_floorNPCsSaved.erase(savedNPCsIt);
    }
    else
    {
        spawnNPCs(3);
    }
    m_respawnTimer=0;

    if (m_player)
    {
        recalcAllStats();
        recalcSpeed();
        m_coreSlots.setSlotCount(m_player->getStats().level);
        recomputeFog();
    }

    updateCamera();
    m_ui.activePanel = UIState::Panel::Inventory;
    m_ui.selectedInvSlot = 0;
    m_ui.selectedEquipSlot = 0;
    m_ui.selectedCoreSlot = 0;
    m_ui.levelUpFlash = false;
    m_ui.targeting.reset();

    if (keepPlayer)
        addLog("  Floor "+std::to_string(m_dungeonFloor)+" - Deeper...", sf::Color(225,222,208));
    else
        addLog("  Floor 1", sf::Color(225,222,208));
}

void Game::applyLetterbox()
{
    auto winSize = m_window.getSize();
    float winW = (float)winSize.x;
    float winH = (float)winSize.y;
    // Fit/Letterbox: ใช้ min เพื่อให้เห็นภาพเกมครบทุกส่วน ไม่ตัดขอบ UI
    // (เหมือนตอนขยายหน้าต่างจาก title bar) — จะมีแถบดำถ้าสัดส่วนจอไม่ตรงกับเกม
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

// ── อัปเดต WINDOW_W/H และค่าที่คำนวณต่อ (GAME_VIEW_W/H, STATUS_PANEL_H) ──
// เรียกครั้งเดียวตอนเปิดเกม (constructor) ด้วยความละเอียดจอจริงของเครื่องนั้นๆ
// เพื่อให้เกมเต็มจอสนิทเสมอแบบ DCSS ไม่ scale ภาพ 800x600 เดิมให้ยืด/หด
// ส่วน panel ต่างๆ (RIGHT_PANEL_W, LOG_PANEL_H) ยังคงขนาด pixel เท่าเดิมเสมอ
void Game::refreshWindowMetrics(int w, int h)
{
    WINDOW_W       = w;
    WINDOW_H       = h;
    GAME_VIEW_W    = WINDOW_W - RIGHT_PANEL_W;
    GAME_VIEW_H    = WINDOW_H - LOG_PANEL_H;
    STATUS_PANEL_H = GAME_VIEW_H - INV_GRID_H;

    m_gameView.setSize({(float)GAME_VIEW_W * 1.5f, (float)GAME_VIEW_H * 1.5f});
    m_uiView.setSize({(float)WINDOW_W, (float)WINDOW_H});
    m_uiView.setCenter({(float)WINDOW_W / 2.f, (float)WINDOW_H / 2.f});
    applyLetterbox();
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
        addLog("  [" + family + "] Boss not defined in JSON!", sf::Color(225,222,208));
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
        addLog("  *** " + family + " BOSS APPEARS! ***", sf::Color(225,222,208));
        addLog("  The slaughter has summoned a guardian!", sf::Color(225,222,208));
        return;
    }
}

// ============================================================
//  grantPartyExp – แจก EXP ให้ player + companion ทุกคนที่ยังไม่ตาย
//  เท่ากันทุกครั้ง ไม่ว่าใครเป็นคนฟันตาย (ข้อ 6: party-wide sharing)
//  เรียกแทนที่โค้ด "expGain > 0 && m_player->addExp(expGain)" เดิม
//  ที่เคยซ้ำกัน 3 จุด (AOE, melee/bow, companion)
// ============================================================
void Game::grantPartyExp(int expGain)
{
    if (expGain <= 0) return;

    if (m_player && m_player->addExp(expGain))
    {
        m_ui.levelUpFlash = true;
        m_ui.levelUpTimer = 120;
        int lv = m_player->getStats().level;
        m_coreSlots.setSlotCount(lv);
        refreshStats();
        recalcSpeed();
        addLog("  *** LEVEL UP! Level " + std::to_string(lv) + " ***", sf::Color(225,222,208));
    }

    for (auto& member : Party::instance().getMembers())
    {
        if (!member || member->isDead()) continue;
        int lvBefore = member->getLevel();
        member->addExp(expGain);
        if (member->getLevel() > lvBefore)
            addLog("  " + member->getName() + " leveled up to Lv." +
                   std::to_string(member->getLevel()) + "!", sf::Color(225,222,208));
    }
}

void Game::onEnemyKilled(Enemy* enemy)
{
    if (!enemy) return;
    if (enemy->getRank() == EnemyRank::Boss)
    {
        m_bossActive[enemy->getFamily()] = false;
        addLog("  Boss defeated! The " + enemy->getFamily() +
               " threat fades...", sf::Color(225,222,208));
        return;
    }
    const std::string& fam = enemy->getFamily();
    m_familyKillCount[fam]++;
    int count = m_familyKillCount[fam];

    if (count % 10 == 0 && count < BOSS_KILL_THRESHOLD)
    {
        addLog("  " + fam + " kills: " + std::to_string(count) +
               "/" + std::to_string(BOSS_KILL_THRESHOLD),
               sf::Color(225,222,208));
    }
    if (count >= BOSS_KILL_THRESHOLD && !m_bossActive[fam])
    {
        spawnBoss(fam);
    }
}

// ============================================================
//  makeDropItem / handleEnemyDeath – รวมศูนย์ logic "มอนตายแล้วเกิดอะไรขึ้น"
//  (drop item + แจก EXP + onEnemyKilled) ที่เคยก็อปแปะกระจาย 5 จุด
//  (playerAttack, fireBow, fireRangedAt, companionAttack, processTurn DOT)
//  โดยแต่ละจุดก็อปฟิลด์ของ Item ไม่ครบกันคนละแบบ ทำให้ของดรอปจากบาง
//  เส้นทาง (fireBow / companion / DOT kill) ไม่มี stat bonus / resist /
//  on-hit effect ใดๆ ทั้งที่ควรจะเหมือนกันหมด — แก้โดยรวมเป็นจุดเดียว
// ============================================================
Item Game::makeDropItem(const ItemData* idata, int col, int row) const
{
    Item drop;
    drop.id            = idata->id;
    drop.name          = idata->name;
    drop.type          = idata->type == "Core" ? ItemType::Core :
                          idata->type == "Ammo" ? ItemType::Ammo :
                                                  ItemType::Material;
    drop.desc          = idata->desc;
    drop.value         = idata->value;
    drop.hpBonus       = idata->hpBonus;
    drop.atkBonus      = idata->atkBonus;
    drop.defBonus      = idata->defBonus;
    drop.dodgeBonus    = idata->dodgeBonus;
    drop.manaBonus     = idata->manaBonus;
    drop.magicDmgBonus = idata->magicDmgBonus;
    drop.magicResBonus = idata->magicResBonus;
    drop.spdBonus      = idata->spdBonus;
    drop.bleedBonus    = idata->bleedBonus;
    drop.poisonBonus   = idata->poisonBonus;
    drop.burnBonus     = idata->burnBonus;
    drop.resistBleed   = idata->resistBleed;
    drop.resistBurn    = idata->resistBurn;
    drop.resistPoison  = idata->resistPoison;
    drop.resistSlow    = idata->resistSlow;
    drop.resistStun    = idata->resistStun;
    drop.bleedDmgReduce  = idata->bleedDmgReduce;
    drop.bleedDurReduce  = idata->bleedDurReduce;
    drop.poisonDmgReduce = idata->poisonDmgReduce;
    drop.poisonDurReduce = idata->poisonDurReduce;
    drop.burnDmgReduce   = idata->burnDmgReduce;
    drop.burnDurReduce   = idata->burnDurReduce;
    drop.stunDurReduce   = idata->stunDurReduce;
    drop.slowDurReduce   = idata->slowDurReduce;
    drop.onHitChance   = idata->onHitChance;
    drop.onHitDuration = idata->onHitDuration;
    drop.onHitPower    = idata->onHitPower;
    drop.onHitStatus   = idata->onHitStatus;
    drop.spriteName    = idata->sprite;
    drop.stackable     = idata->stackable;
    drop.col           = col;
    drop.row           = row;
    return drop;
}

//  handleEnemyDeath – ให้ loot + แจก EXP (ครั้งแรกที่ฆ่ามอน id นี้เท่านั้น)
//  + เรียก onEnemyKilled ครบทุกเส้นทางที่ฆ่ามอนได้
//  killerName ว่าง = ข้อความทั่วไป ("Goblin killed"), ไม่ว่าง = ใส่ชื่อผู้ฆ่า
//  (ใช้กับ companion เช่น "Aria killed Goblin!")
void Game::handleEnemyDeath(Enemy* enemy, const std::string& killerName)
{
    if (!enemy) return;

    // ── Loot drop (ทาง single source of truth: makeDropItem) ──
    auto dropped = DropTable::instance().roll(enemy->getId());
    for (const auto& itemId : dropped)
    {
        const ItemData* idata = DropTable::instance().getItem(itemId);
        if (!idata) continue;
        Item drop = makeDropItem(idata, enemy->getCol(), enemy->getRow());
        m_mapItems.push_back(drop);
        addLog("  Dropped: " + drop.name, sf::Color(225,222,208));
    }

    // ── EXP: ให้เฉพาะครั้งแรกที่ฆ่ามอน id นี้ (ทุกเส้นทางเหมือนกันหมด) ──
    const std::string& eid = enemy->getId();
    bool isFirstKill = (m_firstKillDone.find(eid) == m_firstKillDone.end());
    int expGain = isFirstKill ? enemy->getExp() : 0;

    if (isFirstKill)
    {
        m_firstKillDone.insert(eid);
        if (killerName.empty())
            addLog("  " + enemy->getName() + " killed +" + std::to_string(expGain) + " EXP",
                   sf::Color(225,222,208));
        else
            addLog("  " + killerName + " killed " + enemy->getName() + "! +" +
                   std::to_string(expGain) + " EXP", sf::Color(225,222,208));
    }
    else
    {
        if (killerName.empty())
            addLog("  " + enemy->getName() + " killed", sf::Color(225,222,208));
        else
            addLog("  " + killerName + " killed " + enemy->getName() + "!", sf::Color(225,222,208));
    }

    grantPartyExp(expGain);
    onEnemyKilled(enemy);
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
    std::uniform_int_distribution<int> typeDist(0, 10);
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

        if      (r < 3)  itemId = DropTable::instance().getRandomItemIdByType("Food");
        else if (r < 5)  itemId = DropTable::instance().getRandomItemIdByType("Potion");
        else if (r < 7)  itemId = DropTable::instance().getRandomItemIdByType("Weapon");
        else if (r < 8)  itemId = DropTable::instance().getRandomItemIdByType("OffWeapon");
        else if (r == 8) itemId = DropTable::instance().getRandomItemIdByType("Ammo"); // ← เพิ่ม
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
                            idata->type == "Ammo"    ? ItemType::Ammo      :  // ← เพิ่ม
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
                item.damageType    = damageTypeFromString(idata->damageType);
                item.slashDmgBonus  = idata->slashDmgBonus;
                item.pierceDmgBonus = idata->pierceDmgBonus;
                item.bluntDmgBonus  = idata->bluntDmgBonus;
                item.cleaveDmgBonus = idata->cleaveDmgBonus;
                item.bleedBonus  = idata->bleedBonus;
                item.poisonBonus = idata->poisonBonus;
                item.burnBonus   = idata->burnBonus;
                item.resistBleed = idata->resistBleed;
                item.resistBurn = idata->resistBurn;
                item.resistPoison = idata->resistPoison;
                item.resistSlow = idata->resistSlow;
                item.resistStun = idata->resistStun;
                item.bleedDmgReduce = idata->bleedDmgReduce;
                item.bleedDurReduce = idata->bleedDurReduce;
                item.poisonDmgReduce = idata->poisonDmgReduce;
                item.poisonDurReduce = idata->poisonDurReduce;
                item.burnDmgReduce = idata->burnDmgReduce;
                item.burnDurReduce = idata->burnDurReduce;
                item.stunDurReduce = idata->stunDurReduce;
                item.slowDurReduce = idata->slowDurReduce;
                item.onHitChance = idata->onHitChance;
                item.onHitDuration = idata->onHitDuration;
                item.onHitPower = idata->onHitPower;
                item.onHitStatus = idata->onHitStatus;
                item.spriteName    = idata->sprite;
                item.stackable     = idata->stackable;
                //ตรงนี้
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

void Game::spawnNPCs(int count)
{
    std::uniform_int_distribution<int> colDist(1, m_mapCols - 2);
    std::uniform_int_distribution<int> rowDist(1, m_mapRows - 2);

    auto& npcDB = NPCDB::instance();
    auto& allNPCs = npcDB.getAll();

    if (allNPCs.empty()) return;

    // เลือก NPC บางตัวไปวาง
    std::vector<std::string> npcIds;
    for (const auto& pair : allNPCs)
        npcIds.push_back(pair.first);

    int spawnCount = std::min(count, (int)npcIds.size());
    std::shuffle(npcIds.begin(), npcIds.end(), m_rng);

    for (int i = 0; i < spawnCount; ++i)
    {
        std::string id = npcIds[i];
        for (int att = 0; att < 50; ++att)
        {
            int col = colDist(m_rng);
            int row = rowDist(m_rng);
            if (!m_tileMap.isWalkable(col, row)) continue;

            // ห่างจาก player
            if (m_player)
            {
                int dx = col - m_player->getCol();
                int dy = row - m_player->getRow();
                if (dx*dx + dy*dy < 49) continue;
            }

            // ห่างจาก NPC อื่น
            bool blocked = false;
            for (const auto& npc : m_npcManager.getAll())
            {
                if (npc->getCol() == col && npc->getRow() == row)
                { blocked = true; break; }
            }
            if (blocked) continue;

            auto npc = npcDB.createNPC(id);
            if (!npc) break;

            npc->setPos(col, row);
            m_npcManager.add(npc);
            break;
        }
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

    // ✅ update hover ทุกเฟรมตาม mouse position จริง
    if (!m_playerDead && !m_inRaceSelect)
    {
        if (m_hoverLocked)
        {
            if (m_hoverLockClock.getElapsedTime().asSeconds() >= 0.5f)
                m_hoverLocked = false;
            else
            {
                m_hoverCol = m_lockedHoverCol;
                m_hoverRow = m_lockedHoverRow;
            }
        }

        if (!m_hoverLocked)
        {
            sf::Vector2i mousePixel = sf::Mouse::getPosition(m_window);
            sf::Vector2f uiPos  = m_window.mapPixelToCoords(mousePixel, m_uiView);
            sf::Vector2f world  = m_window.mapPixelToCoords(mousePixel, m_gameView);
            if (uiPos.x < GAME_VIEW_W && uiPos.y < GAME_VIEW_H)
            {
                m_hoverCol = std::clamp((int)(world.x / TILE_SIZE), 0, m_mapCols - 1);
                m_hoverRow = std::clamp((int)(world.y / TILE_SIZE), 0, m_mapRows - 1);
            }
            else { m_hoverCol = -1; m_hoverRow = -1; }
        }
    }
    if (m_inRaceSelect)
    {
        sf::Vector2i mousePixel = sf::Mouse::getPosition(m_window);
        sf::Vector2f uiPos = m_window.mapPixelToCoords(mousePixel, m_uiView);

        const float CARD_W = 180.f;
        const float CARD_H = 205.f;
        const float GAP    = 12.f;
        const int   COLS   = 3;
        const int   TOTAL  = 6;

        float gridW = COLS * CARD_W + (COLS - 1) * GAP;
        float startX = (float)WINDOW_W/2.f - gridW/2.f;
        float startY = 80.f;

        for (int i = 0; i < TOTAL; ++i)
        {
            int col = i % COLS;
            int row = i / COLS;
            float cx = startX + col * (CARD_W + GAP);
            float cy = startY + row * (CARD_H + GAP);
            if (uiPos.x >= cx && uiPos.x <= cx + CARD_W &&
                uiPos.y >= cy && uiPos.y <= cy + CARD_H)
            { m_ui.selectedRace = i; break; }  // ✅ sync ไปที่ keyboard selection เลย
        }
        return;
    }
    // travel — เดินทีละ step ต่อเฟรมที่ player พร้อม
    if (!m_travelPath.empty() && m_player && !m_playerDead)
    {
        // หยุดถ้าเจอศัตรูระหว่าง travel
        for (auto* e : m_enemies)
            if (!e->isDead() && m_fog.isVisible(e->getCol(), e->getRow()))
            { m_travelPath.clear(); break; }

        if (!m_travelPath.empty() && m_travelStep < (int)m_travelPath.size())
        {
            int pc = m_player->getCol();
            int pr = m_player->getRow();
            int dc = m_travelPath[m_travelStep].first  - pc;
            int dr = m_travelPath[m_travelStep].second - pr;
            handlePlayerMove(dc, dr);
            m_travelStep++;
            if (m_travelStep >= (int)m_travelPath.size())
            {
                m_lockedHoverCol = m_player->getCol();
                m_lockedHoverRow = m_player->getRow();
                m_hoverLocked = true;
                m_hoverLockClock.restart();
                m_travelPath.clear();
            }
        }
    }
    
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
            if (key->code == sf::Keyboard::Key::H && sel % 3 > 0)       sel--;
            if (key->code == sf::Keyboard::Key::L && sel % 3 < 2 && sel+1 < total) sel++;
            if (key->code == sf::Keyboard::Key::K && sel - 3 >= 0)       sel -= 3;
            if (key->code == sf::Keyboard::Key::J && sel + 3 < total)    sel += 3;
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
            if (key->code == sf::Keyboard::Key::Escape)
            m_window.close();
                return;
        }
        if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (click->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f uiPos = m_window.mapPixelToCoords(
                    {click->position.x, click->position.y}, m_uiView);

                const float CARD_W = 180.f;
                const float CARD_H = 205.f;
                const float GAP    = 12.f;
                const int   COLS   = 3;
                const int   TOTAL  = 6;

                float gridW = COLS * CARD_W + (COLS - 1) * GAP;
                float startX = (float)WINDOW_W/2.f - gridW/2.f;
                float startY = 80.f;

                for (int i = 0; i < TOTAL; ++i)
                {
                    int col = i % COLS;
                    int row = i / COLS;
                    float cx = startX + col * (CARD_W + GAP);
                    float cy = startY + row * (CARD_H + GAP);

                    if (uiPos.x >= cx && uiPos.x <= cx + CARD_W &&
                        uiPos.y >= cy && uiPos.y <= cy + CARD_H)
                    {
                        auto& races = RaceDB::instance().getAll();
                        if (i < (int)races.size())
                        {
                            m_selectedRace = races[i].id;
                            m_inRaceSelect = false;
                            newDungeon(false);
                        }
                        break;
                    }
                }
            }
        }
        return;
    }
        if (const auto* key = event->getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Tab)
            m_partyUI.toggle();   // เปลี่ยนชื่อ instance ตามที่ประกาศไว้จริงใน Game.hpp
            if (m_playerDead)
            {
                if (key->code==sf::Keyboard::Key::R)
                m_inRaceSelect = true;
                m_ui.selectedRace = 0;
                if (key->code==sf::Keyboard::Key::Q) m_window.close();
                break;
            }

            // ── ข้อ 5: PartyUI เปิดอยู่ → สกัด input ไปเป็นคำสั่งปาร์ตี้ ไม่ให้ตกไปขยับตัวละคร ──
            if (m_partyUI.isVisible() && !m_ui.targeting.active)
            {
                auto& party = Party::instance();
                bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                             sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

                switch (key->code)
                {
                    case sf::Keyboard::Key::J:
                        if (shift) reorderSelectedCompanion(+1);
                        else       m_partyUI.moveSelection(+1, party.size());
                        return;
                    case sf::Keyboard::Key::K:
                        if (shift) reorderSelectedCompanion(-1);
                        else       m_partyUI.moveSelection(-1, party.size());
                        return;
                    case sf::Keyboard::Key::Enter:
                        m_partyUI.toggleDetails();
                        return;
                    case sf::Keyboard::Key::X:
                        dismissSelectedCompanion();
                        return;
                    case sf::Keyboard::Key::Escape:
                        m_partyUI.setVisible(false);
                        return;
                    // กัน movement key อื่นๆ ไม่ให้หลุดไปขยับตัวละครขณะเลือกเมนูปาร์ตี้
                    case sf::Keyboard::Key::H:
                    case sf::Keyboard::Key::L:
                    case sf::Keyboard::Key::Y:
                    case sf::Keyboard::Key::U:
                    case sf::Keyboard::Key::B:
                    case sf::Keyboard::Key::N:
                        return;
                    default: break;  // ปล่อย Tab/F11/ฯลฯ ให้ทำงานตามปกติต่อไปข้างล่าง
                }
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
                    case sf::Keyboard::Key::F:   
                    confirmTarget(); break;
                    case sf::Keyboard::Key::Escape: cancelTargeting(); break;
                    default: break;
                }
                return;
            }

            // ── DCSS-style skill select (Shift+Q): เปิดอยู่ → ดัก input ทั้งหมดไปเลือกสกิลด้วย a-z/A-Z ──
            if (m_ui.skillSelectOpen)
            {
                if (key->code == sf::Keyboard::Key::Escape)
                {
                    m_ui.skillSelectOpen = false;
                    return;
                }
                int idx = letterKeyIndex(key->code);
                if (idx >= 0)
                {
                    bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                 sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                    char letter = char((shift ? 'A' : 'a') + idx);
                    selectSkillByLetter(letter);
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
                    // ใน switch(key->code) หลัก เพิ่ม
                case sf::Keyboard::Key::PageUp:
                    if (m_ui.activePanel == UIState::Panel::Stats)
                        m_statsScrollOffset = std::max(0, m_statsScrollOffset - 1);
                    break;
                case sf::Keyboard::Key::PageDown:
                    if (m_ui.activePanel == UIState::Panel::Stats)
                        m_statsScrollOffset++;
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
                case sf::Keyboard::Key::Space:  tryInteractNPC();   break;  // Try to recruit NPC, otherwise wait
                case sf::Keyboard::Key::F:
                {
                    bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                 sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                    if (shift)
                        toggleFMode();                              // Shift+F: สลับโหมดยิงธนู/ใช้สกิล
                    else if (m_ui.fMode == UIState::FMode::Bow)
                        enterBowTargeting();                        // F: ยิงธนู (โหมด Bow)
                    else
                        executeSelectedSkill();                     // F: ใช้สกิลที่เลือกไว้ (โหมด Skill)
                    break;
                }
                case sf::Keyboard::Key::Q:
                {
                    bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                                 sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                    if (shift) m_ui.skillSelectOpen = !m_ui.skillSelectOpen;  // Shift+Q: เปิด/ปิดหน้าเลือกสกิล
                    break;
                }
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
        if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (click->button == sf::Mouse::Button::Left && !m_playerDead && !m_inRaceSelect)
            {
                sf::Vector2f uiPos = m_window.mapPixelToCoords(
                    {click->position.x, click->position.y}, m_uiView);
                sf::Vector2f world = m_window.mapPixelToCoords(
                    {click->position.x, click->position.y}, m_gameView);
                if (uiPos.x < GAME_VIEW_W && uiPos.y < GAME_VIEW_H)
                {
                    int col = std::clamp((int)(world.x / TILE_SIZE), 0, m_mapCols - 1);
                    int row = std::clamp((int)(world.y / TILE_SIZE), 0, m_mapRows - 1);
                    //m_hoverCol = col;  // ✅ update hover ด้วย
                    //m_hoverRow = row;
                    handleMouseMove(col, row);
                }
            }
        }
        if (const auto* moved = event->getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f uiPos = m_window.mapPixelToCoords(
                {moved->position.x, moved->position.y}, m_uiView);
            sf::Vector2f world = m_window.mapPixelToCoords(
                {moved->position.x, moved->position.y}, m_gameView);

            if (uiPos.x < GAME_VIEW_W && uiPos.y < GAME_VIEW_H)
            {
                //m_hoverCol = std::clamp((int)(world.x / TILE_SIZE), 0, m_mapCols - 1);
                //m_hoverRow = std::clamp((int)(world.y / TILE_SIZE), 0, m_mapRows - 1);
            }
            else
            {
                //m_hoverCol = -1;
                //m_hoverRow = -1;
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
    std::string gateId = m_tileMap.getGateId(m_player->getCol(), m_player->getRow());  // ← เพิ่ม
    m_floorItemsSaved[m_dungeonFloor] = m_mapItems;
    m_floorEnemiesSaved[m_dungeonFloor] = m_enemies; // เซฟ enemies ที่เหลือรอด (ย้าย ownership ไม่ delete)
    m_enemies.clear();
    m_floorNPCsSaved[m_dungeonFloor] = m_npcManager.getAll();  // เซฟ NPC ที่ยังไม่ recruit
    m_dungeonFloor++;
    Stats saved=m_player->getStats();
    auto savedSkills=m_player->getSkills();
    auto savedLetters = m_player->getSkillLetters();
    auto savedSelected = m_player->getSelectedSkillId();
    newDungeon(true);
    if (m_player)
    {
        int foundCol=-1, foundRow=-1, fallbackCol=-1, fallbackRow=-1;
        for (int row=1;row<m_mapRows-1;++row)
            for (int col=1;col<m_mapCols-1;++col)
                if (m_tileMap.getTile(col,row)==TileType::GateUp)
                {
                    if (fallbackCol==-1) { fallbackCol=col; fallbackRow=row; }
                    if (!gateId.empty() && m_tileMap.getGateId(col,row)==gateId)
                        { foundCol=col; foundRow=row; }
                }
        if (foundCol==-1) { foundCol=fallbackCol; foundRow=fallbackRow; }
        if (foundCol!=-1) m_player->setPos(foundCol,foundRow);
        m_player->getStats()=saved;
        m_player->getSkills()=savedSkills;
        m_player->setSkillLetters(savedLetters);
        m_player->setSelectedSkillId(savedSelected);
        updateCamera();
        recomputeFog();
    }
    addLog("  Entered floor "+std::to_string(m_dungeonFloor)+" through the gate!",sf::Color(225,222,208));
}

void Game::tryAscendStairs()
{
    if (!m_player) return;
    if (m_tileMap.getTile(m_player->getCol(),m_player->getRow())!=TileType::GateUp)
    {addLog("  No gate here.");return;}
    if (m_dungeonFloor<=1){addLog("  Already on floor 1.");return;}
    m_floorItemsSaved[m_dungeonFloor] = m_mapItems;  // เซฟของที่วางไว้บนชั้นนี้ก่อนย้าย
    m_floorEnemiesSaved[m_dungeonFloor] = m_enemies; // เซฟ enemies ที่เหลือรอด (ย้าย ownership ไม่ delete)
    m_enemies.clear();
    m_floorNPCsSaved[m_dungeonFloor] = m_npcManager.getAll();  // เซฟ NPC ที่ยังไม่ recruit
    m_dungeonFloor--;
    Stats saved=m_player->getStats();
    auto savedSkills=m_player->getSkills();
    auto savedLetters = m_player->getSkillLetters();
    auto savedSelected = m_player->getSelectedSkillId();
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
        m_player->setSkillLetters(savedLetters);
        m_player->setSelectedSkillId(savedSelected);
        updateCamera();
        recomputeFog();
    }
    addLog("  Returned to floor "+std::to_string(m_dungeonFloor)+" through the gate!",sf::Color(225,222,208));
}

void Game::waitTurn()
{
    if (!m_player) return;

    for (auto* e : m_enemies)
    {
        if (m_fog.isVisible(e->getCol(), e->getRow()))
        {
            addLog("  Can't wait - enemy in sight!", sf::Color(225,222,208));
            return;
        }
    }
    // เติม spdCounter เต็ม (100) เหมือนขยับ 1 เทิร์น
    Stats& ps = m_player->getStats();
    m_globalTime = ps.nextActTime;
    ps.nextActTime += ps.moveAut;
    

    m_turnCount++; m_player->onTurnPassed();
    processTurn(); tryRespawnEnemies();
    recomputeFog();
    addLog("  You wait.", sf::Color(225,222,208));
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
            Item item=m_mapItems[i]; item.col=-1; item.row=-1;

            if (!m_inventory.addItem(item)) {
                addLog("  Inventory full!",sf::Color(225,222,208));
                return;
            }
            m_mapItems.erase(m_mapItems.begin()+i);
            addLog("  Picked up: "+item.name);
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
         addLog("  Ate "+item->name+". Hunger +"+std::to_string(item->value),sf::Color(225,222,208));}
        else
        {s.hp=std::min(s.maxHp,s.hp+item->value);
         addLog("  Used "+item->name+". HP +"+std::to_string(item->value),sf::Color(225,222,208));}
        m_inventory.removeItem(m_ui.selectedInvSlot);
    }
    else if (item->type == ItemType::Ammo)
        addLog("  " + item->name + " is ammunition. Cannot equip.", sf::Color(225,222,208));
    else if (item->isCore()) equipCore();
    else if (item->isMaterial()) addLog("  "+item->name+" is a trade item.",sf::Color(225,222,208));
    else if (item->isEquipment())
    {
        EquipSlot slot=Equipment::itemToSlot(*item);
        if (m_equipment.hasItem(slot))
        {
            Item old=m_equipment.unequip(slot);
            if (!m_inventory.isFull()) m_inventory.addItem(old);
            else{addLog("  Inventory full!",sf::Color(225,222,208));return;}
        }
        Item toEquip=*item;
        m_inventory.removeItem(m_ui.selectedInvSlot);
        m_equipment.equip(toEquip,slot);
        addLog("  Equipped "+toEquip.name+" ["+Equipment::slotName(slot)+"] +"+
               std::to_string(toEquip.value),sf::Color(225,222,208));
        
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
    addLog("  Unequipped "+item.name, sf::Color(225,222,208));
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
    if (freeSlot==-1){addLog("  All core slots full!",sf::Color(225,222,208));return;}
    Item core=*item;
    m_inventory.removeItem(m_ui.selectedInvSlot);
    m_coreSlots.equip(freeSlot,core);
    addLog("  Equipped "+core.name+" in core slot "+std::to_string(freeSlot+1),sf::Color(225,222,208));

    const ItemData* idata = DropTable::instance().getItem(core.id);
    if (idata)
        for (const auto& skillId : idata->coreSkills)
        {
            if (m_player->addCoreSkill(skillId))
                addLog("  Skill unlocked: "+skillId, sf::Color(225,222,208));
            else
                addLog("  Unknown skill id: "+skillId, sf::Color(225,222,208));
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
    addLog("  Removed "+core.name+" from core slot "+std::to_string(m_ui.selectedCoreSlot+1),sf::Color(225,222,208));

    const ItemData* idata = DropTable::instance().getItem(core.id);
    if (idata)
        for (const auto& skillId : idata->coreSkills)
        {
            m_player->removeCoreSkill(skillId);
            addLog("  Skill lost: "+skillId, sf::Color(225,222,208));
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
    addLog("  Dropped: "+d.name,sf::Color(225,222,208));
    m_inventory.removeItem(m_ui.selectedInvSlot);
}

// ============================================================
//  Movement & Combat
// ============================================================
void Game::handleMouseMove(int targetCol, int targetRow)
{
    if (!m_player) return;
    if (!m_tileMap.isWalkable(targetCol, targetRow)) return;

    int pc = m_player->getCol();
    int pr = m_player->getRow();

    // เช็คศัตรูในสายตา
    bool enemyVisible = false;
    for (auto* e : m_enemies)
        if (!e->isDead() && m_fog.isVisible(e->getCol(), e->getRow()))
        { enemyVisible = true; break; }

    auto path = findPath(pc, pr, targetCol, targetRow);
    if (path.empty()) return;

    if (enemyVisible)
    {
        // single step — เดินแค่ก้าวเดียว
        int dc = path[0].first  - pc;
        int dr = path[0].second - pr;
        handlePlayerMove(dc, dr);
        m_travelPath.clear();
    }
    else
    {
        // travel — เดินต่อเนื่อง
        m_travelPath = path;
        m_travelStep = 0;
    }
}

void Game::handlePlayerMove(int dc, int dr)
{
    if (!m_player) return;

    Stats& ms = m_player->getStats();
    

    int tc=m_player->getCol()+dc, tr=m_player->getRow()+dr;

    for (auto*e:m_enemies)
        if (!e->isDead()&&e->getCol()==tc&&e->getRow()==tr)
        {
            playerAttack(e);
            
            m_globalTime = ms.nextActTime;
            ms.nextActTime += ms.atkAut;
            processTurn(); tryRespawnEnemies(); recalcSpeed();
            auto t1 = std::chrono::high_resolution_clock::now();
            recomputeFog();
            auto t2 = std::chrono::high_resolution_clock::now();
            auto ms2 = std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count();
            //addLog("fog: " + std::to_string(ms2) + "ms", sf::Color(225,222,208));
            updateCamera(); return;
            
        }
    if (dc < 0) m_player->setFacing(false);
    else if (dc > 0) m_player->setFacing(true);
    bool moved=m_player->tryMove(dc,dr,m_tileMap);
    if (!moved) return;
        // บันทึกตำแหน่งให้ trail ของ party ทุกครั้งที่ player ขยับ
    Party::instance().recordLeaderStep(m_player->getCol(), m_player->getRow(), m_player->getFacingLeft());
    
    m_globalTime = ms.nextActTime;
    ms.nextActTime += ms.moveAut;

    m_turnCount++; m_player->onTurnPassed();
    recomputeFog();

    processTurn(); tryRespawnEnemies(); recalcSpeed();
    updateCamera();

    int pc=m_player->getCol(),pr=m_player->getRow();
    for (auto& it:m_mapItems)
        if (it.col==pc&&it.row==pr)
        {addLog("  "+it.name+" here. [G] pick up.",sf::Color(225,222,208));break;}

    TileType tile=m_tileMap.getTile(pc,pr);
    if (tile==TileType::GateDown) addLog("  Gate Down",sf::Color(225,222,208));
    if (tile==TileType::GateUp)   addLog("  Gate Up",  sf::Color(225,222,208));

    int hunger=m_player->getStats().hunger;
    if(hunger==50) addLog("  You feel hungry.",      sf::Color(225,222,208));
    if(hunger==20) addLog("  Very hungry!",          sf::Color(225,222,208));
    if(hunger== 0) addLog("  Starving! HP draining!",sf::Color(225,222,208));
}

void Game::playerAttack(Enemy* enemy)
{
    if (!m_player) return;
    int atk=getBuffedAtk();
    int dmg=std::max(1,atk-enemy->getDefense());

    // ── ดาเมจตามประเภทอาวุธ (ฟัน/แทง/ทุบ/ฟาด) vs resist ของมอน ──
    const Item* mainHand = m_equipment.getItem(EquipSlot::MainHand);
    DamageType  weaponDmgType = mainHand ? mainHand->damageType : DamageType::Slash;

    int dmgTypeBonus = 0;
    switch (weaponDmgType)
    {
        case DamageType::Slash:  dmgTypeBonus = m_finalStats.slashDmgBonus;  break;
        case DamageType::Pierce: dmgTypeBonus = m_finalStats.pierceDmgBonus; break;
        case DamageType::Blunt:  dmgTypeBonus = m_finalStats.bluntDmgBonus;  break;
        case DamageType::Cleave: dmgTypeBonus = m_finalStats.cleaveDmgBonus; break;
    }
    dmg += dmgTypeBonus;

    int resist = std::clamp(enemy->getResist(weaponDmgType), -100, 100);
    dmg = std::max(1, dmg * (100 - resist) / 100);

    enemy->takeDamage(dmg);

    bool powered = false;
    for (const auto& sk : m_player->getSkills())
        if (sk.buffActive && sk.data.type == SkillType::ActiveBuff && sk.data.effect.atkPct > 0)
            powered = true;

    std::string msg="  You hit "+enemy->getName()+" for "+std::to_string(dmg)+"!";
    if (powered) msg+=" [POWERED]";
    addLog(msg,sf::Color(225,222,208));

    // ── On-hit status effect จาก weapon ──
if (mainHand && !mainHand->onHitStatus.empty() &&
    mainHand->onHitDuration > 0 && mainHand->onHitChance > 0)
{
    std::uniform_int_distribution<int> rollDist(0, 99);
    if (rollDist(m_rng) < mainHand->onHitChance)
    {
        const std::string& st = mainHand->onHitStatus;
        StatusType stype;
        bool valid = true;
        if      (st == "bleed")  stype = StatusType::Bleed;
        else if (st == "poison") stype = StatusType::Poison;
        else if (st == "burn")   stype = StatusType::Burn;
        else if (st == "stun")   stype = StatusType::Stun;
        else if (st == "slow")   stype = StatusType::Slow;
        else valid = false;

        if (valid)
        {
            int bonus = 0;
            if (st == "bleed")  bonus = m_finalStats.bleedBonus;
            if (st == "poison") bonus = m_finalStats.poisonBonus;
            if (st == "burn")   bonus = m_finalStats.burnBonus;
            StatusEffect se;
            se.type     = stype;
            se.power    = mainHand->onHitPower + bonus;
            se.duration = mainHand->onHitDuration;
            se.sourceId = mainHand->id;
            enemy->applyStatus(se);
            addLog("  " + enemy->getName() + " is " + st + "!", sf::Color(225,222,208));
            
        }
    }
}

    if (enemy->isDead())
        handleEnemyDeath(enemy);
}

void Game::enemyAttack(Enemy* enemy)
{
    if (!m_player) return;
    Stats& s=m_player->getStats();
    int def=getBuffedDef();
    int dodge=s.maxDodge+m_equipment.getTotalDodgeBonus();
    float dodgeChance=std::min(0.45f,0.06f+dodge*0.05f);
    if (std::uniform_int_distribution<int>(0,99)(m_rng)<(int)(dodgeChance*100))
    {addLog("  You dodged "+enemy->getName()+"!",sf::Color(225,222,208));return;}
    int dmg=std::max(1,enemy->getAttack()-def);
    s.hp -= dmg;

// ── Apply status on hit ──
const MonsterData* mdata = MonsterDB::instance().get(enemy->getId());
if (mdata && !mdata->applyStatus.empty() && mdata->statusDuration > 0)
{
    StatusType stype;
    bool valid = true;
    const std::string& st = mdata->applyStatus;

    if      (st == "bleed")  stype = StatusType::Bleed;
    else if (st == "poison") stype = StatusType::Poison;
    else if (st == "burn")   stype = StatusType::Burn;
    else if (st == "stun")   stype = StatusType::Stun;
    else if (st == "slow")   stype = StatusType::Slow;
    else valid = false;

    StatusEffect se;
    se.type     = stype;
    se.power    = mdata->statusPower;
    se.duration = mdata->statusDuration;
    se.sourceId = enemy->getId();

    if (valid)
    {
        int resist = 0;
        if (stype == StatusType::Bleed)  resist = m_finalStats.resistBleed;
        if (stype == StatusType::Poison) resist = m_finalStats.resistPoison;
        if (stype == StatusType::Burn)   resist = m_finalStats.resistBurn;
        if (stype == StatusType::Stun)   resist = m_finalStats.resistStun;
        if (stype == StatusType::Slow)   resist = m_finalStats.resistSlow;

        std::uniform_int_distribution<int> rollDist(0, 99);
        int roll = rollDist(m_rng);
        if (roll < resist)
        {
            addLog("  You resist " + st + "!", sf::Color(225,222,208));
        }
        else
        {
            // declare ก่อน for loop
            int dmgReduce = 0;
            if (stype == StatusType::Bleed)  dmgReduce = m_finalStats.bleedDmgReduce;
            if (stype == StatusType::Poison) dmgReduce = m_finalStats.poisonDmgReduce;
            if (stype == StatusType::Burn)   dmgReduce = m_finalStats.burnDmgReduce;

            int durReduce = 0;
            if (stype == StatusType::Bleed)  durReduce = m_finalStats.bleedDurReduce;
            if (stype == StatusType::Poison) durReduce = m_finalStats.poisonDurReduce;
            if (stype == StatusType::Burn)   durReduce = m_finalStats.burnDurReduce;
            if (stype == StatusType::Stun)   durReduce = m_finalStats.stunDurReduce;
            if (stype == StatusType::Slow)   durReduce = m_finalStats.slowDurReduce;

            bool found = false;
            for (auto& existing : s.statusEffects)
            {
                if (existing.type == stype)
                {
                    existing.duration = std::max(1, se.duration * (100 - durReduce) / 100);
                    existing.power    = std::max(1, se.power    * (100 - dmgReduce) / 100);
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                int dmgReduce = 0;
                if (stype == StatusType::Bleed)  dmgReduce = m_finalStats.bleedDmgReduce;
                if (stype == StatusType::Poison) dmgReduce = m_finalStats.poisonDmgReduce;
                if (stype == StatusType::Burn)   dmgReduce = m_finalStats.burnDmgReduce;

                int durReduce = 0;
                if (stype == StatusType::Bleed)  durReduce = m_finalStats.bleedDurReduce;
                if (stype == StatusType::Poison) durReduce = m_finalStats.poisonDurReduce;
                if (stype == StatusType::Burn)   durReduce = m_finalStats.burnDurReduce;
                if (stype == StatusType::Stun)   durReduce = m_finalStats.stunDurReduce;
                if (stype == StatusType::Slow)   durReduce = m_finalStats.slowDurReduce;

                se.power    = std::max(1, se.power    * (100 - dmgReduce) / 100);
                se.duration = std::max(1, se.duration * (100 - durReduce) / 100);

                s.statusEffects.push_back(se);
                addLog("  You are " + st + "ed!", sf::Color(225,222,208));
            }
        }
    }
    
}
addLog("  "+enemy->getName()+" hits you for "+std::to_string(dmg)+"!",sf::Color(225,222,208));
    if (s.hp<=0)
    {
        s.hp=0;
        m_playerDead=true;
        addLog("  *** YOU DIED *** [R] restart",sf::Color(225,222,208));
    }
}
// ============================================================
//  enemyAttackCompanion – มอนฟัน companion (formula เรียบง่าย
//  ไม่มี dodge/status เหมือนฝั่ง player เพราะ NPC ไม่มีระบบนั้น)
// ============================================================
void Game::enemyAttackCompanion(Enemy* enemy, std::shared_ptr<NPC> npc)
{
    if (!enemy || !npc) return;

    int dmg = std::max(1, enemy->getAttack() - npc->getDefense());
    npc->takeDamage(dmg);

    addLog("  " + enemy->getName() + " hits " + npc->getName() +
           " for " + std::to_string(dmg) + "!", sf::Color(225,222,208));

    if (npc->isDead())
    {
        // ── ข้อ 7: downed ไม่ใช่ตายถาวร ──
        // ไม่ removeMember แล้ว: companion ที่ hp<=0 ยัง "อยู่" ใน party
        // (isDead() ทำให้ combat loop / movement loop ข้ามมันไปเองอยู่แล้ว
        // ที่ทุกจุดมี guard "if (!npc || npc->isDead()) continue;")
        // ตำแหน่งยังโดนลาก follow trail ต่อไปเหมือนถูกหามเดิน
        // ปลุกคืนได้ด้วยการเดินไปยืนติดแล้วกด interact → tryInteractNPC()
        addLog("  " + npc->getName() + " is downed!", sf::Color(225,222,208));
    }
}
// ============================================================
//  companionAttack – companion ฟันมอนกลับ (passive combat, ข้อ 2)
//  ไม่มี weapon damage-type / on-hit status เหมือน player เพราะ
//  NPC ยังไม่มีระบบ equipment (ยกไปทำทีหลังถ้าต้องการ)
//  เรียกจาก processTurn() เท่านั้น → enemy ตายและถูกลบใน
//  processTurn() รอบเดียวกัน (remove_if ท้าย loop) จึงไม่ซ้ำ
//  กับปัญหา double-onEnemyKilled ที่ playerAttack/fireBow เจอ
// ============================================================
void Game::companionAttack(std::shared_ptr<NPC> npc, Enemy* enemy)
{
    if (!npc || !enemy || npc->isDead() || enemy->isDead()) return;

    int dmg = std::max(1, npc->getAttack() - enemy->getDefense());
    enemy->takeDamage(dmg);

    addLog("  " + npc->getName() + " hits " + enemy->getName() +
           " for " + std::to_string(dmg) + "!", sf::Color(225,222,208));

    if (!enemy->isDead()) return;

    // ── EXP + Loot + onEnemyKilled (path เดียวกับ playerAttack/fireBow ทุกจุด) ──
    handleEnemyDeath(enemy, npc->getName());
}
// ============================================================
//  tryMoveCompanion – เดินทีละก้าว (ข้อ 4: active AI ไล่มอน)
//  greedy step ธรรมดา ไม่ใช้ A* เต็มรูปแบบเหมือน Enemy::astar
//  (เอาไว้ทีหลังถ้าทางตันบ่อยเกินไปค่อยอัพเกรด)
//  เช็คชน: wall, player, enemy, companion อื่นในปาร์ตี้
// ============================================================
bool Game::tryMoveCompanion(std::shared_ptr<NPC> npc, int dc, int dr)
{
    if (!npc || (dc == 0 && dr == 0)) return false;

    int nc = npc->getCol() + dc;
    int nr = npc->getRow() + dr;

    if (!m_tileMap.isWalkable(nc, nr)) return false;
    if (m_player && m_player->getCol() == nc && m_player->getRow() == nr) return false;

    for (auto* e : m_enemies)
        if (!e->isDead() && e->getCol() == nc && e->getRow() == nr) return false;

    auto& party = Party::instance();
    for (int i = 0; i < party.size(); i++)
    {
        auto other = party.getMember(i);
        if (other && other != npc && other->getCol() == nc && other->getRow() == nr)
            return false;
    }

    npc->setPos(nc, nr);
    if (dc != 0) npc->setFacing(dc < 0);
    return true;
}
// ============================================================
//  processTurn – จัดการเทิร์นของศัตรู
// ============================================================
void Game::processTurn()
{
    if (!m_player) return;

    regenStamina();

    // ── Player DOT tick ──
    {
        int hpDelta = 0;
        std::string effectName;
        m_player->tickStatusEffects(hpDelta, effectName);
        if (hpDelta < 0)
        {
            addLog("  You take " + std::to_string(-hpDelta) +
                   " " + effectName + " dmg!", sf::Color(225,222,208));
            Stats& ps = m_player->getStats();
            if (ps.hp <= 0)
            {
                ps.hp = 0;
                m_playerDead = true;
                addLog("  *** YOU DIED *** [R] restart", sf::Color(225,222,208));
            }
        }
    }

    long long playerTime = m_globalTime;
    int pc = m_player->getCol();
    int pr = m_player->getRow();

    // ── เพิ่มบรรทัดนี้: ตำแหน่ง companion ทั้งหมด ใช้เป็น blocked tile ให้ enemy เดินอ้อม ──
    std::vector<std::pair<int,int>> partyBlockedTiles;
    {
        auto& party = Party::instance();
        for (int pi = 0; pi < party.size(); pi++)
        {
            auto member = party.getMember(pi);
            if (member) partyBlockedTiles.emplace_back(member->getCol(), member->getRow());
        }
    }

    for (auto* e : m_enemies)
    {
        if (e->isDead()) continue;
        processEnemyTurn(e, playerTime, pc, pr, partyBlockedTiles);
    }

    processCompanionActions(playerTime);

    m_enemies.erase(
        std::remove_if(m_enemies.begin(), m_enemies.end(),
            [](Enemy* e) { bool d = e->isDead(); if (d) delete e; return d; }),
        m_enemies.end());

    recalcAllStats();
}

// ============================================================
//  processEnemyTurn – ตัดสินใจ+ลงมือของมอนตัวเดียวใน 1 เทิร์น
//  (ย้ายออกมาจาก processTurn เดิมที่เป็น loop ยาวเดียว ~330 บรรทัด
//  แยกให้ processTurn เหลือแค่ orchestration)
// ============================================================
void Game::processEnemyTurn(Enemy* e, long long playerTime, int pc, int pr,
                             const std::vector<std::pair<int,int>>& partyBlockedTiles)
{
    // ── ยังไม่ถึงเวลา → รอ ไม่ต้อง tick/advance ──
    // (ย้ายมาเช็คก่อน tickStatusEffects เพื่อให้ duration ของ status
    //  (Slow/Bleed/Poison ฯลฯ) นับตาม "เทิร์นของมอนเอง" ไม่ใช่นับทุกเทิร์นผู้เล่น
    //  เดิม tick ทุกเทิร์นผู้เล่นทำให้ status หมดอายุก่อนมอนจะได้ขยับจริงๆ)
    if (e->getNextActTime() > playerTime)
        return;

    // ── Tick status effects ──
    {
        int hpDelta = 0;
        std::string effectName;
        e->tickStatusEffects(hpDelta, effectName);
        if (hpDelta < 0)
            addLog("  " + e->getName() + " takes " + std::to_string(-hpDelta) +
                   " " + effectName + " dmg", sf::Color(225,222,208));
    }

    if (e->isDead())
    {
        // เดิมจุดนี้ไม่แจก EXP เลย (มอนตายเพราะ bleed/poison/burn ระหว่างเทิร์น
        // ไม่เคยเรียก grantPartyExp) และของดรอปก็ไม่มี stat bonus ใดๆ —
        // handleEnemyDeath() แก้ทั้งสองปัญหาให้ตรงกับทุกเส้นทางอื่น
        handleEnemyDeath(e);
        return;
    }

    // ── Stun → เสียเวลาแต่ไม่ขยับ ──
    if (e->hasStatus(StatusType::Stun))
    {
        e->advanceActTime();
        e->tickSkills();
        return;
    }

    // ── ไม่เห็นและไม่ alerted → advance แล้วข้าม ──
    if (!m_fog.isVisible(e->getCol(), e->getRow()) && !e->isAlerted())
    {
        e->advanceActTime();
        return;
    }

    // ── หาว่ามี companion ยืนติดตัวมอนไหม ก่อนเช็คระยะ player ──
    std::shared_ptr<NPC> adjacentCompanion = nullptr;
    {
        auto& party = Party::instance();
        for (int pi = 0; pi < party.size(); pi++)
        {
            auto member = party.getMember(pi);
            if (!member || member->isDead()) continue;
            int mdx = std::abs(e->getCol() - member->getCol());
            int mdy = std::abs(e->getRow() - member->getRow());
            if (mdx <= 1 && mdy <= 1) { adjacentCompanion = member; break; }
        }
    }

    int dx = std::abs(e->getCol() - pc);
    int dy = std::abs(e->getRow() - pr);

    if (adjacentCompanion)
    {
        enemyAttackCompanion(e, adjacentCompanion);
    }
    else if (dx <= 1 && dy <= 1 && (dx + dy) > 0)
    {
        enemyAttack(e);
    }
    else
    {
        e->updateAI(pc, pr, m_tileMap, m_enemies, partyBlockedTiles);

        if (e->hasPendingSkill())
        {
            auto ps = e->consumePendingSkill();
            SkillInstance* sk = e->findSkill(ps.skillId);
            if (!sk) { e->advanceActTime(); e->tickSkills(); return; }

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
                               " " + std::to_string(actualDmg) + " dmg",
                               sf::Color(225,222,208));
                        if (s.hp <= 0) { s.hp = 0; m_playerDead = true;
                            addLog("  *** YOU DIED *** [R] restart", sf::Color(225,222,208)); }
                        break;
                    }
                }
                if (blocked)
                    addLog("  " + e->getName() + "'s " + sk->data.name + " was blocked.",
                           sf::Color(225,222,208));
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
                           sf::Color(225,222,208));
                    if (s.hp <= 0) { s.hp = 0; m_playerDead = true;
                        addLog("  *** YOU DIED *** [R] restart", sf::Color(225,222,208)); }
                }
            }
        }
    }
    e->advanceActTime();
    e->tickSkills();
}

// ============================================================
//  processCompanionActions – ผูกเข้าระบบ Aut/turn ของตัวเอง (ข้อ 3)
//  + Active AI: เลือกเป้าหมายใกล้สุดที่เห็นได้แล้วเดินเข้าไปฟัน (ข้อ 4)
//  ── ทำ 2 pass ──
//  Pass 1: เช็คก่อนว่า companion ตัวไหน "engaged" กับมอนอยู่
//          (adjacent หรือเห็นมอนในระยะ) โดยใช้ตำแหน่งปัจจุบัน
//          (ก่อน follow snap ของเทิร์นนี้)
//  Follow: snap เข้า trail เฉพาะตัวที่ "ไม่" engaged เท่านั้น
//  Pass 2: ตัว engaged ลงมือตี/เดินไล่ ตาม nextActTime ของตัวเอง
//  แก้บั๊ก: เดิมเรียก follow ก่อนเสมอ ทำให้ตำแหน่งที่เพิ่งไล่มอน
//  ไปเมื่อเทิร์นก่อนถูก snap กลับ formation ทุกเทิร์น (yo-yo,
//  ตีบ้างไม่ตีบ้าง)
//  (ย้ายออกมาจาก processTurn เดิม เพื่อแยก orchestration ออกจาก detail)
// ============================================================
void Game::processCompanionActions(long long playerTime)
{
    {
        auto& party = Party::instance();
        int n = party.size();

        std::vector<bool>   engaged(n, false);
        std::vector<Enemy*> adjTargets(n, nullptr);
        std::vector<Enemy*> chaseTargets(n, nullptr);

        // ── Pass 1: หา target ของแต่ละ companion ──
        for (int pi = 0; pi < n; pi++)
        {
            auto npc = party.getMember(pi);
            if (!npc || npc->isDead()) continue;

            for (auto* e : m_enemies)
            {
                if (e->isDead()) continue;
                int dx = std::abs(e->getCol() - npc->getCol());
                int dy = std::abs(e->getRow() - npc->getRow());
                if (dx <= 1 && dy <= 1 && (dx + dy) > 0) { adjTargets[pi] = e; break; }
            }
            if (adjTargets[pi]) { engaged[pi] = true; continue; }

            Enemy* nearest  = nullptr;
            int    bestDist = COMPANION_DETECT_RANGE * COMPANION_DETECT_RANGE + 1;
            for (auto* e : m_enemies)
            {
                if (e->isDead()) continue;
                if (!m_fog.isVisible(e->getCol(), e->getRow())) continue;
                int dx = e->getCol() - npc->getCol();
                int dy = e->getRow() - npc->getRow();
                int d2 = dx*dx + dy*dy;
                if (d2 <= COMPANION_DETECT_RANGE * COMPANION_DETECT_RANGE && d2 < bestDist)
                { bestDist = d2; nearest = e; }
            }
            if (nearest) { chaseTargets[pi] = nearest; engaged[pi] = true; }
        }

        // ── Follow trail เฉพาะตัวที่ไม่ engaged (ตัว engaged ตำแหน่งค้างไว้) ──
        updatePartyFollowPositions(engaged);

        // ── Pass 1.5: แจกช่องล้อมมอน (surround) คนละช่อง ──
        //    ไม่งั้น companion ทุกตัวจะพุ่งไปทิศเดียวกันแล้วต่อแถวชนกันเอง
        //    (ตัวที่ adjacent อยู่แล้ว "จอง" ช่องตัวเองไว้ก่อน กันตัวไล่หลัง
        //    เลือกช่องเดียวกัน)
        std::set<std::pair<int,int>> claimedTiles;
        for (int pi = 0; pi < n; pi++)
        {
            if (!adjTargets[pi]) continue;
            auto npc = party.getMember(pi);
            if (npc) claimedTiles.insert({npc->getCol(), npc->getRow()});
        }

        std::vector<std::pair<int,int>> moveTarget(n, {-1, -1});  // ช่องที่แต่ละตัวจะเดินไปหา
        for (int pi = 0; pi < n; pi++)
        {
            if (!chaseTargets[pi]) continue;
            auto npc = party.getMember(pi);
            if (!npc) continue;

            Enemy* e  = chaseTargets[pi];
            int    ec = e->getCol(), er = e->getRow();

            static const int offs[8][2] = {
                {-1,-1}, {0,-1}, {1,-1},
                {-1, 0},         {1, 0},
                {-1, 1}, {0, 1}, {1, 1}
            };

            std::pair<int,int> best = {-1, -1};
            int bestD = 1 << 30;
            for (auto& o : offs)
            {
                int tc = ec + o[0], tr = er + o[1];
                if (!m_tileMap.isWalkable(tc, tr)) continue;
                if (claimedTiles.count({tc, tr})) continue;
                if (m_player && m_player->getCol() == tc && m_player->getRow() == tr) continue;

                bool occByEnemy = false;
                for (auto* e2 : m_enemies)
                    if (!e2->isDead() && e2->getCol() == tc && e2->getRow() == tr) { occByEnemy = true; break; }
                if (occByEnemy) continue;

                int dx = tc - npc->getCol(), dy = tr - npc->getRow();
                int d  = dx * dx + dy * dy;
                if (d < bestD) { bestD = d; best = {tc, tr}; }
            }

            if (best.first != -1)
            {
                claimedTiles.insert(best);
                moveTarget[pi] = best;
            }
            else
            {
                moveTarget[pi] = {ec, er};  // ช่องรอบมอนเต็มหมด (ล้อมเกินคนแล้ว) → เดินตรงเข้าหาไปก่อน
            }
        }

        // ── Pass 2: ตัว engaged ลงมือ ──
        for (int pi = 0; pi < n; pi++)
        {
            if (!engaged[pi]) continue;
            auto npc = party.getMember(pi);
            if (!npc || npc->isDead()) continue;

            // ── ยังไม่ถึงเวลา → รอ ตำแหน่งค้างไว้เหมือนกัน (ไม่ snap follow) ──
            if (npc->getNextActTime() > playerTime) continue;

            if (adjTargets[pi])
            {
                companionAttack(npc, adjTargets[pi]);
                npc->advanceActTime();
                continue;
            }

            if (!chaseTargets[pi]) continue;
            auto [tc, tr] = moveTarget[pi];
            if (tc < 0) continue;

            int dc = 0, dr = 0;
            if (tc > npc->getCol()) dc = 1;
            else if (tc < npc->getCol()) dc = -1;
            if (tr > npc->getRow()) dr = 1;
            else if (tr < npc->getRow()) dr = -1;

            // ลองเดินทแยงก่อน ถ้าไม่ได้ลองแกนเดียว (แบบ greedy ธรรมดา)
            bool moved = tryMoveCompanion(npc, dc, dr);
            if (!moved && dc != 0 && dr != 0)
                moved = tryMoveCompanion(npc, dc, 0) || tryMoveCompanion(npc, 0, dr);

            if (moved)
                npc->advanceActTime();
            // เดินไม่ได้เลย (ทางตัน) → ไม่ advance รอเทิร์นหน้าลองใหม่
        }
    }
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
    m_npcManager.render(m_window);
    // วาด party companions ใน world (ลบออกจาก npcManager ตอน recruit แล้ว ต้องวาดแยก)
    for (auto& companion : Party::instance().getMembers())
    if (companion) m_npcManager.renderNPC(m_window, companion);

    if (m_player) m_player->render(m_window);
    m_fog.render(m_window, TILE_SIZE, m_gameView);

    if (m_hoverCol >= 0 && m_hoverRow >= 0)
    {
        // มุมสี่ด้าน (corner brackets) แทนกรอบสี่เหลี่ยมทึบ — ไม่มี fill
        const float tx = (float)(m_hoverCol * TILE_SIZE);
        const float ty = (float)(m_hoverRow * TILE_SIZE);
        const float ts = (float)TILE_SIZE;
        const float armLen = ts * 0.28f;   // ความยาวแขนมุม (ปรับได้)
        const float thick  = 3.f;          // ความหนาเส้น
        const sf::Color hoverColor(255, 255, 255, 200);

        auto drawCornerArm = [&](sf::Vector2f corner, float hSign, float vSign)
        {
            // hSign/vSign: -1 = แขนยื่นไปทางลบ, +1 = แขนยื่นไปทางบวก
            sf::RectangleShape h({armLen, thick});
            h.setPosition({ hSign < 0 ? corner.x - armLen : corner.x,
                             corner.y - thick / 2.f });
            h.setFillColor(hoverColor);
            m_window.draw(h);

            sf::RectangleShape v({thick, armLen});
            v.setPosition({ corner.x - thick / 2.f,
                             vSign < 0 ? corner.y - armLen : corner.y });
            v.setFillColor(hoverColor);
            m_window.draw(v);
        };

        // มุมบนซ้าย, บนขวา, ล่างซ้าย, ล่างขวา
        drawCornerArm({tx, ty},       +1.f, +1.f);
        drawCornerArm({tx + ts, ty},  -1.f, +1.f);
        drawCornerArm({tx, ty + ts},  +1.f, -1.f);
        drawCornerArm({tx + ts, ty + ts}, -1.f, -1.f);
    }

    if (m_ui.targeting.active) renderTargeting();

    m_window.setView(m_uiView);
    renderStatusPanel();
    renderRightPanel();
    renderLogPanel();
    if (m_ui.activePanel == UIState::Panel::Stats) renderStatsOverlay();
    renderFStatus();
    renderStatusEffects();  // ← เพิ่ม
    renderPartyUI();        // ← Party UI overlay
    if (m_ui.levelUpFlash) renderLevelUpEffect();
    if (m_ui.skillSelectOpen) renderSkillSelect();   // ← หน้าจอเลือกสกิลแบบ DCSS (Shift+Q) วาดทับบนสุด
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
//  F-status indicator (แทน Hotbar bar เดิม) — โชว์โหมด F ปัจจุบัน
//  + สกิลที่เลือกไว้ (ถ้าอยู่โหมด Skill)
// ============================================================
void Game::renderFStatus()
{
    if (!m_player || !m_fontLoaded) return;

    float x = 8.f, y = 558.f;

    bool isSkillMode = (m_ui.fMode == UIState::FMode::Skill);
    std::string modeLabel = isSkillMode ? "Skill" : "Bow";

    //sf::Text modeTxt(m_font, "[F] " + modeLabel + "   (Shift+F: switch, Shift+Q: choose skill)", 9);
    //modeTxt.setFillColor(sf::Color(160,160,160));
    //modeTxt.setPosition({x, y});
    //m_window.draw(modeTxt);

    if (!isSkillMode) return;

    const std::string& id = m_player->getSelectedSkillId();
    if (id.empty())
    {
        //sf::Text noneTxt(m_font, "  (no skill selected)", 9);
        //noneTxt.setFillColor(sf::Color(150,90,90));
        //noneTxt.setPosition({x, y + 12.f});
        //m_window.draw(noneTxt);
        return;
    }

    SkillInstance* sk = m_player->findSkill(id);
    char letter = m_player->getSkillLetter(id);
    std::string label = sk ? sk->data.name : id;
    std::string line = "  " + std::string(1, letter ? letter : '?') + ") " + label;
    sf::Color col = sf::Color(200,200,120);

    if (sk && !sk->isReady())
    {
        line += "  [cd:" + std::to_string(sk->cooldownLeft) + "]";
        col = sf::Color(180,80,80);
    }
    else if (sk && sk->buffActive)
    {
        line += "  [" + std::to_string(sk->durationLeft) + "t]";
        col = sf::Color(255,220,50);
    }
    else
    {
        line += "  ready";
        col = sf::Color(100,220,100);
    }

    sf::Text skillTxt(m_font, line, 9);
    skillTxt.setFillColor(col);
    skillTxt.setPosition({x, y + 12.f});
    m_window.draw(skillTxt);
}

// ============================================================
//  DCSS-style Skill Select overlay (Shift+Q)
//  แสดงสกิล active ทั้งหมดที่ผู้เล่นมี พร้อมตัวอักษร a-z/A-Z ประจำตัว
//  กดตัวอักษร → เลือกเป็นสกิลปัจจุบัน แล้วหน้าต่างพับลงทันที (ดู Game::selectSkillByLetter)
// ============================================================
void Game::renderSkillSelect()
{
    if (!m_player || !m_fontLoaded) return;

    sf::RectangleShape dim({(float)WINDOW_W, (float)WINDOW_H});
    dim.setFillColor(sf::Color(0,0,0,180));
    m_window.draw(dim);

    // เก็บรายชื่อสกิลที่มีตัวอักษร (active เท่านั้น — passive ไม่ได้ assign ตัวอักษร) เรียงตามตัวอักษร
    std::vector<std::pair<char,std::string>> entries;
    for (const auto& kv : m_player->getSkillLetters())
        entries.push_back({kv.second, kv.first});
    std::sort(entries.begin(), entries.end(),
        [](const auto& a, const auto& b){ return a.first < b.first; });

    float pw = 340.f;
    float ph = std::max(120.f, 60.f + entries.size() * 16.f + 20.f);
    ph = std::min(ph, (float)WINDOW_H - 40.f);
    float px = (WINDOW_W - pw) / 2.f;
    float py = (WINDOW_H - ph) / 2.f;

    sf::RectangleShape panel({pw, ph});
    panel.setFillColor(sf::Color(12,12,18));
    panel.setOutlineColor(sf::Color(120,120,120));
    panel.setOutlineThickness(1.f);
    panel.setPosition({px, py});
    m_window.draw(panel);

    sf::Text header(m_font, "Select a skill  (Esc: cancel)", 12);
    header.setFillColor(sf::Color(220,220,220));
    header.setPosition({px + 14.f, py + 12.f});
    m_window.draw(header);

    if (entries.empty())
    {
        sf::Text empty(m_font, "  (no active skills learned)", 10);
        empty.setFillColor(sf::Color(140,140,140));
        empty.setPosition({px + 14.f, py + 40.f});
        m_window.draw(empty);
        return;
    }

    float y = py + 40.f;
    const std::string& curSelected = m_player->getSelectedSkillId();

    for (const auto& [letter, id] : entries)
    {
        if (y + 14.f > py + ph - 10.f) break;  // เกินพื้นที่ panel — ตัดไว้ (มี scroll ได้ในอนาคตถ้าจำเป็น)

        const SkillInstance* sk = m_player->findSkill(id);
        std::string name = sk ? sk->data.name : id;

        std::string status;
        sf::Color col = sf::Color(200,200,200);
        if (sk && !sk->isReady())
        {
            status = "  [cd:" + std::to_string(sk->cooldownLeft) + "]";
            col = sf::Color(170,90,90);
        }
        else if (sk && sk->buffActive)
        {
            status = "  [" + std::to_string(sk->durationLeft) + "t]";
            col = sf::Color(230,200,60);
        }

        std::string line = std::string(1, letter) + ") " + name + status;
        if (id == curSelected) line += "  <selected>";

        sf::Text t(m_font, line, 10);
        t.setFillColor(id == curSelected ? sf::Color(120,220,255) : col);
        t.setPosition({px + 14.f, y});
        m_window.draw(t);
        y += 16.f;
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
    // ↓↓↓↓↓↓↓↓↓↓ แทรกตรงนี้ ↓↓↓↓↓↓↓↓↓↓
    if (m_ui.targeting.isArea)
    {
        int radius = m_ui.targeting.areaRadius;
        int tc = m_ui.targeting.targetCol;
        int tr = m_ui.targeting.targetRow;
        bool locked = m_ui.targeting.locked;

        SkillInstance* curSk = m_player->findSkill(m_ui.targeting.skillId);
        bool isLeap = curSk && curSk->data.type == SkillType::ActiveAoeWarp;

        sf::Color fillCol, outlineCol;
        if (isLeap)      { fillCol = sf::Color(80,255,170,90); outlineCol = sf::Color(120,255,190); }
        else if (locked) { fillCol = sf::Color(150,90,255,90); outlineCol = sf::Color(190,150,255); }
        else             { fillCol = sf::Color(255,140,40,90); outlineCol = sf::Color(255,200,50);  }

        for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
        {
            if (dx*dx + dy*dy > radius*radius) continue;
            int tx = tc + dx, ty = tr + dy;
            if (tx < 0 || ty < 0 || tx >= m_mapCols || ty >= m_mapRows) continue;

            sf::RectangleShape highlight({ts, ts});
            highlight.setFillColor(fillCol);
            highlight.setPosition({tx*ts, ty*ts});
            m_window.draw(highlight);
        }

        sf::RectangleShape center({ts, ts});
        center.setFillColor(sf::Color::Transparent);
        center.setOutlineColor(outlineCol);
        center.setOutlineThickness(2.f);
        center.setPosition({tc*ts, tr*ts});
        m_window.draw(center);
        return;
    }

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
            //float rawScale = ts * 0.6f / std::max((float)sz.x, (float)sz.y);
            //float scale = std::max(1.f, std::floor(rawScale)); // snap เป็น 1, 2, 3...
            //spr.setScale({scale, scale});
            float scale=ts * 0.8f /std::max((float)sz.x,(float)sz.y);
            spr.setScale({scale,scale});
            spr.setPosition({px+ts*0.1f ,py+ts*0.1f});
            //spr.setPosition({px+ts*0.2f,py+ts*0.2f});
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
    panel.setOutlineColor(sf::Color(120,120,120));
    panel.setOutlineThickness(1.f);
    panel.setPosition({px, py});
    m_window.draw(panel);

    int sz = 11;
    float lineH = sz + 8.f;
    float contentTop = py + 16.f;
    float contentBot = py + ph - 16.f;

    // y เริ่มจาก offset
    float y = contentTop - m_statsScrollOffset * lineH;
    float x = px + 20.f;

    // วาด line เฉพาะที่อยู่ใน panel
    // ── cache sf::Text ต่อบรรทัด: สร้างครั้งแรกครั้งเดียว เฟรมต่อไปแค่ setString/setPosition ──
    // (เดิมสร้าง sf::Text ใหม่ ~30 ตัวทุกเฟรมตอนพาเนลเปิด → กิน FPS)
    size_t lineIdx = 0;
    auto line = [&](const std::string& label, const std::string& val,
                    sf::Color col = sf::Color(200,200,200))
    {
        size_t idx = lineIdx++;
        if (y >= contentTop && y + lineH <= contentBot)
        {
            if (idx >= m_statsOverlayCache.size())
                m_statsOverlayCache.emplace_back();
            auto& opt = m_statsOverlayCache[idx];
            if (!opt) opt.emplace(m_font, "", sz);

            std::string str = label + val;
            if (opt->getString() != str) opt->setString(str);
            opt->setFillColor(col);
            opt->setPosition({x, y});
            m_window.draw(*opt);
        }
        y += lineH;
    };

    // ส่วนที่เหลือของฟังก์ชัน (บรรทัด line(...) ทั้งหมด) ไม่ต้องแก้เลยครับ

    line("Level:       ", std::to_string(s.level));
    line("EXP:         ", std::to_string(s.exp) + "/" + std::to_string(s.expToNext));
    line("HP:          ", std::to_string(s.hp) + "/" + std::to_string(m_finalStats.maxHp));
    line("Strength:    ", std::to_string(s.strength));
    line("Weight:      ", std::to_string(s.bodyWeight + s.equipWeight) + " kg");
    line("Move Aut:    ", std::to_string(s.moveAut));
    line("Atk Aut:     ", std::to_string(s.atkAut));
    line("ATK:         ", std::to_string(m_finalStats.atk));
    line("MATK:        ", std::to_string(m_finalStats.matk));
    line("DEF:         ", std::to_string(m_finalStats.def));
    line("MDEF:        ", std::to_string(m_finalStats.mdef));
    
    // Stamina bar
    {
        std::string stam = std::to_string(s.stamina) + "/" + std::to_string(s.maxStamina);
        sf::Color stCol = s.stamina < s.maxStamina / 3 ? sf::Color(255,120,50)
                        : s.stamina < s.maxStamina * 2 / 3 ? sf::Color(255,200,80)
                        : sf::Color(80,220,140);
        line("Stamina:     ", stam, stCol);
    }
    line("Mana:        ", std::to_string(s.mana) + "/" + std::to_string(m_finalStats.maxMana));
    
    
    line("Dodge:       ", std::to_string(m_finalStats.dodge));
    line("Hunger:      ", std::to_string(s.hunger) + "/100");
    

    

    line("SlaDmg:      ", std::to_string(m_finalStats.slashDmgBonus));
    line("PieDmg:      ", std::to_string(m_finalStats.pierceDmgBonus));
    line("BluDmg:      ", std::to_string(m_finalStats.bluntDmgBonus));
    line("CleDmg:      ", std::to_string(m_finalStats.cleaveDmgBonus));

    line("BleedDmg:    ", std::to_string(m_finalStats.bleedBonus));
    line("PoisonDmg:   ", std::to_string(m_finalStats.poisonBonus));
    line("BurnDmg:     ", std::to_string(m_finalStats.burnBonus));

    line("BleRes%:     ", std::to_string(m_finalStats.resistBleed));
    line("PoiRes%:     ", std::to_string(m_finalStats.resistPoison));
    line("BurRes%:     ", std::to_string(m_finalStats.resistBurn));
    line("StuRes%:     ", std::to_string(m_finalStats.resistStun));
    line("SloRes%:     ", std::to_string(m_finalStats.resistSlow));

    line("BleDmgRed:   ", std::to_string(m_finalStats.bleedDmgReduce));
    line("PoiDmgRed:   ", std::to_string(m_finalStats.poisonBonus));
    line("BurDmgRed:   ", std::to_string(m_finalStats.burnDmgReduce));

    line("BleDurRed:   ", std::to_string(m_finalStats.bleedDurReduce));
    line("PoiDurRed:   ", std::to_string(m_finalStats.poisonDurReduce));
    line("BurDurRed:   ", std::to_string(m_finalStats.burnDurReduce));
    line("StuDurRed:   ", std::to_string(m_finalStats.stunDurReduce));
    line("SloDurRed:   ", std::to_string(m_finalStats.slowDurReduce));

    

    
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
            std::vector<sf::Text> pendingCounts;

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
                            auto tb = ct.getLocalBounds();
                            ct.setPosition({sx + SZ - tb.size.x - 3.f, sy + SZ - 11.f});
                            pendingCounts.push_back(ct);
                        }
                    }
                }
            for (auto& ct : pendingCounts) m_window.draw(ct);

            Item* selItem = m_inventory.getItem(m_ui.selectedInvSlot);
            std::string info = "(empty)";
            if (selItem) {
                int cnt = m_inventory.getCount(m_ui.selectedInvSlot);
                info = selItem->name + " (" + std::to_string(selItem->value) + ")";
                if (cnt > 1) info += " x" + std::to_string(cnt);
            }
            sf::Text infoTxt(m_font, info, 9);
            infoTxt.setFillColor(sf::Color::White);   // ← สีเดียวกับ Status Panel (renderStatusPanel)
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
    m_log.emplace_back(m_font, msg, color);
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
    for (auto& entry:m_log)
    {
        entry.renderText.setPosition({8.f,y}); m_window.draw(entry.renderText);
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
    titleBox.setFillColor(sf::Color(155, 155, 155));
    titleBox.setOutlineColor(sf::Color(180, 180, 180));
    titleBox.setOutlineThickness(2.f);
    titleBox.setPosition({(float)WINDOW_W/2.f - 160.f, 24.f});
    m_window.draw(titleBox);

    sf::Text title(m_font, "SELECT YOUR CHARACTER", 14);
    title.setFillColor(sf::Color(50, 50, 50));
    auto tb = title.getLocalBounds();
    title.setPosition({(float)WINDOW_W/2.f - tb.size.x/2.f, 36.f});
    //title.setPosition({252.5f, 36.f});
    m_window.draw(title);

    // grid: 3 cols x 2 rows
    const float CARD_W = 180.f;
    const float CARD_H = 205.f;
    const float GAP    = 12.f;
    const int   COLS   = 3;
    const int   TOTAL  = 6; // 5 races + 1 ???

    float gridW = COLS * CARD_W + (COLS - 1) * GAP;
    //float startX = 1.f;
    float startX = (float)WINDOW_W/2.f - gridW/2.f;
    float startY = 80.f;

    auto& tm = TextureManager::instance();

    for (int i = 0; i < TOTAL; ++i)
    {
        int col = i % COLS;
        int row = i / COLS;
        float cx = startX + col * (CARD_W + GAP);
        float cy = startY + row * (CARD_H + GAP);

        bool sel = (i == m_ui.selectedRace);
        bool isEmpty = (i >= (int)races.size()); // slot ???

        // card bg
        sf::RectangleShape card({CARD_W, CARD_H});
        card.setFillColor(sf::Color(155, 155, 155));
        card.setOutlineColor(sel ? sf::Color(255, 255, 255) : sf::Color(90, 90, 90));
        card.setOutlineThickness(sel ? 3.f : 1.5f);
        card.setPosition({cx, cy});
        m_window.draw(card);
//sf::Color(255, 220, 50)
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
                cy + 10.f
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
        sf::Text name(m_font, r.name, 16);
        name.setFillColor(sel ? sf::Color(255, 255, 255) : sf::Color(200, 200, 200));
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
void Game::renderStatusEffects()
{
    if (!m_player || !m_fontLoaded) return;

    const float SZ    = 32.f;
    const float GAP   = 4.f;
    const float Y     = 558.f;
    const float RIGHT = 550.f;

    // ── 1. Passive skills — ชิดขวาสุด ──
    std::vector<const SkillInstance*> passives;
    for (const auto& sk : m_player->getSkills())
        if (sk.data.type == SkillType::Passive && sk.fromCore)
            passives.push_back(&sk);

    if (m_passiveCache.size() < passives.size())
        m_passiveCache.resize(passives.size());

    float passiveStartX = RIGHT - passives.size() * (SZ + GAP);
    for (int i = 0; i < (int)passives.size(); ++i)
    {
        const auto& sk = *passives[i];
        float sx = passiveStartX + i * (SZ + GAP);
        StatusIconCache& c = m_passiveCache[i];

        c.slotBg.setFillColor(sf::Color(20, 20, 20, 220));
        c.slotBg.setOutlineColor(sf::Color(100, 200, 255));
        c.slotBg.setOutlineThickness(2.f);
        c.slotBg.setPosition({sx, Y});
        m_window.draw(c.slotBg);

        if (sk.data.id != c.lastKey)
        {
            c.lastKey = sk.data.id;
            c.icon.reset();
            c.fallbackTxt.reset();
            if (!sk.data.icon.empty())
            {
                const sf::Texture* tex = TextureManager::instance().get(sk.data.icon);
                if (tex)
                {
                    c.icon.emplace(*tex);
                    auto tsz = tex->getSize();
                    float sc = SZ / std::max((float)tsz.x, (float)tsz.y);
                    c.icon->setScale({sc, sc});
                }
            }
            if (!c.icon)
            {
                c.fallbackTxt.emplace(m_font, sk.data.name.substr(0, 3), 7);
                c.fallbackTxt->setFillColor(sf::Color(100, 200, 255));
            }
        }
        if (c.icon)
        {
            c.icon->setPosition({sx, Y});
            m_window.draw(*c.icon);
        }
        else if (c.fallbackTxt)
        {
            c.fallbackTxt->setPosition({sx + 2.f, Y + 2.f});
            m_window.draw(*c.fallbackTxt);
        }
    }

    // ── 2. Active buffs — ซ้ายของ passive ──
    std::vector<const SkillInstance*> buffs;
    for (const auto& sk : m_player->getSkills())
        if (sk.data.type == SkillType::ActiveBuff && sk.buffActive)
            buffs.push_back(&sk);

    if (m_buffCache.size() < buffs.size())
        m_buffCache.resize(buffs.size());

    float buffStartX = passiveStartX - buffs.size() * (SZ + GAP) - GAP;
    for (int i = 0; i < (int)buffs.size(); ++i)
    {
        const auto& sk = *buffs[i];
        float sx = buffStartX + i * (SZ + GAP);
        StatusIconCache& c = m_buffCache[i];

        c.slotBg.setFillColor(sf::Color(20, 20, 20, 220));
        c.slotBg.setOutlineColor(sf::Color(255, 200, 50));
        c.slotBg.setOutlineThickness(2.f);
        c.slotBg.setPosition({sx, Y});
        m_window.draw(c.slotBg);

        if (sk.data.id != c.lastKey)
        {
            c.lastKey = sk.data.id;
            c.icon.reset();
            c.fallbackTxt.reset();
            if (!sk.data.icon.empty())
            {
                const sf::Texture* tex = TextureManager::instance().get(sk.data.icon);
                if (tex)
                {
                    c.icon.emplace(*tex);
                    auto tsz = tex->getSize();
                    float sc = SZ / std::max((float)tsz.x, (float)tsz.y);
                    c.icon->setScale({sc, sc});
                }
            }
            if (!c.icon)
            {
                c.fallbackTxt.emplace(m_font, sk.data.name.substr(0, 3), 7);
                c.fallbackTxt->setFillColor(sf::Color(255, 200, 50));
            }
        }
        if (c.icon)
        {
            c.icon->setPosition({sx, Y});
            m_window.draw(*c.icon);
        }
        else if (c.fallbackTxt)
        {
            c.fallbackTxt->setPosition({sx + 2.f, Y + 2.f});
            m_window.draw(*c.fallbackTxt);
        }

        // duration remaining — เปลี่ยนทุกเทิร์น ใช้ setString แทนสร้างใหม่
        if (!c.durTxt)
        {
            c.durTxt.emplace(m_font, std::to_string(sk.durationLeft), 9);
            c.durTxt->setFillColor(sf::Color::White);
        }
        else
        {
            c.durTxt->setString(std::to_string(sk.durationLeft));
        }
        c.durTxt->setPosition({sx + 2.f, Y + SZ - 11.f});
        m_window.draw(*c.durTxt);
    }

    // ── 3. Status effects (debuff) — ซ้ายของ buff ──
    const auto& effects = m_player->getStats().statusEffects;

    if (m_statusEffectCache.size() < effects.size())
        m_statusEffectCache.resize(effects.size());

    float statusStartX = buffs.empty()
        ? passiveStartX - effects.size() * (SZ + GAP) - GAP
        : buffStartX - effects.size() * (SZ + GAP) - GAP;

    for (int i = 0; i < (int)effects.size(); ++i)
    {
        const auto& se = effects[i];
        float sx = statusStartX + i * (SZ + GAP);
        StatusIconCache& c = m_statusEffectCache[i];

        const StatusEffectData* data = StatusEffectDB::instance().get(se.typeId());
        sf::Color outlineCol = data ? sf::Color(data->r, data->g, data->b)
                                : sf::Color::White;

        c.slotBg.setFillColor(sf::Color(20, 20, 20, 220));
        c.slotBg.setOutlineColor(outlineCol);
        c.slotBg.setOutlineThickness(2.f);
        c.slotBg.setPosition({sx, Y});
        m_window.draw(c.slotBg);

        if (se.typeId() != c.lastKey)
        {
            c.lastKey = se.typeId();
            c.icon.reset();
            c.fallbackTxt.reset();
            const sf::Texture* tex = data
                ? TextureManager::instance().get(data->icon)
                : nullptr;
            if (tex)
            {
                c.icon.emplace(*tex);
                auto tsz = tex->getSize();
                float sc = SZ / std::max((float)tsz.x, (float)tsz.y);
                c.icon->setScale({sc, sc});
            }
            else
            {
                c.fallbackTxt.emplace(m_font, se.typeId().substr(0, 3), 7);
            }
        }
        // สีของ fallback อาจเปลี่ยนได้ถ้า data เปลี่ยน (rare) — เซ็ตสีทุกเฟรมไว้ ถูกกว่าสร้างใหม่มาก
        if (c.icon)
        {
            c.icon->setPosition({sx, Y});
            m_window.draw(*c.icon);
        }
        else if (c.fallbackTxt)
        {
            c.fallbackTxt->setFillColor(outlineCol);
            c.fallbackTxt->setPosition({sx + 2.f, Y + 2.f});
            m_window.draw(*c.fallbackTxt);
        }

        if (!c.durTxt)
        {
            c.durTxt.emplace(m_font, std::to_string(se.duration), 9);
            c.durTxt->setFillColor(sf::Color::White);
        }
        else
        {
            c.durTxt->setString(std::to_string(se.duration));
        }
        c.durTxt->setPosition({sx + 2.f, Y + SZ - 11.f});
        m_window.draw(*c.durTxt);
    }
}

void Game::renderPartyUI()
{
    m_partyUI.render(m_window, Party::instance(), m_player);
}

// ============================================================
//  dismissSelectedCompanion – ไล่ companion ที่ cursor เลือกอยู่ใน
//  PartyUI ออกจากปาร์ตี้ (ข้อ 5)
//  ดีไซน์: คืนกลับไปยืนบนแมพที่ตำแหน่งผู้เล่นปัจจุบัน แล้ว add เข้า
//  NPCManager ใหม่ → ไม่หายไปถาวร คุยแล้ว recruit กลับเข้าปาร์ตี้ได้อีก
//  เหมือน spawn ปกติ (pattern เดียวกับ spawnNPCs: setPos แล้ว add)
// ============================================================
void Game::dismissSelectedCompanion()
{
    auto& party = Party::instance();
    int idx = m_partyUI.getSelectedIndex();
    auto npc = party.getMember(idx);
    if (!npc) return;

    if (m_player)
    {
        npc->setPos(m_player->getCol(), m_player->getRow());
        npc->setFacing(m_player->getFacingLeft());
    }

    std::string name = npc->getName();
    party.removeMember(npc->getId());   // เอาออกจากปาร์ตี้ก่อน (ล้าง trail-follow/combat state ของตัวนี้)
    m_npcManager.add(npc);              // แล้วโผล่กลับบนแมพ ให้คุยแล้ว recruitNPC() ซ้ำได้

    addLog("  " + name + " leaves the party.", sf::Color(225,222,208));
    m_partyUI.resetSelection(party.size());
}

// ============================================================
//  reorderSelectedCompanion – สลับตำแหน่ง companion ที่เลือกกับคนที่
//  อยู่ติดกัน (ข้อ 5). trail-follow index และ surround-slot ของข้อ 3-4
//  อ่านตำแหน่งจาก Party::getMembers() ตรงๆ อยู่แล้ว จึงแค่ swap ก็พอ
//  ไม่ต้องแก้ระบบ follow/combat ใดๆ เพิ่ม
// ============================================================
void Game::reorderSelectedCompanion(int dir)
{
    auto& party = Party::instance();
    int idx    = m_partyUI.getSelectedIndex();
    int target = idx + dir;
    if (target < 0 || target >= party.size()) return;  // ชนขอบแถวแล้ว ไม่ต้องทำอะไร

    if (party.swapMembers(idx, target))
        m_partyUI.setSelectedIndex(target);  // cursor เกาะไปกับตัวที่ย้าย
}

void Game::recruitNPC(const std::string& npcId)
{
    // Get NPC template from database
    auto npcData = NPCDB::instance().get(npcId);
    if (!npcData)
    {
        addLog("Could not recruit: NPC not found");
        return;
    }
    
    // Create NPC instance
    auto npc = NPCDB::instance().createNPC(npcId);
    if (!npc)
    {
        addLog("Could not create NPC instance");
        return;
    }
    
    // Try to add to party
    auto& party = Party::instance();
    if (party.isFull())
    {
        addLog("Party is full!");
        return;
    }
    
    if (party.hasMember(npc->getId()))
    {
        addLog(npc->getName() + " is already in your party");
        return;
    }
    
    party.addMember(npc);
    addLog("Recruited " + npc->getName() + "!");
    
    // Remove from map and NPC manager
    m_npcManager.remove(npcId);
}

void Game::tryInteractNPC()
{
    if (!m_player) return;
    
    int playerCol = m_player->getCol();
    int playerRow = m_player->getRow();

    // ── ข้อ 7: ปลุก companion ที่ downed อยู่ก่อน ถ้ามีตัวใดยืนติดอยู่ ──
    // (เช็คก่อน recruit เพราะ downed companion ก็ยืนติด player อยู่แล้วเสมอ
    // จาก follow trail ตำแหน่งจะใกล้กว่าตัว NPC บนแมพทั่วไป)
    for (auto& member : Party::instance().getMembers())
    {
        if (!member || !member->isDead()) continue;
        int dc = std::abs(member->getCol() - playerCol);
        int dr = std::abs(member->getRow() - playerRow);
        if (dc <= 1 && dr <= 1)
        {
            member->revive();
            addLog("  " + member->getName() + " is back on their feet!", sf::Color(225,222,208));
            processTurn();  // Count as action
            return;
        }
    }
    
    // Check all 8 adjacent tiles for NPCs
    int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    
    for (int i = 0; i < 8; i++)
    {
        int checkCol = playerCol + dx[i];
        int checkRow = playerRow + dy[i];
        
        // Bounds check
        if (checkCol < 0 || checkCol >= m_mapCols || checkRow < 0 || checkRow >= m_mapRows)
        continue;
        
        auto npc = m_npcManager.getNPCAt(checkCol, checkRow);
        if (npc && npc->getType() == NPCType::Companion)
        {
            // Found a companion NPC, recruit them
            std::string npcId = npc->getId();
            recruitNPC(npcId);
            processTurn();  // Count as action
            return;
        }
    }
    // ก่อน waitTurn() ที่ท้ายฟังก์ชัน
    //addLog("No adjacent NPC found", sf::Color(225,222,208));
    // No adjacent NPC found, just wait
    waitTurn();
}

void Game::updatePartyFollowPositions(const std::vector<bool>& skip)
{
    auto& party = Party::instance();
    if (party.size() == 0 || !m_player) return;

    for (int i = 0; i < party.size(); i++)
    {
        // ── ข้าม companion ที่กำลัง engaged กับมอนอยู่ (ข้อ 4) ──
        //    ไม่งั้น follow trail จะ snap ทับตำแหน่งที่เพิ่งไล่/ยืนตีมอนไป
        if (i < (int)skip.size() && skip[i]) continue;

        auto companion = party.getMember(i);
        if (!companion) continue;

        // companion i ดูตำแหน่ง trail[(i+1) * TRAIL_SPACING]
        int trailIdx = (i + 1) * Party::TRAIL_SPACING;
        const TrailPoint* tp = party.getTrailPoint(trailIdx);
        if (!tp) continue;

        companion->setPos(tp->col, tp->row);
        companion->setFacing(tp->facingLeft);
    }
}