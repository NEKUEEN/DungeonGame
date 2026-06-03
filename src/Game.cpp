#include "Game.hpp"
#include "TextureManager.hpp"
#include "MonsterDB.hpp"
#include "DropTable.hpp"
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <optional>

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

    m_gameView.setSize({(float)GAME_VIEW_W*1.25f,(float)GAME_VIEW_H*1.25f});
    m_gameView.setViewport(sf::FloatRect({0.f,0.f},
        {(float)GAME_VIEW_W/WINDOW_W,(float)GAME_VIEW_H/WINDOW_H}));
    m_uiView.setSize({(float)WINDOW_W,(float)WINDOW_H});
    m_uiView.setCenter({(float)WINDOW_W/2.f,(float)WINDOW_H/2.f});

    newDungeon(false);
}

Game::~Game() { delete m_player; clearEnemies(); }

// ============================================================
//  Stats Helpers (NEW)
// ============================================================
int Game::computeBody() const
{
    if (!m_player) return 0;
    const Stats& s = m_player->getStats();
    CoreStats cb   = m_coreSlots.getTotalBonus();

    // base + bonus จาก core/equipment
    int hp    = s.maxHp    + cb.body + m_equipment.getTotalHpBonus(); // core body + equipment hp
    int atk   = s.maxAtk   + m_equipment.getTotalAtkBonus();
    int def   = s.maxDef   + m_equipment.getTotalDefBonus();
    int dodge = s.maxDodge + m_equipment.getTotalDodgeBonus();

    return (int)(hp*0.4f + atk*0.3f + def*0.2f + dodge*0.1f);
}

int Game::getItemLevelTotal() const
{
    // รวม value ของ equipment ทุกชิ้นที่ใส่อยู่ (ใช้ item.value เป็น grade ไปก่อน)
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
    int mentality = s.maxMentality + cb.mentality;
    int itemLv    = getItemLevelTotal();

    // bonus% = 30% + (level-1)*5%  →  lv1=30%, lv2=35%, lv3=40% ...
    float bonusPct = 0.30f + (s.level - 1) * 0.05f;

    return (int)(body + mentality + itemLv * bonusPct);
}

void Game::drainMentality()
{
    if (!m_player) return;
    Stats& s = m_player->getStats();
    if (!s.hpDepleted) return;
    if (m_playerDead) return;

    s.mentality -= 3;
    if (s.mentality < 0) s.mentality = 0;

    addLog("> Mentality -3/turn (" +
           std::to_string(s.mentality) + "/" + std::to_string(s.maxMentality) + ")",
           sf::Color(180, 80, 220));

    if (s.mentality <= 0)
    {
        m_playerDead = true;
        addLog(" *** Mind shattered   DEAD *** [R] restart",
               sf::Color(255, 50, 50));
    }
}

