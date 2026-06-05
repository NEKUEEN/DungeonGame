#include "Game.hpp"
#include "TextureManager.hpp"
#include "MonsterDB.hpp"
#include "DropTable.hpp"
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <optional>
#include <cmath>

// ============================================================
//  Constructor / Destructor
// ============================================================
Game::Game()
    : m_window(sf::VideoMode({WINDOW_W, WINDOW_H}), "Dungeon and Stone",
               sf::Style::Titlebar | sf::Style::Close)
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

    m_gameView.setSize({(float)GAME_VIEW_W*1.25f,(float)GAME_VIEW_H*1.25f});
    m_gameView.setViewport(sf::FloatRect({0.f,0.f},
        {(float)GAME_VIEW_W/WINDOW_W,(float)GAME_VIEW_H/WINDOW_H}));
    m_uiView.setSize({(float)WINDOW_W,(float)WINDOW_H});
    m_uiView.setCenter({(float)WINDOW_W/2.f,(float)WINDOW_H/2.f});

    newDungeon(false);
}

Game::~Game() { delete m_player; clearEnemies(); }

// ============================================================
//  Stats Helpers
// ============================================================
int Game::computeBody() const
{
    if (!m_player) return 0;
    const Stats& s = m_player->getStats();
    CoreStats cb   = m_coreSlots.getTotalBonus();
    int hp    = s.maxHp    + cb.hp    + m_equipment.getTotalHpBonus();
    int atk   = s.maxAtk   + cb.atk   + m_equipment.getTotalAtkBonus();
    int def   = s.maxDef   + cb.def   + m_equipment.getTotalDefBonus();
    int dodge = s.maxDodge + cb.dodge + m_equipment.getTotalDodgeBonus();
    return (int)(hp*0.4f + atk*0.3f + def*0.2f + dodge*0.1f);
}

int Game::computeMentality() const
{
    if (!m_player) return 0;
    const Stats& s = m_player->getStats();
    CoreStats cb   = m_coreSlots.getTotalBonus();
    int mana = s.maxMana + cb.mana             + m_equipment.getTotalManaBonus();
    int magicDmg = s.maxMagicDmg + cb.magicDmg + m_equipment.getTotalMagicDmgBonus();
    int magicRes = s.maxMagicRes + cb.magicRes + m_equipment.getTotalMagicResBonus();
    return (int)(mana * 0.4f + magicDmg * 0.3f + magicRes * 0.3f);
}

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

int Game::computeBattleIndex() const
{
    if (!m_player) return 0;
    const Stats& s = m_player->getStats();
    CoreStats cb   = m_coreSlots.getTotalBonus();
    int body      = computeBody();
    int mentality = computeMentality();
    int itemLv    = getItemLevelTotal();
    float bonusPct = 0.30f + (s.level - 1) * 0.05f;
    return (int)(body + mentality + itemLv * bonusPct);
}

void Game::drainMentality()
{
    if (!m_player) return;
    Stats& s = m_player->getStats();
    if (!s.hpDepleted || m_playerDead) return;
    s.mentality -= 3;
    if (s.mentality < 0) s.mentality = 0;
    addLog("> Mentality -3/turn ("+std::to_string(s.mentality)+"/"+std::to_string(s.maxMentality)+")",
           sf::Color(180,80,220));
    if (s.mentality <= 0)
    {
        m_playerDead = true;
        addLog(" *** Mind shattered   DEAD *** [R] restart", sf::Color(255,50,50));
    }
}

// ============================================================
//  AP Helpers
// ============================================================
void Game::recalcAP()
{
    if (!m_player) return;
    Stats& s = m_player->getStats();
    int ap = 1;

    for (const auto& sk : m_player->getSkills())
    {
        if (!sk.buffActive && !sk.isPassive()) continue;
        ap += sk.data.effect.speedBonus;
    }
    s.maxAP     = ap;
    s.currentAP = ap;

    int atkCost = 100;
    for (const auto& sk : m_player->getSkills())
    {
        if (!sk.buffActive && !sk.isPassive()) continue;
        if (sk.data.effect.atkSpeedPct < atkCost)
            atkCost = sk.data.effect.atkSpeedPct;
    }
    s.atkSpeedCost = atkCost;
    s.atkAPAccum   = 0;
}

// ============================================================
//  Skill Helpers
// ============================================================
int Game::getBuffedAtk() const
{
    if (!m_player) return 0;
    const Stats& s = m_player->getStats();
    CoreStats cb   = m_coreSlots.getTotalBonus();
    int base = s.maxAtk + cb.atk + m_equipment.getTotalAtkBonus();
    for (const auto& sk : m_player->getSkills())
        if (sk.buffActive && sk.data.type == SkillType::ActiveBuff)
            base = base + base * sk.data.effect.atkPct / 100;
    return base;
}

int Game::getBuffedDef() const
{
    if (!m_player) return 0;
    const Stats& s = m_player->getStats();
    int base = s.maxDef + m_equipment.getTotalDefBonus();
    for (const auto& inst : m_player->getSkills())
        if (inst.data.type == SkillType::Passive && inst.data.effect.defPct > 0)
            base = base + base * inst.data.effect.defPct / 100;
    return base;
}

void Game::useSkillBuff(const std::string& skillId)
{
    if (!m_player) return;
    SkillInstance* sk = m_player->findSkill(skillId);
    if (!sk || sk->data.type != SkillType::ActiveBuff) return;
    if (!sk->isReady())
    {
        addLog("> "+sk->data.name+" cooldown: "+std::to_string(sk->cooldownLeft)+" turns",
               sf::Color(160,160,160));
        return;
    }
    sk->buffActive   = true;
    sk->durationLeft = sk->data.duration;
    sk->cooldownLeft = sk->data.cooldown;
    addLog("> "+sk->data.name+" activated! ATK +"+std::to_string(sk->data.effect.atkPct)+
           "% for "+std::to_string(sk->data.duration)+" turns", sf::Color(255,200,50));
    recalcAP();
}

