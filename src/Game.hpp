#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <map>
#include <random>
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

constexpr int LOG_MAX_LINES   = 9;
constexpr int TILE_SIZE       = 64;
constexpr int MAP_COLS        = 50;   // default fallback (random gen)
constexpr int MAP_ROWS        = 50;   // runtime size stored in m_mapCols/m_mapRows
constexpr int VIEW_RADIUS     = 4;
constexpr int MAX_ENEMIES     = 12;
constexpr int RESPAWN_TURNS   = 15;

// ── Boss trigger: ฆ่ามอนใน family ครบกี่ตัวถึง spawn boss ──
constexpr int BOSS_KILL_THRESHOLD = 50;

constexpr bool USE_HANDCRAFT_MAP = false; // เปลี่ยนเป็น true เมื่อพร้อม

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
    void unequipSelected();

    // ── Skill (hotbar) ──
    void executeSkill(int hotbarIdx);
    void executeAoe(SkillInstance* sk);
    void executeWarp(int col, int row);

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
    void recalcAP();

    // ── Rank spawn helpers ──
    // เลือก rank ตาม weighted random แล้วคืน monster id ใน rank นั้น
    std::string pickRandomMonster(int floor);

    // ── Kill counter / Boss trigger ──
    // เรียกหลัง enemy ตาย — อัพเดท counter และ trigger boss ถ้าถึง threshold
    void onEnemyKilled(Enemy* enemy);
    // spawn boss ของ family นั้นที่ตำแหน่งเหมาะสม
    void spawnBoss(const std::string& family);

    // ── Stats helpers ──
    int  computeBody()        const;
    int  computeMentality()   const;
    int  computeBattleIndex() const;
    int  getItemLevelTotal()  const;
    void drainMentality();
    void refreshStats();

    // ── Skill helpers (legacy) ──
    void useSkillBuff(const std::string& skillId);
    void fireRangedAt(int targetCol, int targetRow);
    int  getBuffedAtk() const;
    int  getBuffedDef() const;
    int  getBuffedMagicDmg() const;
    int  getScaledDamage(const SkillEffect& effect) const;

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

    // ── RNG (ใช้ <random> แทน rand()) ──
    std::mt19937                          m_rng{ std::random_device{}() };
    std::uniform_real_distribution<float> m_fDist{ 0.f, 1.f };

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
    int  m_mapCols       = MAP_COLS;   // actual runtime map size
    int  m_mapRows       = MAP_ROWS;
    int  m_respawnTimer  = 0;
    bool m_levelUpFlash  = false;
    int  m_levelUpTimer  = 0;
    bool m_playerDead    = false;

    // ── Kill counters ──
    // key = family name ("Goblin", "Orc", "Rat" ฯลฯ)
    // value = จำนวนที่ฆ่าไปในทุก non-Boss rank
    std::map<std::string, int>  m_familyKillCount;
    // ป้องกัน boss เกิดซ้ำก่อนที่จะฆ่าบอสตัวเดิม
    std::map<std::string, bool> m_bossActive;

    // ── Stat delta ──
    int m_deltaBody        = 0;
    int m_deltaMentality   = 0;
    int m_deltaBattleIndex = 0;
    int m_deltaTimer       = 0;

    struct LogEntry { std::string text; sf::Color color; };
    std::vector<LogEntry> m_log;
    int m_turnCount = 0;
};