// ============================================================
//  Dungeon Setup
// ============================================================
void Game::newDungeon(bool keepPlayer)
{
    m_tileMap.generate();
    m_fog.reset();
    m_playerDead = false;

    if (!keepPlayer)
    {
        delete m_player; m_player=nullptr;
        m_dungeonFloor=1;
        m_inventory=Inventory();
        m_equipment=Equipment();
    }

    // วาง player ที่ StairsUp
    bool placed=false;
    for (int row=1;row<MAP_ROWS-1&&!placed;++row)
        for (int col=1;col<MAP_COLS-1&&!placed;++col)
            if (m_tileMap.getTile(col,row)==TileType::StairsUp)
            {
                delete m_player;
                m_player=new Player(col,row,TILE_SIZE);
                placed=true;
            }

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
        // sync stats ทันทีหลัง spawn
        Stats& s      = m_player->getStats();
        s.body        = computeBody();
        s.itemLevel   = getItemLevelTotal();
        s.battleIndex = computeBattleIndex();

        m_fog.compute(m_player->getCol(),m_player->getRow(),VIEW_RADIUS,m_tileMap);
        m_coreSlots.setSlotCount(m_player->getStats().level);
    }

    updateCamera();
    m_selectedSlot=0; m_viewEquipment=false; m_viewCores=false;

    if (keepPlayer)
        addLog("> Floor "+std::to_string(m_dungeonFloor)+" — Deeper...", sf::Color(180,140,60));
    else
        addLog("> Floor 1 — [E]=Equipment [Tab]=Inventory", sf::Color(100,220,100));
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
void Game::spawnEnemies(int count) { for(int i=0;i<count;++i) spawnEnemy(m_dungeonFloor); }

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
        std::string monsterId = ids[std::rand() % ids.size()];
        m_enemies.push_back(new Enemy(monsterId, col, row, TILE_SIZE, floor));
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
        if (r<3)       itemId = DropTable::instance().getRandomItemIdByType("Food");
        else if (r<5)  itemId = DropTable::instance().getRandomItemIdByType("Potion");
        else if (r<7)  itemId = DropTable::instance().getRandomItemIdByType("Weapon");
        else if (r<8)  itemId = DropTable::instance().getRandomItemIdByType("OffWeapon");
        else {
            const char* armorTypes[] = {"Helmet","BodyArmor","Gloves","Greaves","Boots"};
            itemId = DropTable::instance().getRandomItemIdByType(armorTypes[std::rand()%5]);
        }

        if (itemId)
        {
            const ItemData* idata = DropTable::instance().getItem(*itemId);
            if (idata)
            {
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
                            idata->type=="OffWeapon" ? ItemType::OffWeapon : ItemType::Material;
                item.name       = idata->name;
                item.desc       = idata->desc;
                item.value      = idata->value;
                item.hpBonus    = idata->hpBonus;
                item.atkBonus   = idata->atkBonus;
                item.defBonus   = idata->defBonus;
                item.dodgeBonus = idata->dodgeBonus;
                item.spriteName = idata->sprite;
                item.stackable  = idata->stackable;
            }
        }

        if (item.name.empty())
        {
            if(r<3)      item=Item::makeFood();
            else if(r<5) item=Item::makePotion();
            else if(r<7) item=Item::makeWeapon();
            else if(r<8) item=Item::makeOffWeapon();
            else         item=Item::makeArmor();
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
            // ถ้าตายแล้ว รับแค่ R กับ Escape
            if (m_playerDead)
            {
                if (key->code == sf::Keyboard::Key::R)  newDungeon(false);
                if (key->code == sf::Keyboard::Key::Escape) m_window.close();
                return;
            }

            switch(key->code)
            {
                case sf::Keyboard::Key::Escape: m_window.close();        break;
                case sf::Keyboard::Key::Up:
                case sf::Keyboard::Key::K:  handlePlayerMove(0,-1);      break;
                case sf::Keyboard::Key::Down:
                case sf::Keyboard::Key::J:  handlePlayerMove(0, 1);      break;
                case sf::Keyboard::Key::Left:
                case sf::Keyboard::Key::H:  handlePlayerMove(-1,0);      break;
                case sf::Keyboard::Key::Right:
                case sf::Keyboard::Key::L:  handlePlayerMove(1, 0);      break;
                case sf::Keyboard::Key::Y:  handlePlayerMove(-1,-1);     break;
                case sf::Keyboard::Key::U:  handlePlayerMove(1,-1);      break;
                case sf::Keyboard::Key::B:  handlePlayerMove(-1, 1);     break;
                case sf::Keyboard::Key::N:  handlePlayerMove(1,  1);     break;
                case sf::Keyboard::Key::R:  newDungeon(false);           break;
                case sf::Keyboard::Key::G:  tryPickupItem();             break;
                case sf::Keyboard::Key::D:  dropSelectedItem();          break;
                case sf::Keyboard::Key::E:
                    m_viewEquipment=!m_viewEquipment;
                    m_viewCores=false;
                    addLog(m_viewEquipment?"> Equipment view":"> Inventory view",
                           sf::Color(160,160,160));
                    break;
                case sf::Keyboard::Key::C:
                    m_viewCores=!m_viewCores;
                    m_viewEquipment=false;
                    addLog(m_viewCores?"> Core Slots view":"> Inventory view",
                           sf::Color(100,200,255));
                    break;
                case sf::Keyboard::Key::LBracket:
                    if (m_viewCores && m_selectedCoreSlot>0) m_selectedCoreSlot--;
                    break;
                case sf::Keyboard::Key::RBracket:
                    if (m_viewCores && m_selectedCoreSlot<m_coreSlots.getSlotCount()-1)
                        m_selectedCoreSlot++;
                    break;
                case sf::Keyboard::Key::X:
                    if (m_viewCores) unequipCore();
                    break;
                case sf::Keyboard::Key::Period: tryDescendStairs();      break;
                case sf::Keyboard::Key::Comma:  tryAscendStairs();       break;
                case sf::Keyboard::Key::Space:  waitTurn();              break;
                case sf::Keyboard::Key::Num1: m_selectedSlot=0; if(m_viewCores) equipCore(); else if(!m_viewEquipment) useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num2: m_selectedSlot=1; if(m_viewCores) equipCore(); else if(!m_viewEquipment) useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num3: m_selectedSlot=2; if(m_viewCores) equipCore(); else if(!m_viewEquipment) useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num4: m_selectedSlot=3; if(m_viewCores) equipCore(); else if(!m_viewEquipment) useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num5: m_selectedSlot=4; if(m_viewCores) equipCore(); else if(!m_viewEquipment) useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num6: m_selectedSlot=5; if(m_viewCores) equipCore(); else if(!m_viewEquipment) useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num7: m_selectedSlot=6; if(m_viewCores) equipCore(); else if(!m_viewEquipment) useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num8: m_selectedSlot=7; if(m_viewCores) equipCore(); else if(!m_viewEquipment) useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num9: m_selectedSlot=8; if(m_viewCores) equipCore(); else if(!m_viewEquipment) useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num0: m_selectedSlot=9; if(m_viewCores) equipCore(); else if(!m_viewEquipment) useOrEquipSelected(); break;
                case sf::Keyboard::Key::Tab:
                    m_selectedSlot=(m_selectedSlot+1)%MAX_INVENTORY; break;
                default: break;
            }
        }
    }
}