// ============================================================
//  executeSkill
// ============================================================
void Game::executeSkill(int hotbarIdx)
{
    if (!m_player) return;
    const std::string& id = m_player->getHotbar(hotbarIdx);
    if (id.empty()) { addLog("> Slot "+std::to_string(hotbarIdx+1)+" empty."); return; }

    SkillInstance* sk = m_player->findSkill(id);
    if (!sk) return;

    if (!sk->isReady())
    {
        addLog("> "+sk->data.name+" cooldown: "
               +std::to_string(sk->cooldownLeft)+" turns",
               sf::Color(160,160,160));
        return;
    }

    switch (sk->data.type)
    {
    case SkillType::ActiveBuff:
        sk->buffActive   = true;
        sk->durationLeft = sk->data.duration;
        sk->cooldownLeft = sk->data.cooldown;
        addLog("> "+sk->data.name+" activated!", sf::Color(255,200,50));
        recalcAP();
        break;

    case SkillType::ActiveHeal:
    {
        Stats& s = m_player->getStats();
        int heal = sk->data.effect.healFlat
                 + s.maxHp * sk->data.effect.healPct / 100;
        s.hp = std::min(s.maxHp, s.hp + heal);
        if (s.hp > 0 && s.hpDepleted)
        {
            s.hpDepleted = false;
            addLog("> HP recovered", sf::Color(100,220,180));
        }
        sk->cooldownLeft = sk->data.cooldown;
        addLog("> "+sk->data.name+" +"+std::to_string(heal)+" HP",
               sf::Color(80,220,120));
        break;
    }

    case SkillType::ActiveRanged:
        m_targetingMode = true;
        m_targetSkillId = id;
        m_targetCol = m_player->getCol();
        m_targetRow = m_player->getRow();
        addLog(" [Targeting] arrow/HJKL | Enter=fire | Esc=cancel",
               sf::Color(255,220,50));
        break;

    case SkillType::ActiveWarp:
        m_targetingMode = true;
        m_targetSkillId = id;
        m_targetCol = m_player->getCol();
        m_targetRow = m_player->getRow();
        addLog(" [Warp] choose destination | Enter=warp | Esc=cancel",
               sf::Color(100,200,255));
        break;

    case SkillType::ActiveAoe:
        executeAoe(sk);
        sk->cooldownLeft = sk->data.cooldown;
        break;

    case SkillType::Passive:
        addLog("> "+sk->data.name+" is passive.", sf::Color(120,200,120));
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
            addLog("> "+sk->data.name+" hits "+e->getName()+" for "+std::to_string(dmg)+"!",
                   sf::Color(255,180,50));
            hits++;
        }
    }
    if (hits == 0)
        addLog("> "+sk->data.name+" — no targets in range.", sf::Color(140,140,140));

    processTurn(); tryRespawnEnemies();
    m_fog.compute(m_player->getCol(), m_player->getRow(), VIEW_RADIUS, m_tileMap);
}

void Game::executeWarp(int col, int row)
{
    if (!m_tileMap.isWalkable(col, row))
    { addLog("> Cannot warp there!"); return; }
    m_player->setPos(col, row);
    SkillInstance* sk = m_player->findSkill(m_targetSkillId);
    if (sk) sk->cooldownLeft = sk->data.cooldown;
    addLog("> Warped!", sf::Color(100,200,255));
    m_fog.compute(col, row, VIEW_RADIUS, m_tileMap);
    updateCamera();
    processTurn(); tryRespawnEnemies();
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
    if (!m_player) return;
    SkillInstance* sk = m_player->findSkill("stone_throw");
    if (!sk) return;

    if (!sk->isReady())
    {
        addLog("> Stone Throw cooldown: "+std::to_string(sk->cooldownLeft)+" turns",
               sf::Color(160,160,160));
        return;
    }

    m_targetingMode  = true;
    m_targetSkillId  = "stone_throw";
    m_targetCol = m_player->getCol();
    m_targetRow = m_player->getRow();

    addLog(" [Targeting] Move cursor: arrow/HJKLYUBN", sf::Color(255,220,50));
    addLog(" Fire: Enter/Space  |  Cancel: Esc", sf::Color(255,220,50));
}

void Game::moveTargetCursor(int dc, int dr)
{
    if (!m_player) return;
    SkillInstance* sk = m_player->findSkill(m_targetSkillId);
    if (!sk) return;

    int nc = m_targetCol + dc;
    int nr = m_targetRow + dr;

    int ddx = nc - m_player->getCol();
    int ddy = nr - m_player->getRow();
    float dist = std::sqrt((float)(ddx*ddx + ddy*ddy));
    if (dist > sk->data.effect.range)
    {
        addLog("> Out of range (max "+std::to_string(sk->data.effect.range)+" tiles)",
               sf::Color(220,100,50));
        return;
    }

    nc = std::clamp(nc, 0, MAP_COLS-1);
    nr = std::clamp(nr, 0, MAP_ROWS-1);

    m_targetCol = nc;
    m_targetRow = nr;
}

void Game::confirmTarget()
{
    if (!m_player) return;
    m_targetingMode = false;
    SkillInstance* sk = m_player->findSkill(m_targetSkillId);
    if (!sk) return;

    if (sk->data.type == SkillType::ActiveRanged)
        fireRangedAt(m_targetCol, m_targetRow);
    else if (sk->data.type == SkillType::ActiveWarp)
        executeWarp(m_targetCol, m_targetRow);
}

void Game::cancelTargeting()
{
    m_targetingMode = false;
    addLog("> Targeting cancelled.", sf::Color(140,140,140));
}

