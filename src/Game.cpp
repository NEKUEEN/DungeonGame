#include "Game.hpp"
#include "TextureManager.hpp"
#include <iostream>
#include <cstdlib>
#include <algorithm>

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

    // โหลด textures ทั้งหมดครั้งเดียว
    TextureManager::instance().loadAll();

    m_gameView.setSize({(float)GAME_VIEW_W*1.1f,(float)GAME_VIEW_H*1.1f});
    m_gameView.setViewport(sf::FloatRect({0.f,0.f},
        {(float)GAME_VIEW_W/WINDOW_W,(float)GAME_VIEW_H/WINDOW_H}));
    m_uiView.setSize({(float)WINDOW_W,(float)WINDOW_H});
    m_uiView.setCenter({(float)WINDOW_W/2.f,(float)WINDOW_H/2.f});

    newDungeon(false);
}

Game::~Game() { delete m_player; clearEnemies(); }

void Game::newDungeon(bool keepPlayer)
{
    m_tileMap.generate();
    m_fog.reset();

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

    if (m_player) m_fog.compute(m_player->getCol(),m_player->getRow(),VIEW_RADIUS,m_tileMap);
    updateCamera();
    m_selectedSlot=0; m_viewEquipment=false;

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

void Game::spawnEnemies(int count) { for(int i=0;i<count;++i) spawnEnemy(m_dungeonFloor); }

void Game::spawnEnemy(int floor)
{
    for (int att=0;att<100;++att)
    {
        int col=1+std::rand()%(MAP_COLS-2),row=1+std::rand()%(MAP_ROWS-2);
        if (!m_tileMap.isWalkable(col,row)) continue;
        if (m_player){int dx=col-m_player->getCol(),dy=row-m_player->getRow();if(dx*dx+dy*dy<36)continue;}
        int r=std::rand()%100;
        EnemyRank rank=r<5?EnemyRank::Boss:r<30?EnemyRank::Elite:EnemyRank::Normal;
        m_enemies.push_back(new Enemy((EnemyType)(std::rand()%3),rank,col,row,TILE_SIZE,floor));
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
        int r=std::rand()%10;
        if(r<3)      item=Item::makeFood();
        else if(r<5) item=Item::makePotion();
        else if(r<7) item=Item::makeWeapon();
        else if(r<8) item=Item::makeOffWeapon();
        else         item=Item::makeArmor();
        item.col=col; item.row=row;
        m_mapItems.push_back(item);
    }
}

void Game::clearEnemies(){for(auto*e:m_enemies)delete e;m_enemies.clear();}

void Game::run(){while(m_window.isOpen()){processEvents();update();render();}}

void Game::update()
{
    if (m_levelUpFlash){m_levelUpTimer--;if(m_levelUpTimer<=0)m_levelUpFlash=false;}
}

void Game::processEvents()
{
    while (const std::optional event = m_window.pollEvent())
    {
        if (event->is<sf::Event::Closed>()) m_window.close();
        if (const auto* key = event->getIf<sf::Event::KeyPressed>())
        {
            switch(key->code)
            {
                case sf::Keyboard::Key::Escape: m_window.close();       break;
                case sf::Keyboard::Key::Up:
                case sf::Keyboard::Key::K:  handlePlayerMove(0,-1);     break;
                case sf::Keyboard::Key::Down:
                case sf::Keyboard::Key::J:  handlePlayerMove(0, 1);     break;
                case sf::Keyboard::Key::Left:
                case sf::Keyboard::Key::H:  handlePlayerMove(-1,0);     break;
                case sf::Keyboard::Key::Right:
                case sf::Keyboard::Key::L:  handlePlayerMove(1, 0);     break;
                case sf::Keyboard::Key::Y:  handlePlayerMove(-1,-1);    break;
                case sf::Keyboard::Key::U:  handlePlayerMove(1,-1);     break;
                case sf::Keyboard::Key::B:  handlePlayerMove(-1, 1);    break;
                case sf::Keyboard::Key::N:  handlePlayerMove(1,  1);    break;
                case sf::Keyboard::Key::R:  newDungeon(false);          break;
                case sf::Keyboard::Key::G:  tryPickupItem();            break;
                case sf::Keyboard::Key::D:  dropSelectedItem();         break;
                case sf::Keyboard::Key::E:
                    m_viewEquipment=!m_viewEquipment;
                    addLog(m_viewEquipment?"> Equipment view":"> Inventory view",
                           sf::Color(160,160,160));
                    break;
                case sf::Keyboard::Key::Period: tryDescendStairs();     break;
                case sf::Keyboard::Key::Comma:  tryAscendStairs();      break;
                case sf::Keyboard::Key::Space:  waitTurn();             break;
                case sf::Keyboard::Key::Num1: m_selectedSlot=0; useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num2: m_selectedSlot=1; useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num3: m_selectedSlot=2; useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num4: m_selectedSlot=3; useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num5: m_selectedSlot=4; useOrEquipSelected(); break;
                case sf::Keyboard::Key::Num6: m_selectedSlot=5; useOrEquipSelected(); break;
                case sf::Keyboard::Key::Tab:
                    m_selectedSlot=(m_selectedSlot+1)%MAX_INVENTORY; break;
                default: break;
            }
        }
    }
}

void Game::handleInventoryInput(sf::Keyboard::Key){}

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
        {s.hp=std::min(s.body,s.hp+item->value);
         addLog("> Used "+item->name+". HP +"+std::to_string(item->value),sf::Color(80,180,220));}
        m_inventory.removeItem(m_selectedSlot);
    }
    else
    {
        // Equipment → ใส่ใน equip slot ที่เหมาะสม
        EquipSlot slot=Equipment::itemToSlot(*item);

        // ถ้า slot มีของอยู่แล้ว → เอาออกมาใส่กระเป๋าก่อน
        if (m_equipment.hasItem(slot))
        {
            Item old=m_equipment.unequip(slot);
            if (!m_inventory.isFull()) m_inventory.addItem(old);
            else { addLog("> Inventory full! Unequip failed.",sf::Color(220,120,50)); return; }
        }

        Item toEquip=*item;
        m_inventory.removeItem(m_selectedSlot);
        m_equipment.equip(toEquip,slot);

        addLog("> Equipped "+toEquip.name+" ["+Equipment::slotName(slot)+"] +"+
               std::to_string(toEquip.value),sf::Color(180,220,180));
    }
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