void Game::handleInventoryInput(sf::Keyboard::Key){}

// ============================================================
//  Stairs
// ============================================================
void Game::tryDescendStairs()
{
    if (!m_player) return;
    if (m_tileMap.getTile(m_player->getCol(),m_player->getRow())!=TileType::StairsDown)
    {addLog("> No stairs here.");return;}
    m_dungeonFloor++;
    Stats saved=m_player->getStats();
    newDungeon(true);
    if (m_player) m_player->getStats()=saved;
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
    newDungeon(true);
    if (m_player) m_player->getStats()=saved;
    addLog("> Ascended to floor "+std::to_string(m_dungeonFloor)+"!",sf::Color(100,220,255));
}

void Game::waitTurn()
{
    if (!m_player) return;
    m_turnCount++; m_player->onTurnPassed();
    processTurn(); tryRespawnEnemies();
    m_fog.compute(m_player->getCol(),m_player->getRow(),VIEW_RADIUS,m_tileMap);
    addLog("> You wait.", sf::Color(140,140,140));
}

// ============================================================
//  Inventory / Item actions
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
        {// Potion: ฟื้น hp และถ้า hp กลับมา > 0 ให้ clear hpDepleted
         s.hp=std::min(s.maxHp,s.hp+item->value);
         if (s.hp > 0 && s.hpDepleted)
         {
             s.hpDepleted = false;
             addLog("> HP ฟื้นแล้ว — ค่าจิตหยุดลด",sf::Color(100,220,180));
         }
         addLog("> Used "+item->name+". HP +"+std::to_string(item->value),sf::Color(80,180,220));}
        m_inventory.removeItem(m_selectedSlot);
    }
    else if (item->isCore())
    {
        equipCore();
    }
    else if (item->isMaterial())
    {
        addLog("> "+item->name+" is a trade item.",sf::Color(160,160,160));
    }
    else if (item->isEquipment())
    {
        EquipSlot slot=Equipment::itemToSlot(*item);
        if (m_equipment.hasItem(slot))
        {
            Item old=m_equipment.unequip(slot);
            if (!m_inventory.isFull()) m_inventory.addItem(old);
            else { addLog("> Inventory full!",sf::Color(220,120,50)); return; }
        }
        Item toEquip=*item;
        m_inventory.removeItem(m_selectedSlot);
        m_equipment.equip(toEquip,slot);
        addLog("> Equipped "+toEquip.name+" ["+Equipment::slotName(slot)+"] +"+
               std::to_string(toEquip.value),sf::Color(180,220,180));

        // sync item level ทันที
        m_player->getStats().itemLevel = getItemLevelTotal();
    }
}

