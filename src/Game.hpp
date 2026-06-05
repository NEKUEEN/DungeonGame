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
#include "SkillDB.hpp"

constexpr int WINDOW_W        = 800;
constexpr int WINDOW_H        = 600;
constexpr int RIGHT_PANEL_W   = 250;
constexpr int LOG_PANEL_H     = 170;
constexpr int GAME_VIEW_W     = WINDOW_W - RIGHT_PANEL_W;
constexpr int GAME_VIEW_H     = WINDOW_H - LOG_PANEL_H;
constexpr int INV_GRID_H      = 185;

constexpr int STATUS_PANEL_TOP_PADDING = 20;
constexpr int STATUS_HEADER_SIZE       = 13;
constexpr int STATUS_LINE_SIZE         = 10;
constexpr int STATUS_LINE_SPACING      = 10;
constexpr int STATUS_HEADER_SPACING    = 16;
constexpr int STATUS_PANEL_H           = GAME_VIEW_H - INV_GRID_H;

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
    // ── Event / Update / Render ──
    void processEvents();
    void update();
    void render();
    void renderStatusPanel();
    void renderRightPanel();
    void renderLogPanel();
    void renderItems();
    void renderLevelUpEffect();
    void renderSkillPanel();
    void renderHotbar();
    void renderTargeting();

    // ── Player actions ──
    void handlePlayerMove(int dc, int dr);
    void handleInventoryInput(sf::Keyboard::Key key);
    void tryPickupItem();
    void useOrEquipSelected();
    void equipCore();
    void unequipCore();
    void dropSelectedItem();
    void tryDescendStairs();
    void tryAscendStairs();
    void waitTurn();

    // ── Skill (hotbar) ──
    void executeSkill(int hotbarIdx);        // กด Num1-9 → ใช้ skill ใน slot
    void executeAoe(SkillInstance* sk);      // AOE รอบตัว
    void executeWarp(int col, int row);      // teleport ไปที่ target

    // ── Targeting ──
    void enterTargetingMode();
    void moveTargetCursor(int dc, int dr);
    void confirmTarget();
    void cancelTargeting();

    // ── Turn / Combat / AP ──
    void processTurn();
    void tryRespawnEnemies();
    void spawnEnemy(int floor);
    void spawnEnemies(int count);
    void spawnItems();
    void clearEnemies();
    void playerAttack(Enemy* enemy);
    void enemyAttack(Enemy* enemy);
    void recalcAP();                         // คำนวณ/reset AP จาก buff/passive

    // ── Stats helpers ──
    int  computeBody()        const;
    int  computeMentality()   const;         // ← เพิ่ม
    int  computeBattleIndex() const;
    int  getItemLevelTotal()  const;
    void drainMentality();

    // ── Skill helpers (legacy) ──
    void useSkillBuff(const std::string& skillId);
    void fireRangedAt(int targetCol, int targetRow);
    int  getBuffedAtk() const;
    int  getBuffedDef() const;

    // ── Line helper ──
    std::vector<sf::Vector2i> getLine(int x0, int y0, int x1, int y1) const;

    // ── Misc ──
    void addLog(const std::string& msg, sf::Color color = sf::Color(200,200,200));
    void newDungeon(bool keepPlayer = false);
    void updateCamera();

    // ── SFML ──
    sf::RenderWindow m_window;
    sf::Font         m_font;
    bool             m_fontLoaded = false;
    sf::View         m_gameView;
    sf::View         m_uiView;

    // ── Game objects ──
    TileMap              m_tileMap;
    FogOfWar             m_fog;
    Player*              m_player   = nullptr;
    std::vector<Enemy*>  m_enemies;
    std::vector<Item>    m_mapItems;
    Inventory            m_inventory;
    Equipment            m_equipment;
    CoreSlots            m_coreSlots;

    // ── UI state ──
    int  m_selectedSlot      = 0;
    bool m_viewEquipment     = false;
    int  m_selectedEquipSlot = 0;
    int  m_selectedCoreSlot  = 0;
    bool m_viewCores         = false;

    // ── Targeting state ──
    bool        m_targetingMode  = false;
    int         m_targetCol      = 0;
    int         m_targetRow      = 0;
    std::string m_targetSkillId;

    // ── Dungeon state ──
    int  m_dungeonFloor  = 1;
    int  m_respawnTimer  = 0;
    bool m_levelUpFlash  = false;
    int  m_levelUpTimer  = 0;
    bool m_playerDead    = false;

    struct LogEntry { std::string text; sf::Color color; };
    std::vector<LogEntry> m_log;
    int m_turnCount = 0;
};