void Game::handlePlayerMove(int dc, int dr)
{
    if (!m_player) return;
    int tc=m_player->getCol()+dc,tr=m_player->getRow()+dr;

    for (auto*e:m_enemies)
        if (!e->isDead()&&e->getCol()==tc&&e->getRow()==tr)
        {playerAttack(e);processTurn();
         m_fog.compute(m_player->getCol(),m_player->getRow(),VIEW_RADIUS,m_tileMap);
         updateCamera();return;}

    bool moved=m_player->tryMove(dc,dr,m_tileMap);
    if (!moved){addLog("> Blocked!");return;}

    m_turnCount++; m_player->onTurnPassed();
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
    Stats& s=m_player->getStats();
    int atk=s.body/5+m_equipment.getTotalAtkBonus();
    int dmg=std::max(1,atk+std::rand()%4-enemy->getDefense());
    enemy->takeDamage(dmg);
    addLog("> You hit "+enemy->getName()+" for "+std::to_string(dmg)+"!",sf::Color(255,200,50));
    if (enemy->isDead())
    {
        addLog("> "+enemy->getName()+" dead! +"+std::to_string(enemy->getExp())+" EXP",sf::Color(220,80,80));
        if (m_player->addExp(enemy->getExp()))
        {
            m_levelUpFlash=true; m_levelUpTimer=120;
            addLog("> *** LEVEL UP! Level "+std::to_string(m_player->getStats().level)+" ***",
                   sf::Color(255,255,50));
        }
    }
}

void Game::enemyAttack(Enemy* enemy)
{
    if (!m_player) return;
    Stats& s=m_player->getStats();
    int def=s.body/10+m_equipment.getTotalDefBonus();
    int dmg=std::max(1,enemy->getAttack()-def+std::rand()%3);
    s.hp-=dmg;
    addLog("> "+enemy->getName()+" hits you for "+std::to_string(dmg)+"!",sf::Color(220,80,80));
    if (s.hp<=0){s.hp=0;addLog("> You died on floor "+std::to_string(m_dungeonFloor)+"! [R] restart.",sf::Color(255,50,50));}
}