void Game::equipCore()
{
    Item* item = m_inventory.getItem(m_selectedSlot);
    if (!item || !item->isCore())
    { addLog("> Not a core item!"); return; }

    int freeSlot = -1;
    for (int i=0; i<m_coreSlots.getSlotCount(); ++i)
        if (!m_coreSlots.hasCore(i)) { freeSlot=i; break; }

    if (freeSlot == -1)
    { addLog("> All core slots full!", sf::Color(220,120,50)); return; }

    Item core = *item;
    m_inventory.removeItem(m_selectedSlot);
    m_coreSlots.equip(freeSlot, core);
    addLog("> Equipped "+core.name+" in core slot "+std::to_string(freeSlot+1),
           sf::Color(100,200,255));
}

void Game::unequipCore()
{
    if (!m_coreSlots.hasCore(m_selectedCoreSlot))
    { addLog("> No core in slot "+std::to_string(m_selectedCoreSlot+1)); return; }
    if (m_inventory.isFull())
    { addLog("> Inventory full!"); return; }

    Item core = m_coreSlots.unequip(m_selectedCoreSlot);
    m_inventory.addItem(core);
    addLog("> Removed "+core.name+" from core slot "+std::to_string(m_selectedCoreSlot+1),
           sf::Color(160,160,160));
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
    int tc=m_player->getCol()+dc,tr=m_player->getRow()+dr;

    for (auto*e:m_enemies)
        if (!e->isDead()&&e->getCol()==tc&&e->getRow()==tr)
        {
            playerAttack(e);
            processTurn();
            m_fog.compute(m_player->getCol(),m_player->getRow(),VIEW_RADIUS,m_tileMap);
            updateCamera();
            return;
        }

    bool moved=m_player->tryMove(dc,dr,m_tileMap);
    if (!moved){addLog("> Blocked!");return;}

    m_turnCount++;
    m_player->onTurnPassed();
    m_fog.compute(m_player->getCol(),m_player->getRow(),VIEW_RADIUS,m_tileMap);
    processTurn(); tryRespawnEnemies(); updateCamera();

    int pc=m_player->getCol(),pr=m_player->getRow();
    for (auto& it:m_mapItems)
        if (it.col==pc&&it.row==pr)
        {addLog("> "+it.name+" here. [G] pick up.",sf::Color(200,200,80));break;}

    TileType tile=m_tileMap.getTile(pc,pr);
    if (tile==TileType::StairsDown) addLog("> Stairs down! [.] descend.",sf::Color(255,220,50));
    if (tile==TileType::StairsUp)   addLog("> Stairs up! [,] ascend.",  sf::Color(100,220,255));

    int hunger=m_player->getStats().hunger;
    if(hunger==50) addLog("> You feel hungry.",       sf::Color(220,180,50));
    if(hunger==20) addLog("> Very hungry!",           sf::Color(220,100,50));
    if(hunger== 0) addLog("> Starving! HP draining!", sf::Color(220,50,50));
}

void Game::playerAttack(Enemy* enemy)
{
    if (!m_player) return;
    const Stats& s = m_player->getStats();
    CoreStats cb   = m_coreSlots.getTotalBonus();

    int atk = s.maxAtk + cb.body/3 + m_equipment.getTotalAtkBonus();
    int dmg = std::max(1, atk + std::rand()%4 - enemy->getDefense());
    enemy->takeDamage(dmg);
    addLog("> You hit "+enemy->getName()+" for "+std::to_string(dmg)+"!",sf::Color(255,200,50));

    if (enemy->isDead())
    {
        addLog("> "+enemy->getName()+" dead! +"+std::to_string(enemy->getExp())+" EXP",
               sf::Color(220,80,80));

        // drop items
        auto dropped = DropTable::instance().roll(enemy->getId());
        for (const auto& itemId : dropped)
        {
            const ItemData* idata = DropTable::instance().getItem(itemId);
            if (!idata) continue;
            Item drop;
            drop.type       = (idata->type=="Core") ? ItemType::Core : ItemType::Material;
            drop.name       = idata->name;
            drop.desc       = idata->desc;
            drop.value      = idata->value;
            drop.hpBonus    = idata->hpBonus;
            drop.atkBonus   = idata->atkBonus;
            drop.defBonus   = idata->defBonus;
            drop.dodgeBonus = idata->dodgeBonus;
            drop.spriteName = idata->sprite;
            drop.stackable  = idata->stackable;
            drop.col        = enemy->getCol();
            drop.row        = enemy->getRow();
            m_mapItems.push_back(drop);
            addLog("> Dropped: "+idata->name, sf::Color(180,220,255));
        }

        if (m_player->addExp(enemy->getExp()))
        {
            m_levelUpFlash=true; m_levelUpTimer=120;
            int newLevel = m_player->getStats().level;
            m_coreSlots.setSlotCount(newLevel);
            addLog("> *** LEVEL UP! Level "+std::to_string(newLevel)+" — Core slot unlocked! ***",
                   sf::Color(255,255,50));
        }
    }
}