void Game::fireRangedAt(int targetCol, int targetRow)
{
    if (!m_player) return;
    SkillInstance* sk = m_player->findSkill(m_targetSkillId);
    if (!sk) return;

    sk->cooldownLeft = sk->data.cooldown;

    int pc = m_player->getCol();
    int pr = m_player->getRow();
    int baseAtk = getBuffedAtk();
    int projDmg = std::max(1, baseAtk * sk->data.effect.damagePct / 100);

    auto line = getLine(pc, pr, targetCol, targetRow);
    bool hit = false;

    for (int i = 1; i < (int)line.size(); ++i)
    {
        int tx = line[i].x;
        int ty = line[i].y;

        if (m_tileMap.getTile(tx, ty) == TileType::Wall) break;

        int ddx = tx - pc, ddy = ty - pr;
        if (std::sqrt((float)(ddx*ddx+ddy*ddy)) > sk->data.effect.range) break;

        for (auto* e : m_enemies)
        {
            if (!e->isDead() && e->getCol()==tx && e->getRow()==ty)
            {
                e->takeDamage(projDmg);
                addLog("> " + sk->data.name + " hits " + e->getName() + " for " + std::to_string(projDmg) + "!",
                       sf::Color(180,220,255));

                if (e->isDead())
                {
                    addLog("> "+e->getName()+" dead! +"+std::to_string(e->getExp())+" EXP",
                           sf::Color(220,80,80));

                    auto dropped = DropTable::instance().roll(e->getId());
                    for (const auto& itemId : dropped)
                    {
                        const ItemData* idata = DropTable::instance().getItem(itemId);
                        if (!idata) continue;
                        Item drop;
                        drop.id         = idata->id;
                        drop.type       = (idata->type=="Core") ? ItemType::Core : ItemType::Material;
                        drop.name       = idata->name;
                        drop.desc       = idata->desc;
                        drop.value      = idata->value;
                        drop.hpBonus    = idata->hpBonus;
                        drop.atkBonus   = idata->atkBonus;
                        drop.defBonus   = idata->defBonus;
                        drop.dodgeBonus = idata->dodgeBonus;
                        drop.manaBonus  = idata->manaBonus;
                        drop.magicDmgBonus = idata->magicDmgBonus;
                        drop.magicResBonus = idata->magicResBonus;
                        drop.spriteName = idata->sprite;
                        drop.stackable  = idata->stackable;
                        drop.col        = e->getCol();
                        drop.row        = e->getRow();
                        m_mapItems.push_back(drop);
                        addLog("> Dropped: "+idata->name, sf::Color(180,220,255));
                    }

                    if (m_player->addExp(e->getExp()))
                    {
                        m_levelUpFlash=true; m_levelUpTimer=120;
                        int lv = m_player->getStats().level;
                        m_coreSlots.setSlotCount(lv);
                        addLog("> *** LEVEL UP! Level "+std::to_string(lv)+" ***",
                               sf::Color(255,255,50));
                    }
                }
                hit = true;
                break;
            }
        }
        if (hit) break;
    }

    if (!hit)
        addLog("> Stone flies and hits nothing.", sf::Color(140,140,140));

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
    m_tileMap.generate();
    m_fog.reset();
    m_playerDead    = false;
    m_targetingMode = false;

    if (!keepPlayer)
    {
        delete m_player; m_player=nullptr;
        m_dungeonFloor=1;
        m_inventory=Inventory();
        m_equipment=Equipment();
    }

    bool placed=false;
    for (int row=1;row<MAP_ROWS-1&&!placed;++row)
        for (int col=1;col<MAP_COLS-1&&!placed;++col)
            if (m_tileMap.getTile(col,row)==TileType::StairsUp)
            { delete m_player; m_player=new Player(col,row,TILE_SIZE); placed=true; }

    if (!placed)
        for (int row=1;row<MAP_ROWS-1&&!m_player;++row)
            for (int col=1;col<MAP_COLS-1&&!m_player;++col)
                if (m_tileMap.isWalkable(col,row))
                    m_player=new Player(col,row,TILE_SIZE);

    clearEnemies(); spawnEnemies(8+m_dungeonFloor);
    m_mapItems.clear(); spawnItems();
    m_respawnTimer=0;

    if (m_player)
    {
        Stats& s      = m_player->getStats();
        s.maxMentality= computeMentality();
        s.mentality   = s.maxMentality;
        s.body        = computeBody();
        s.itemLevel   = getItemLevelTotal();
        s.battleIndex = computeBattleIndex();
        m_fog.compute(m_player->getCol(),m_player->getRow(),VIEW_RADIUS,m_tileMap);
        m_coreSlots.setSlotCount(m_player->getStats().level);
        recalcAP();
    }

    updateCamera();
    m_selectedSlot=0; m_viewEquipment=false; m_viewCores=false;

    if (keepPlayer)
        addLog("> Floor "+std::to_string(m_dungeonFloor)+" - Deeper...", sf::Color(180,140,60));
    else
        addLog("> Floor 1 | 1-9=Skills [E]=Equip [Tab]=Inv", sf::Color(100,220,100));
}

void Game::updateCamera()
{
    if (!m_player) return;
    float px=(float)(m_player->getCol()*TILE_SIZE)+TILE_SIZE/2.f;
    float py=(float)(m_player->getRow()*TILE_SIZE)+TILE_SIZE/2.f;
    float hw=m_gameView.getSize().x/2.f,hh=m_gameView.getSize().y/2.f;
    px=std::clamp(px,hw,(float)(MAP_COLS*TILE_SIZE)-hw);
    py=std::clamp(py,hh,(float)(MAP_ROWS*TILE_SIZE)-hh);
    m_gameView.setCenter({px,py});
}

// ============================================================
//  Enemy Spawn
// ============================================================
void Game::spawnEnemies(int count){ for(int i=0;i<count;++i) spawnEnemy(m_dungeonFloor); }

