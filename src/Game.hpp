#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "TileMap.hpp"
#include "Player.hpp"
#include "Enemy.hpp"
#include "FogOfWar.hpp"
#include "Item.hpp"
#include "Inventory.hpp"
#include "Equipment.hpp"
#include "TextureManager.hpp"
#include "CoreSlots.hpp"

constexpr int WINDOW_W        = 860;
constexpr int WINDOW_H        = 660;
constexpr int RIGHT_PANEL_W   = 220;
constexpr int LOG_PANEL_H     = 140;
constexpr int GAME_VIEW_W     = WINDOW_W - RIGHT_PANEL_W;
constexpr int GAME_VIEW_H     = WINDOW_H - LOG_PANEL_H;
constexpr int INV_GRID_H      = 200;   // เพิ่มความสูงสำหรับ equip slots
constexpr int STATUS_PANEL_H  = GAME_VIEW_H - INV_GRID_H;
constexpr int LOG_MAX_LINES   = 7;

constexpr int TILE_SIZE       = 64;
constexpr int MAP_COLS        = 50;
constexpr int MAP_ROWS        = 50;
constexpr int VIEW_RADIUS     = 6;
constexpr int MAX_ENEMIES     = 12;
constexpr int RESPAWN_TURNS   = 15;

class Game
{
public:
    Game();
    ~Game();
    void run();

private:
    void processEvents();
    void update();
    void render();
    void renderStatusPanel();
    void renderRightPanel();      // inventory + equipment รวม
    void renderLogPanel();
    void renderItems();
    void renderLevelUpEffect();

    void handlePlayerMove(int dc, int dr);
    void handleInventoryInput(sf::Keyboard::Key key);
    void tryPickupItem();
    void useOrEquipSelected();
    void equipCore();          // ใส่แกนจาก inventory ไป core slot
    void unequipCore();        // ถอดแกนจาก core slot กลับ inventory
    void dropSelectedItem();
    void tryDescendStairs();
    void tryAscendStairs();
    void waitTurn();

    void processTurn();
    void tryRespawnEnemies();
    void spawnEnemy(int floor);
    void spawnEnemies(int count);
    void spawnItems();
    void clearEnemies();
    void playerAttack(Enemy* enemy);
    void enemyAttack(Enemy* enemy);
    void addLog(const std::string& msg, sf::Color color = sf::Color(200,200,200));
    void newDungeon(bool keepPlayer = false);
    void updateCamera();

    sf::RenderWindow m_window;
    sf::Font         m_font;
    bool             m_fontLoaded = false;

    sf::View         m_gameView;
    sf::View         m_uiView;

    TileMap              m_tileMap;
    FogOfWar             m_fog;
    Player*              m_player   = nullptr;
    std::vector<Enemy*>  m_enemies;
    std::vector<Item>    m_mapItems;
    Inventory            m_inventory;
    Equipment            m_equipment;
    CoreSlots            m_coreSlots;

    // UI state
    int  m_selectedSlot      = 0;
    bool m_viewEquipment     = false;
    int  m_selectedEquipSlot = 0;
    int  m_selectedCoreSlot  = 0;
    bool m_viewCores         = false;

    int  m_dungeonFloor  = 1;
    int  m_respawnTimer  = 0;
    bool m_levelUpFlash  = false;
    int  m_levelUpTimer  = 0;

    struct LogEntry { std::string text; sf::Color color; };
    std::vector<LogEntry> m_log;
    int m_turnCount = 0;
};