void Game::enemyAttack(Enemy* enemy)
{
    if (!m_player) return;
    Stats& s = m_player->getStats();

    int def   = s.maxDef + m_equipment.getTotalDefBonus();
    int dodge = s.maxDodge + m_equipment.getTotalDodgeBonus();
    float dodgeChance = std::min(0.45f, 0.06f + dodge * 0.05f);

    if ((std::rand() % 100) < (int)(dodgeChance * 100))
    {
        addLog("> You dodged "+enemy->getName()+"!", sf::Color(150,220,150));
        return;
    }

    int dmg = std::max(1, enemy->getAttack() - def + std::rand()%3);
    s.hp -= dmg;

    addLog("> "+enemy->getName()+" hits you for "+std::to_string(dmg)+"!",
           sf::Color(220,80,80));

    if (s.hp <= 0)
    {
        s.hp = 0;
        if (!s.hpDepleted)
        {
            s.hpDepleted = true;
            addLog("> WARNING: HP depleted — Mentality drain starts!",
                   sf::Color(255, 120, 50));
            addLog("> Mentality -3/turn until recovered",
                   sf::Color(220, 100, 200));
        }
    }
}

// ============================================================
//  Process Turn
// ============================================================
void Game::processTurn()
{
    if (!m_player) return;

    // ── sync computed stats ──
    Stats& s      = m_player->getStats();
    s.body        = computeBody();
    s.itemLevel   = getItemLevelTotal();
    s.battleIndex = computeBattleIndex();
    s.ability     = m_coreSlots.getTotalBonus().ability;

    // ── mentality drain ──
    drainMentality();

    if (m_playerDead) return;  // หยุดประมวลผล enemy ถ้าตายแล้ว

    // ── enemy AI ──
    for (auto* e : m_enemies)
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

    m_window.setView(m_uiView);
    renderStatusPanel();
    renderRightPanel();
    renderLogPanel();
    if (m_levelUpFlash) renderLevelUpEffect();
    m_window.display();
}