void Game::spawnEnemy(int floor)
{
    for (int att=0;att<100;++att)
    {
        int col=1+std::rand()%(MAP_COLS-2),row=1+std::rand()%(MAP_ROWS-2);
        if (!m_tileMap.isWalkable(col,row)) continue;
        if (m_player){int dx=col-m_player->getCol(),dy=row-m_player->getRow();if(dx*dx+dy*dy<36)continue;}
        int r=std::rand()%100;
        std::string rankStr = r<5?"Boss":r<30?"Elite":"Normal";
        auto ids = MonsterDB::instance().getByRank(rankStr);
        if (ids.empty()) return;
        m_enemies.push_back(new Enemy(ids[std::rand()%ids.size()],col,row,TILE_SIZE,floor));
        return;
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
    int count=10+std::rand()%8,att=0;
    while ((int)m_mapItems.size()<count&&att<300)
    {
        att++;
        int col=1+std::rand()%(MAP_COLS-2),row=1+std::rand()%(MAP_ROWS-2);
        if (!m_tileMap.isWalkable(col,row)) continue;
        Item item;
        std::optional<std::string> itemId;
        int r=std::rand()%10;
        if (r<3)       itemId=DropTable::instance().getRandomItemIdByType("Food");
        else if (r<5)  itemId=DropTable::instance().getRandomItemIdByType("Potion");
        else if (r<7)  itemId=DropTable::instance().getRandomItemIdByType("Weapon");
        else if (r<8)  itemId=DropTable::instance().getRandomItemIdByType("OffWeapon");
        else {
            const char* armorTypes[]={"Helmet","BodyArmor","Gloves","Greaves","Boots"};
            itemId=DropTable::instance().getRandomItemIdByType(armorTypes[std::rand()%5]);
        }
        if (itemId)
        {
            const ItemData* idata=DropTable::instance().getItem(*itemId);
            if (idata)
            {
                item.id = idata->id;
                item.type = idata->type=="Core"?ItemType::Core:idata->type=="Material"?ItemType::Material:
                            idata->type=="Food"?ItemType::Food:idata->type=="Potion"?ItemType::Potion:
                            idata->type=="Helmet"?ItemType::Helmet:idata->type=="BodyArmor"?ItemType::BodyArmor:
                            idata->type=="Gloves"?ItemType::Gloves:idata->type=="Greaves"?ItemType::Greaves:
                            idata->type=="Boots"?ItemType::Boots:idata->type=="Weapon"?ItemType::Weapon:
                            idata->type=="OffWeapon"?ItemType::OffWeapon:ItemType::Material;
                item.name=idata->name; item.desc=idata->desc; item.value=idata->value;
                item.hpBonus=idata->hpBonus; item.atkBonus=idata->atkBonus;
                item.defBonus=idata->defBonus; item.dodgeBonus=idata->dodgeBonus;
                item.manaBonus=idata->manaBonus; item.magicDmgBonus=idata->magicDmgBonus;
                item.magicResBonus=idata->magicResBonus;
                item.spriteName=idata->sprite; item.stackable=idata->stackable;
            }
        }
        if (item.name.empty())
        {
            if(r<3) item=Item::makeFood(); else if(r<5) item=Item::makePotion();
            else if(r<7) item=Item::makeWeapon(); else if(r<8) item=Item::makeOffWeapon();
            else item=Item::makeArmor();
        }
        item.col=col; item.row=row;
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
    if (m_levelUpFlash){m_levelUpTimer--;if(m_levelUpTimer<=0)m_levelUpFlash=false;}
}

// ============================================================
//  Input
// ============================================================
void Game::processEvents()
{
    while (const std::optional event = m_window.pollEvent())
    {
        if (event->is<sf::Event::Closed>()) m_window.close();
        if (const auto* key = event->getIf<sf::Event::KeyPressed>())
        {
            if (m_playerDead)
            {
                if (key->code==sf::Keyboard::Key::R)      newDungeon(false);
                if (key->code==sf::Keyboard::Key::Escape) m_window.close();
                return;
            }

            if (m_targetingMode)
            {
                switch(key->code)
                {
                    case sf::Keyboard::Key::Up:    case sf::Keyboard::Key::K: moveTargetCursor(0,-1);  break;
                    case sf::Keyboard::Key::Down:  case sf::Keyboard::Key::J: moveTargetCursor(0, 1);  break;
                    case sf::Keyboard::Key::Left:  case sf::Keyboard::Key::H: moveTargetCursor(-1, 0); break;
                    case sf::Keyboard::Key::Right: case sf::Keyboard::Key::L: moveTargetCursor(1, 0);  break;
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
                case sf::Keyboard::Key::Up:
                case sf::Keyboard::Key::K:  handlePlayerMove(0,-1);  break;
                case sf::Keyboard::Key::Down:
                case sf::Keyboard::Key::J:  handlePlayerMove(0, 1);  break;
                case sf::Keyboard::Key::Left:
                case sf::Keyboard::Key::H:  handlePlayerMove(-1, 0); break;
                case sf::Keyboard::Key::Right:
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

                case sf::Keyboard::Key::R:  newDungeon(false);        break;
                case sf::Keyboard::Key::G:  tryPickupItem();          break;
                case sf::Keyboard::Key::D:  dropSelectedItem();       break;
                case sf::Keyboard::Key::E:
                    m_viewEquipment=!m_viewEquipment; m_viewCores=false;
                    addLog(m_viewEquipment?"> Equipment view":"> Inventory view",sf::Color(160,160,160));
                    break;
                case sf::Keyboard::Key::C:
                    m_viewCores=!m_viewCores; m_viewEquipment=false;
                    addLog(m_viewCores?"> Core Slots view":"> Inventory view",sf::Color(100,200,255));
                    break;
                case sf::Keyboard::Key::LBracket:
                    if (m_viewCores&&m_selectedCoreSlot>0) m_selectedCoreSlot--; break;
                case sf::Keyboard::Key::RBracket:
                    if (m_viewCores&&m_selectedCoreSlot<m_coreSlots.getSlotCount()-1) m_selectedCoreSlot++; break;
                case sf::Keyboard::Key::X:
                    if (m_viewCores) unequipCore(); break;
                case sf::Keyboard::Key::Period: tryDescendStairs(); break;
                case sf::Keyboard::Key::Comma:  tryAscendStairs();  break;
                case sf::Keyboard::Key::Space:  waitTurn();         break;
                case sf::Keyboard::Key::Enter:
                    if(m_viewCores)equipCore(); else if(!m_viewEquipment)useOrEquipSelected(); break;
                case sf::Keyboard::Key::Tab:
                    m_selectedSlot=(m_selectedSlot+1)%MAX_INVENTORY; break;
                default: break;
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
    if (m_tileMap.getTile(m_player->getCol(),m_player->getRow())!=TileType::StairsDown)
    {addLog("> No stairs here.");return;}
    m_dungeonFloor++;
    Stats saved=m_player->getStats();
    auto savedSkills=m_player->getSkills();
    auto savedHotbar = std::vector<std::string>();
    for (int i=0;i<9;++i) savedHotbar.push_back(m_player->getHotbar(i));
    newDungeon(true);
    if (m_player)
    {
        m_player->getStats()=saved;
        m_player->getSkills()=savedSkills;
        for (int i=0;i<9;++i) m_player->setHotbar(i,savedHotbar[i]);
    }
    addLog("> Descended to floor "+std::to_string(m_dungeonFloor)+"!",sf::Color(255,220,50));
}

void Game::tryAscendStairs()
{
    if (!m_player) return;
    if (m_tileMap.getTile(m_player->getCol(),m_player->getRow())!=TileType::StairsUp)
    {addLog("> No stairs up here.");return;}
    if (m_dungeonFloor<=1){addLog("> Already on floor 1.");return;}
    m_dungeonFloor--;
    Stats saved=m_player->getStats();
    auto savedSkills=m_player->getSkills();
    auto savedHotbar = std::vector<std::string>();
    for (int i=0;i<9;++i) savedHotbar.push_back(m_player->getHotbar(i));
    newDungeon(true);
    if (m_player)
    {
        m_player->getStats()=saved;
        m_player->getSkills()=savedSkills;
        for (int i=0;i<9;++i) m_player->setHotbar(i,savedHotbar[i]);
    }
    addLog("> Ascended to floor "+std::to_string(m_dungeonFloor)+"!",sf::Color(100,220,255));
}

void Game::waitTurn()
{
    if (!m_player) return;
    m_turnCount++; m_player->onTurnPassed();
    processTurn(); tryRespawnEnemies();
    m_fog.compute(m_player->getCol(),m_player->getRow(),VIEW_RADIUS,m_tileMap);
    addLog("> You wait.",sf::Color(140,140,140));
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
            if (m_inventory.isFull()){addLog("> Inventory full!",sf::Color(220,120,50));return;}
            Item item=m_mapItems[i]; item.col=-1; item.row=-1;
            m_inventory.addItem(item);
            m_mapItems.erase(m_mapItems.begin()+i);
            addLog("> Picked up: "+item.name,sf::Color(180,220,100));
            return;
        }
    addLog("> Nothing here.");
}

void Game::useOrEquipSelected()
{
    Item* item=m_inventory.getItem(m_selectedSlot);
    if (!item||!m_player) return;
    Stats& s=m_player->getStats();
    if (item->isUsable())
    {
        if(item->type==ItemType::Food)
        {s.hunger=std::min(100,s.hunger+item->value);
         addLog("> Ate "+item->name+". Hunger +"+std::to_string(item->value),sf::Color(200,180,80));}
        else
        {s.hp=std::min(s.maxHp,s.hp+item->value);
         if(s.hp>0&&s.hpDepleted){s.hpDepleted=false;addLog("> HP recovered",sf::Color(100,220,180));}
         addLog("> Used "+item->name+". HP +"+std::to_string(item->value),sf::Color(80,180,220));}
        m_inventory.removeItem(m_selectedSlot);
    }
    else if (item->isCore()) equipCore();
    else if (item->isMaterial()) addLog("> "+item->name+" is a trade item.",sf::Color(160,160,160));
    else if (item->isEquipment())
    {
        EquipSlot slot=Equipment::itemToSlot(*item);
        if (m_equipment.hasItem(slot))
        {
            Item old=m_equipment.unequip(slot);
            if (!m_inventory.isFull()) m_inventory.addItem(old);
            else{addLog("> Inventory full!",sf::Color(220,120,50));return;}
        }
        Item toEquip=*item;
        m_inventory.removeItem(m_selectedSlot);
        m_equipment.equip(toEquip,slot);
        addLog("> Equipped "+toEquip.name+" ["+Equipment::slotName(slot)+"] +"+
               std::to_string(toEquip.value),sf::Color(180,220,180));
        m_player->getStats().itemLevel=getItemLevelTotal();
    }
}

void Game::equipCore()
{
    Item* item=m_inventory.getItem(m_selectedSlot);
    if (!item||!item->isCore()){addLog("> Not a core item!");return;}
    int freeSlot=-1;
    for (int i=0;i<m_coreSlots.getSlotCount();++i)
        if (!m_coreSlots.hasCore(i)){freeSlot=i;break;}
    if (freeSlot==-1){addLog("> All core slots full!",sf::Color(220,120,50));return;}
    Item core=*item;
    m_inventory.removeItem(m_selectedSlot);
    m_coreSlots.equip(freeSlot,core);
    addLog("> Equipped "+core.name+" in core slot "+std::to_string(freeSlot+1),sf::Color(100,200,255));

    const ItemData* idata = DropTable::instance().getItem(core.id);
    if (idata)
        for (const auto& skillId : idata->coreSkills)
        {
            if (m_player->addCoreSkill(skillId))
                addLog("> Skill unlocked: "+skillId, sf::Color(100,220,255));
            else
                addLog("> Hotbar full — "+skillId+" not assigned", sf::Color(180,120,50));
        }
    m_player->getStats().ability = (int)m_player->getSkills().size();
}

void Game::unequipCore()
{
    if (!m_coreSlots.hasCore(m_selectedCoreSlot))
    {addLog("> No core in slot "+std::to_string(m_selectedCoreSlot+1));return;}
    if (m_inventory.isFull()){addLog("> Inventory full!");return;}
    Item core=m_coreSlots.unequip(m_selectedCoreSlot);
    m_inventory.addItem(core);
    addLog("> Removed "+core.name+" from core slot "+std::to_string(m_selectedCoreSlot+1),sf::Color(160,160,160));

    const ItemData* idata = DropTable::instance().getItem(core.id);
    if (idata)
        for (const auto& skillId : idata->coreSkills)
        {
            m_player->removeCoreSkill(skillId);
            addLog("> Skill lost: "+skillId, sf::Color(160,100,100));
        }
    m_player->getStats().ability = (int)m_player->getSkills().size();
}

void Game::dropSelectedItem()
{
    Item* item=m_inventory.getItem(m_selectedSlot);
    if (!item||!m_player) return;
    Item d=*item; d.col=m_player->getCol(); d.row=m_player->getRow();
    m_mapItems.push_back(d);
    addLog("> Dropped: "+d.name,sf::Color(160,160,160));
    m_inventory.removeItem(m_selectedSlot);
}

// ============================================================
//  Movement & Combat
// ============================================================
void Game::handlePlayerMove(int dc, int dr)
{
    if (!m_player) return;

    if (m_player->getStats().currentAP <= 0) return;

    int tc=m_player->getCol()+dc, tr=m_player->getRow()+dr;

    for (auto*e:m_enemies)
        if (!e->isDead()&&e->getCol()==tc&&e->getRow()==tr)
        {
            playerAttack(e);
            if (m_player->getStats().currentAP <= 0)
            {
                processTurn(); tryRespawnEnemies(); recalcAP();
            }
            m_fog.compute(m_player->getCol(),m_player->getRow(),VIEW_RADIUS,m_tileMap);
            updateCamera(); return;
        }

    bool moved=m_player->tryMove(dc,dr,m_tileMap);
    if (!moved){addLog("> Blocked!");return;}

    m_player->getStats().currentAP--;

    m_turnCount++; m_player->onTurnPassed();
    m_fog.compute(m_player->getCol(),m_player->getRow(),VIEW_RADIUS,m_tileMap);

    if (m_player->getStats().currentAP <= 0)
    {
        processTurn(); tryRespawnEnemies(); recalcAP();
    }
    updateCamera();

    int pc=m_player->getCol(),pr=m_player->getRow();
    for (auto& it:m_mapItems)
        if (it.col==pc&&it.row==pr)
        {addLog("> "+it.name+" here. [G] pick up.",sf::Color(200,200,80));break;}

    TileType tile=m_tileMap.getTile(pc,pr);
    if (tile==TileType::StairsDown) addLog("> Stairs down! [.] descend.",sf::Color(255,220,50));
    if (tile==TileType::StairsUp)   addLog("> Stairs up! [,] ascend.",  sf::Color(100,220,255));

    int hunger=m_player->getStats().hunger;
    if(hunger==50) addLog("> You feel hungry.",      sf::Color(220,180,50));
    if(hunger==20) addLog("> Very hungry!",          sf::Color(220,100,50));
    if(hunger== 0) addLog("> Starving! HP draining!",sf::Color(220,50,50));
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

    std::string msg="> You hit "+enemy->getName()+" for "+std::to_string(dmg)+"!";
    if (powered) msg+=" [POWERED]";
    addLog(msg,sf::Color(255,200,50));

    Stats& s = m_player->getStats();
    s.atkAPAccum += s.atkSpeedCost;
    while (s.atkAPAccum >= 100)
    {
        s.atkAPAccum -= 100;
        s.currentAP--;
    }

    if (enemy->isDead())
    {
        addLog("> "+enemy->getName()+" dead! +"+std::to_string(enemy->getExp())+" EXP",sf::Color(220,80,80));
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
            drop.magicResBonus=idata->magicResBonus;
            drop.spriteName=idata->sprite; drop.stackable=idata->stackable;
            drop.col=enemy->getCol(); drop.row=enemy->getRow();
            m_mapItems.push_back(drop);
            addLog("> Dropped: "+idata->name,sf::Color(180,220,255));
        }
        if (m_player->addExp(enemy->getExp()))
        {
            m_levelUpFlash=true; m_levelUpTimer=120;
            int lv=m_player->getStats().level;
            m_coreSlots.setSlotCount(lv);
            addLog("> *** LEVEL UP! Level "+std::to_string(lv)+" ***",sf::Color(255,255,50));
        }
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
    {addLog("> You dodged "+enemy->getName()+"!",sf::Color(150,220,150));return;}
    int dmg=std::max(1,enemy->getAttack()-def+std::rand()%3);
    s.hp-=dmg;
    addLog("> "+enemy->getName()+" hits you for "+std::to_string(dmg)+"!",sf::Color(220,80,80));
    if (s.hp<=0)
    {
        s.hp=0;
        if (!s.hpDepleted)
        {
            s.hpDepleted=true;
            addLog("> WARNING: HP depleted - Mentality drain starts!",sf::Color(255,120,50));
            addLog("> Mentality -3/turn until recovered",sf::Color(220,100,200));
        }
    }
}

// ============================================================
//  Process Turn
// ============================================================
void Game::processTurn()
{
    if (!m_player) return;
    Stats& s=m_player->getStats();
    s.body         = computeBody();
    s.itemLevel    = getItemLevelTotal();
    s.battleIndex  = computeBattleIndex();
    s.maxMentality = computeMentality();
    s.ability      = (int)m_player->getSkills().size();
    drainMentality();
    if (m_playerDead) return;
    for (auto*e:m_enemies)
    {
        if (e->isDead()) continue;
        if (!m_fog.isVisible(e->getCol(),e->getRow())) continue;
        int dx=std::abs(e->getCol()-m_player->getCol());
        int dy=std::abs(e->getRow()-m_player->getRow());
        if (dx<=1&&dy<=1&&(dx+dy)>0) enemyAttack(e);
        else e->updateAI(m_player->getCol(),m_player->getRow(),m_tileMap,m_enemies);
    }
    m_enemies.erase(std::remove_if(m_enemies.begin(),m_enemies.end(),
        [](Enemy*e){bool d=e->isDead();if(d)delete e;return d;}),m_enemies.end());
}

void Game::addLog(const std::string& msg, sf::Color color)
{
    m_log.push_back({msg,color});
    if((int)m_log.size()>LOG_MAX_LINES) m_log.erase(m_log.begin());
}

// ============================================================
//  Render
// ============================================================
void Game::render()
{
    m_window.clear(sf::Color(0,0,0));
    m_window.setView(m_gameView);
    m_tileMap.render(m_window);
    renderItems();
    for (auto*e:m_enemies)
        if (m_fog.isVisible(e->getCol(),e->getRow())) e->render(m_window);
    if (m_player) m_player->render(m_window);
    m_fog.render(m_window,TILE_SIZE);

    if (m_targetingMode) renderTargeting();

    m_window.setView(m_uiView);
    renderStatusPanel();
    renderRightPanel();
    renderLogPanel();
    renderHotbar();
    if (m_levelUpFlash) renderLevelUpEffect();
    m_window.display();
}

// ============================================================
//  Hotbar Render
// ============================================================
void Game::renderHotbar()
{
    if (!m_player || !m_fontLoaded) return;

    const float SZ  = 32.f;
    const float GAP = 4.f;
    const int   N   = 9;
    float totalW = N * (SZ + GAP) - GAP;
    float startX = ((float)GAME_VIEW_W - totalW) / 2.f;
    float startY = (float)GAME_VIEW_H - SZ - 8.f;

    for (int i = 0; i < N; ++i)
    {
        float sx = startX + i * (SZ + GAP);
        const std::string& id = m_player->getHotbar(i);
        const SkillInstance* sk = id.empty() ? nullptr : m_player->findSkill(id);

        sf::RectangleShape slot({SZ, SZ});
        slot.setFillColor(sf::Color(10, 10, 10, 200));
        slot.setOutlineColor(sf::Color(80, 70, 50));
        slot.setOutlineThickness(1.5f);
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
    if (!m_player) return;

    int pc = m_player->getCol();
    int pr = m_player->getRow();
    float ts = (float)TILE_SIZE;

    SkillInstance* sk = m_player->findSkill(m_targetSkillId);
    int maxRange = sk ? sk->data.effect.range : 6;

    auto line = getLine(pc, pr, m_targetCol, m_targetRow);

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
        float cpx = m_targetCol * ts;
        float cpy = m_targetRow * ts;

        int ddx = m_targetCol-pc, ddy = m_targetRow-pr;
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
                if (!e->isDead() && e->getCol()==m_targetCol && e->getRow()==m_targetRow)
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
    float alpha=std::min(180.f,(float)m_levelUpTimer*1.5f);
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
void Game::renderStatusPanel()
{
    float px=(float)(WINDOW_W-RIGHT_PANEL_W);
    sf::RectangleShape panel({(float)RIGHT_PANEL_W,(float)STATUS_PANEL_H});
    panel.setFillColor(sf::Color(8,8,8));
    panel.setPosition({px,0.f});
    panel.setOutlineColor(sf::Color(80,60,40));
    panel.setOutlineThickness(1.f);
    m_window.draw(panel);

    if (!m_fontLoaded||!m_player) return;
    const Stats& s=m_player->getStats();
    float x=px+15.f, y=(float)STATUS_PANEL_TOP_PADDING;

    auto drawLine=[&](const std::string& label,const std::string& value,
                      sf::Color color=sf::Color::White,int size=STATUS_LINE_SIZE)
    {
        sf::Text t(m_font,label+" "+value,(unsigned int)size);
        t.setFillColor(color); t.setPosition({x,y}); m_window.draw(t);
        y+=size+STATUS_LINE_SPACING;
    };

    {sf::Text h(m_font,"player 1",STATUS_HEADER_SIZE);
     h.setPosition({x,y});m_window.draw(h);
     y+=STATUS_HEADER_SIZE+STATUS_HEADER_SPACING;}

    drawLine("level:",     std::to_string(s.level));
    drawLine("body:",      std::to_string(s.body));
    drawLine("mentality:", std::to_string(s.maxMentality));
    drawLine("ability:",   std::to_string(s.ability));
    drawLine("item level:",std::to_string(s.itemLevel));
    drawLine("Battle Index:",std::to_string(s.battleIndex));

    std::string apStr = "AP: "+std::to_string(s.currentAP)+"/"+std::to_string(s.maxAP);
    sf::Color apColor = s.currentAP > 0 ? sf::Color(100,200,255) : sf::Color(100,100,100);
    drawLine("", apStr, apColor);

    renderSkillPanel();
}

// ============================================================
//  Right Panel
// ============================================================
void Game::renderRightPanel()
{
    float panelX=(float)(WINDOW_W-RIGHT_PANEL_W);
    float panelY=(float)STATUS_PANEL_H;

    sf::RectangleShape panel({(float)RIGHT_PANEL_W,(float)INV_GRID_H});
    panel.setFillColor(sf::Color(8,8,8));
    panel.setPosition({panelX,panelY});
    panel.setOutlineColor(sf::Color(80,60,40));
    panel.setOutlineThickness(0.f);
    m_window.draw(panel);
    sf::Vertex leftLine[2];
    leftLine[0].position = sf::Vector2f(panelX, panelY);
    leftLine[0].color = sf::Color(80,60,40);
    leftLine[1].position = sf::Vector2f(panelX, panelY + 250.f);
    leftLine[1].color = sf::Color(80,60,40);
    m_window.draw(leftLine, 2, sf::PrimitiveType::Lines);
    if (!m_fontLoaded) return;

    if (!m_viewEquipment&&!m_viewCores)
    {
        const int COLS=5,ROWS=4;
        const float SZ=35.f,GAP=4.f;
        float sx0=panelX+(RIGHT_PANEL_W-(COLS*(SZ+GAP)-GAP))/2.f;
        float sy0=panelY+10.f;

        for (int row=0;row<ROWS;++row)
        for (int col=0;col<COLS;++col)
        {
            int idx=row*COLS+col;
            float sx=sx0+col*(SZ+GAP),sy=sy0+row*(SZ+GAP);
            bool sel=(idx==m_selectedSlot);
            sf::RectangleShape slot({SZ,SZ});
            slot.setFillColor(sel?sf::Color(60,50,30):sf::Color(8,8,8));
            slot.setOutlineColor(sel?sf::Color(200,160,60):sf::Color(120,120,120));
            slot.setOutlineThickness(1.5f);
            slot.setPosition({sx,sy}); m_window.draw(slot);
            Item* item=m_inventory.getItem(idx);
            if (item)
            {
                auto& tm=TextureManager::instance();
                std::string path=item->spritePath();
                std::string name=path.substr(path.rfind('/')+1);
                name=name.substr(0,name.rfind('.'));
                const sf::Texture* tex=tm.get(name);
                if (tex)
                {
                    sf::Sprite spr(*tex);
                    auto sz=tex->getSize();
                    float sc=SZ*0.8f/std::max((float)sz.x,(float)sz.y);
                    spr.setScale({sc,sc});
                    spr.setPosition({sx+SZ*0.1f,sy+SZ*0.1f}); m_window.draw(spr);
                }
                else
                {sf::CircleShape dot(SZ*0.28f);dot.setFillColor(item->color());dot.setPosition({sx+SZ*0.22f,sy+SZ*0.22f});m_window.draw(dot);}
                int cnt=m_inventory.getCount(idx);
                if (cnt>1){sf::Text ct(m_font,"x"+std::to_string(cnt),7);ct.setFillColor(sf::Color(255,220,100));ct.setPosition({sx+SZ-14.f,sy+SZ-10.f});m_window.draw(ct);}
            }
        }
        Item* sel=m_inventory.getItem(m_selectedSlot);
        std::string info="(empty)";
        if (sel){int cnt=m_inventory.getCount(m_selectedSlot);info=sel->name+" ("+std::to_string(sel->value)+")";if(cnt>1)info+=" x"+std::to_string(cnt);}
        sf::Text infoTxt(m_font,info,9);
        infoTxt.setFillColor(sf::Color(180,160,100));
        infoTxt.setPosition({panelX+8.f,sy0+ROWS*(SZ+GAP)+4.f}); m_window.draw(infoTxt);
    }
    else if (m_viewCores)
    {
        m_coreSlots.render(m_window,m_font,panelX+8.f,panelY+20.f,(float)RIGHT_PANEL_W,m_selectedCoreSlot,true);
        CoreStats cb=m_coreSlots.getTotalBonus();
        float hy=panelY+INV_GRID_H-55.f;
        auto dh=[&](const std::string& t,sf::Color c=sf::Color(70,70,70))
        {sf::Text tx(m_font,t,8);tx.setFillColor(c);tx.setPosition({panelX+8.f,hy});m_window.draw(tx);hy+=12.f;};
        dh("Bonus: HP+"+std::to_string(cb.hp)+" ATK+"+std::to_string(cb.atk)+" DEF+"+std::to_string(cb.def)+" DOD+"+std::to_string(cb.dodge),sf::Color(100,200,255));
        dh("[C] Close  [1-9] Equip core");
        dh("[X] Remove core");
    }
    else if (m_viewEquipment)
    {
        sf::Text title(m_font,"EQUIPMENT  [E]=Bag",9);
        title.setFillColor(sf::Color(120,160,120));
        title.setPosition({panelX+8.f,panelY+6.f}); m_window.draw(title);
        const EquipSlot slots[]={EquipSlot::Head,EquipSlot::Body,EquipSlot::Arms,
                                  EquipSlot::Legs,EquipSlot::Feet,EquipSlot::MainHand,EquipSlot::OffHand};
        const float SZH=18.f,GAP=3.f;
        float sx=panelX+8.f,sy=panelY+20.f;
        for (int i=0;i<7;++i)
        {
            const Item* item=m_equipment.getItem(slots[i]);
            sf::Text label(m_font,Equipment::slotName(slots[i])+":",9);
            label.setFillColor(sf::Color(140,120,80));label.setPosition({sx,sy});m_window.draw(label);
            sf::Text itemTxt(m_font,item?item->name:"--",9);
            itemTxt.setFillColor(item?item->color():sf::Color(60,60,60));
            itemTxt.setPosition({sx+44.f,sy});m_window.draw(itemTxt);
            if (item){sf::Text val(m_font,"+"+std::to_string(item->value),9);val.setFillColor(sf::Color(180,200,100));val.setPosition({sx+160.f,sy});m_window.draw(val);}
            sy+=SZH+GAP;
        }
    }
}

void Game::renderLogPanel()
{
    float panelY=(float)(WINDOW_H-LOG_PANEL_H);
    sf::RectangleShape panel({(float)WINDOW_W,(float)LOG_PANEL_H});
    panel.setFillColor(sf::Color(5,5,5));
    panel.setPosition({0.f,panelY});
    panel.setOutlineColor(sf::Color(80,60,40));
    panel.setOutlineThickness(2.f);
    m_window.draw(panel);
    if (!m_fontLoaded) return;
    float y=panelY+8.f;
    for (const auto& entry:m_log)
    {
        sf::Text t(m_font,entry.text,10);
        t.setFillColor(entry.color);
        t.setPosition({8.f,y}); m_window.draw(t);
        y+=17.f;
    }
}