void Game::processTurn()
{
    if (!m_player) return;
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

void Game::addLog(const std::string& msg,sf::Color color)
{
    m_log.push_back({msg,color});
    if((int)m_log.size()>LOG_MAX_LINES) m_log.erase(m_log.begin());
}

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

        // เลือก texture ตาม type
        std::string texName;
        switch(item.type)
        {
            case ItemType::Food:      texName="item_food";    break;
            case ItemType::Potion:    texName="item_potion";  break;
            case ItemType::Weapon:
            case ItemType::OffWeapon: texName="item_weapon";  break;
            default:                  texName="item_armor";   break;
        }

        const sf::Texture* tex = tm.get(texName);
        if (tex)
        {
            sf::Sprite spr(*tex);
            sf::Vector2u sz=tex->getSize();
            float scale = ts * 0.6f / std::max((float)sz.x,(float)sz.y);
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
    const Stats& s=m_player->getStats();
    float x=px+10.f,y=10.f;

    auto dl=[&](const std::string& text,sf::Color color=sf::Color::White,int size=11)
    {
        sf::Text t(m_font,text,(unsigned int)size);
        t.setFillColor(color); t.setPosition({x,y});
        m_window.draw(t); y+=size+5.f;
    };

    dl("player 1",sf::Color::Yellow,13); y+=2.f;
    dl("Floor: "+std::to_string(m_dungeonFloor),sf::Color(180,140,60),11); y+=2.f;
    dl("level: "+std::to_string(s.level));
    dl("EXP: "+std::to_string(s.exp)+"/"+std::to_string(s.expToNext),sf::Color(100,200,255),10);

    float bw=(float)RIGHT_PANEL_W-20.f;
    sf::RectangleShape ebg({bw,5.f}); ebg.setFillColor(sf::Color(30,30,60)); ebg.setPosition({x,y}); m_window.draw(ebg);
    float er=std::clamp((float)s.exp/s.expToNext,0.f,1.f);
    sf::RectangleShape ebar({bw*er,5.f}); ebar.setFillColor(sf::Color(80,160,255)); ebar.setPosition({x,y}); m_window.draw(ebar);
    y+=10.f;

    dl("body: "+std::to_string(s.body));
    dl("HP: "+std::to_string(s.hp)+"/"+std::to_string(s.body),
       s.hp>s.body/2?sf::Color(50,200,50):sf::Color(220,80,80));
    dl("mentality: "+std::to_string(s.mentality));
    dl("ability: "+std::to_string(s.ability)); y+=2.f;
    sf::Color hc=s.hunger>50?sf::Color(100,200,100):s.hunger>20?sf::Color(220,180,50):sf::Color(220,60,60);
    dl("hunger: "+std::to_string(s.hunger)+"/100",hc); y+=2.f;
    dl("ATK: +"+std::to_string(m_equipment.getTotalAtkBonus()),sf::Color(200,200,100),10);
    dl("DEF: +"+std::to_string(m_equipment.getTotalDefBonus()),sf::Color(140,180,140),10);
    dl("Turn: "+std::to_string(m_turnCount),sf::Color(120,120,120),10);
    dl("Enemies: "+std::to_string(m_enemies.size()),sf::Color(200,100,100),10);
    y+=6.f;
    dl("[HJKL] Move [G]Pick",sf::Color(70,70,70),9);
    dl("[D]Drop [1-6]Use/Eq",sf::Color(70,70,70),9);
    dl("[E]Equip [Tab]Sel",sf::Color(70,70,70),9);
    dl("[.]Down [,]Up [Spc]Wait",sf::Color(70,70,70),9);
}

// ============================================================
//  Right Panel Lower  –  Inventory หรือ Equipment
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

    if (!m_viewEquipment)
    {
        // ---- Inventory Grid ----
        sf::Text title(m_font,"BAG  [E]=Equip view",9);
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
                // หา texture name จาก spritePath
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
                // แสดง stack count ถ้ามากกว่า 1
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
    else
    {
        // ---- Equipment Slots ----
        sf::Text title(m_font,"EQUIPMENT  [E]=Bag",9);
        title.setFillColor(sf::Color(120,160,120));
        title.setPosition({panelX+8.f,panelY+6.f});
        m_window.draw(title);

        const EquipSlot slots[]={
            EquipSlot::Head, EquipSlot::Body, EquipSlot::Arms,
            EquipSlot::Legs, EquipSlot::Feet,
            EquipSlot::MainHand, EquipSlot::OffHand
        };
        const float SZW=80.f,SZH=18.f,GAP=3.f;
        float sx=panelX+8.f, sy=panelY+20.f;

        for (int i=0;i<7;++i)
        {
            EquipSlot slot=slots[i];
            const Item* item=m_equipment.getItem(slot);

            // slot label
            sf::Text label(m_font,Equipment::slotName(slot)+":",9);
            label.setFillColor(sf::Color(140,120,80));
            label.setPosition({sx,sy}); m_window.draw(label);

            // item name
            std::string itemName=item?item->name:"--";
            sf::Color itemColor=item?item->color():sf::Color(60,60,60);
            sf::Text itemTxt(m_font,itemName,9);
            itemTxt.setFillColor(itemColor);
            itemTxt.setPosition({sx+44.f,sy}); m_window.draw(itemTxt);

            // value
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