void Game::renderItems()
{
    auto& tm = TextureManager::instance();
    for (const auto& item:m_mapItems)
    {
        if (!m_fog.isVisible(item.col,item.row)) continue;
        float px=(float)(item.col*TILE_SIZE),py=(float)(item.row*TILE_SIZE),ts=(float)TILE_SIZE;

        std::string path = item.spritePath();
        std::string texName = path.substr(path.rfind('/')+1);
        if (texName.find('.') != std::string::npos)
            texName = texName.substr(0, texName.rfind('.'));

        const sf::Texture* tex = tm.get(texName);
        if (tex)
        {
            sf::Sprite spr(*tex);
            sf::Vector2u sz=tex->getSize();
            float scale = ts*0.6f / std::max((float)sz.x,(float)sz.y);
            spr.setScale({scale,scale});
            spr.setPosition({px+ts*0.2f, py+ts*0.2f});
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
//  Status Panel  –  ขวาบน
// ============================================================
void Game::renderStatusPanel()
{
    float px=(float)(WINDOW_W-RIGHT_PANEL_W);
    sf::RectangleShape panel({(float)RIGHT_PANEL_W,(float)STATUS_PANEL_H});
    panel.setFillColor(sf::Color(10,10,10));
    panel.setPosition({px,0.f});
    panel.setOutlineColor(sf::Color(80,60,40));
    panel.setOutlineThickness(2.f);
    m_window.draw(panel);

    if (!m_fontLoaded||!m_player) return;
    const Stats& s  = m_player->getStats();
    float x=px+10.f, y=(float)STATUS_PANEL_TOP_PADDING;

    auto drawLine=[&](const std::string& label, const std::string& value,
                      sf::Color color=sf::Color::White, int size=STATUS_LINE_SIZE)
    {
        sf::Text t(m_font, label+" "+value, (unsigned int)size);
        t.setFillColor(color);
        t.setPosition({x,y});
        m_window.draw(t);
        y += size + STATUS_LINE_SPACING;
    };

    // ── Header ──
    {
        sf::Text header(m_font, "player 1", STATUS_HEADER_SIZE);
        header.setFillColor(sf::Color::Yellow);
        header.setPosition({x,y});
        m_window.draw(header);
        y += STATUS_HEADER_SIZE + STATUS_HEADER_SPACING;
    }

    drawLine("level:", std::to_string(s.level));

    // body — แสดงพร้อม breakdown เล็กน้อย
    drawLine("body:", std::to_string(s.body));

    // (atk/def/dodge คงทำงานเบื้องหลัง ไม่แสดงใน status panel)

    // mentality — สีม่วงถ้ากำลัง drain
    {
        sf::Color mc = s.hpDepleted ? sf::Color(220,100,200) : sf::Color::White;
        std::string mVal = std::to_string(s.maxMentality);
        if (s.hpDepleted) mVal += " (!drain)";
        drawLine("mentality:", mVal, mc);
    }

    drawLine("ability:", std::to_string(s.ability));
    drawLine("item level:", std::to_string(s.itemLevel));

    // battle index
    drawLine("battle", "");
    drawLine("index:", std::to_string(s.battleIndex), sf::Color(255,220,100));

    // separator
    y += 4;
    sf::RectangleShape sep({(float)RIGHT_PANEL_W-20.f, 1.f});
    sep.setFillColor(sf::Color(60,50,30));
    sep.setPosition({x, y});
    m_window.draw(sep);
    y += 6;

    // hp bar numeric
    {
        std::string hpStr = std::to_string(s.hp) + "/" + std::to_string(s.maxHp);
        sf::Color hpColor = s.hp > s.maxHp/2 ? sf::Color(80,200,80) :
                            s.hp > s.maxHp/4 ? sf::Color(220,180,0) :
                                               sf::Color(220,60,60);
        drawLine("hp:", hpStr, hpColor);
    }

    // hunger
    {
        sf::Color hColor = s.hunger > 50 ? sf::Color(180,160,80) :
                           s.hunger > 20 ? sf::Color(220,130,50) :
                                           sf::Color(220,60,60);
        drawLine("hunger:", std::to_string(s.hunger)+"%", hColor);
    }
}

// ============================================================
//  Right Panel Lower  –  Inventory / Equipment / Cores
// ============================================================
void Game::renderRightPanel()
{
    float panelX=(float)(WINDOW_W-RIGHT_PANEL_W);
    float panelY=(float)STATUS_PANEL_H;

    sf::RectangleShape panel({(float)RIGHT_PANEL_W,(float)INV_GRID_H});
    panel.setFillColor(sf::Color(8,8,8));
    panel.setPosition({panelX,panelY});
    panel.setOutlineColor(sf::Color(80,60,40));
    panel.setOutlineThickness(2.f);
    m_window.draw(panel);

    if (!m_fontLoaded) return;

    if (!m_viewEquipment && !m_viewCores)
    {
        // ── Inventory Grid ──
        sf::Text title(m_font,"BAG  [E]=Equip",9);
        title.setFillColor(sf::Color(120,100,60));
        title.setPosition({panelX+8.f,panelY+6.f});
        m_window.draw(title);

        const int COLS=6,ROWS=2;
        const float SZ=28.f,GAP=4.f;
        float sx0=panelX+(RIGHT_PANEL_W-(COLS*(SZ+GAP)-GAP))/2.f;
        float sy0=panelY+22.f;

        for (int row=0;row<ROWS;++row)
        for (int col=0;col<COLS;++col)
        {
            int idx=row*COLS+col;
            float sx=sx0+col*(SZ+GAP),sy=sy0+row*(SZ+GAP);
            bool sel=(idx==m_selectedSlot);
            sf::RectangleShape slot({SZ,SZ});
            slot.setFillColor(sel?sf::Color(60,50,30):sf::Color(25,20,15));
            slot.setOutlineColor(sel?sf::Color(200,160,60):sf::Color(70,55,40));
            slot.setOutlineThickness(1.5f);
            slot.setPosition({sx,sy}); m_window.draw(slot);

            Item* item=m_inventory.getItem(idx);
            if (item)
            {
                auto& tm = TextureManager::instance();
                std::string path = item->spritePath();
                std::string name = path.substr(path.rfind('/')+1);
                name = name.substr(0, name.rfind('.'));
                const sf::Texture* tex = tm.get(name);
                if (tex)
                {
                    sf::Sprite spr(*tex);
                    auto sz=tex->getSize();
                    float sc=SZ*0.8f/std::max((float)sz.x,(float)sz.y);
                    spr.setScale({sc,sc});
                    spr.setPosition({sx+SZ*0.1f,sy+SZ*0.1f});
                    m_window.draw(spr);
                }
                else
                {
                    sf::CircleShape dot(SZ*0.28f);
                    dot.setFillColor(item->color());
                    dot.setPosition({sx+SZ*0.22f,sy+SZ*0.22f});
                    m_window.draw(dot);
                }
                if (idx<9)
                {
                    sf::Text num(m_font,std::to_string(idx+1),7);
                    num.setFillColor(sf::Color(150,150,150));
                    num.setPosition({sx+2.f,sy+1.f});
                    m_window.draw(num);
                }
                int cnt = m_inventory.getCount(idx);
                if (cnt > 1)
                {
                    sf::Text countTxt(m_font,"x"+std::to_string(cnt),7);
                    countTxt.setFillColor(sf::Color(255,220,100));
                    countTxt.setPosition({sx+SZ-14.f, sy+SZ-10.f});
                    m_window.draw(countTxt);
                }
            }
        }

        Item* sel=m_inventory.getItem(m_selectedSlot);
        std::string info="(empty)";
        if (sel)
        {
            int cnt=m_inventory.getCount(m_selectedSlot);
            info=sel->name+" ("+std::to_string(sel->value)+")";
            if (cnt>1) info+=" x"+std::to_string(cnt);
        }
        sf::Text infoTxt(m_font,info,9);
        infoTxt.setFillColor(sf::Color(180,160,100));
        infoTxt.setPosition({panelX+8.f,sy0+ROWS*(SZ+GAP)+4.f});
        m_window.draw(infoTxt);
    }
    else if (m_viewCores)
    {
        m_coreSlots.render(m_window, m_font,
                           panelX+8.f, panelY+20.f, (float)RIGHT_PANEL_W,
                           m_selectedCoreSlot, true);

        CoreStats cb = m_coreSlots.getTotalBonus();
        float hy = panelY + INV_GRID_H - 55.f;
        auto dh = [&](const std::string& t, sf::Color c=sf::Color(70,70,70))
        {
            sf::Text tx(m_font,t,8); tx.setFillColor(c);
            tx.setPosition({panelX+8.f,hy}); m_window.draw(tx); hy+=12.f;
        };
        dh("Bonus: B+"+std::to_string(cb.body)+
           " M+"+std::to_string(cb.mentality)+
           " A+"+std::to_string(cb.ability), sf::Color(100,200,255));
        dh("[C] Close  [1-9] Equip core");
        dh("[X] Remove core");
    }
    else if (m_viewEquipment)
    {
        sf::Text title(m_font,"EQUIPMENT  [E]=Bag",9);
        title.setFillColor(sf::Color(120,160,120));
        title.setPosition({panelX+8.f,panelY+6.f});
        m_window.draw(title);

        const EquipSlot slots[]={
            EquipSlot::Head, EquipSlot::Body, EquipSlot::Arms,
            EquipSlot::Legs, EquipSlot::Feet,
            EquipSlot::MainHand, EquipSlot::OffHand
        };
        const float SZH=18.f,GAP=3.f;
        float sx=panelX+8.f, sy=panelY+20.f;

        for (int i=0;i<7;++i)
        {
            EquipSlot slot=slots[i];
            const Item* item=m_equipment.getItem(slot);

            sf::Text label(m_font,Equipment::slotName(slot)+":",9);
            label.setFillColor(sf::Color(140,120,80));
            label.setPosition({sx,sy}); m_window.draw(label);

            std::string itemName=item?item->name:"--";
            sf::Color itemColor=item?item->color():sf::Color(60,60,60);
            sf::Text itemTxt(m_font,itemName,9);
            itemTxt.setFillColor(itemColor);
            itemTxt.setPosition({sx+44.f,sy}); m_window.draw(itemTxt);

            if (item)
            {
                sf::Text val(m_font,"+"+std::to_string(item->value),9);
                val.setFillColor(sf::Color(180,200,100));
                val.setPosition({sx+160.f,sy}); m_window.draw(val);
            }
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
        t.setPosition({8.f,y});
        m_window.draw(t);
        y+=17.f;
    }
}
