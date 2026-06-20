// ==================== Game.hpp (สมบูรณ์) ====================
#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <set>
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
#include "UIState.hpp"
#include "StatBonus.hpp"
#include "RaceDB.hpp"

constexpr int WINDOW_W        = 800;
constexpr int WINDOW_H        = 600;
constexpr int RIGHT_PANEL_W   = 250;
constexpr int LOG_PANEL_H     = 170;
constexpr int GAME_VIEW_W     = WINDOW_W - RIGHT_PANEL_W;
constexpr int GAME_VIEW_H     = WINDOW_H - LOG_PANEL_H;
constexpr int INV_GRID_H      = 185;

constexpr int STATUS_PANEL_TOP_PADDING = 20;
constexpr float STATUS_HEADER_SIZE       = 11.8;
constexpr float STATUS_LINE_SIZE         = 8.8;
constexpr int STATUS_LINE_SPACING      = 10;
constexpr int STATUS_HEADER_SPACING    = 16;
constexpr int STATUS_PANEL_H           = GAME_VIEW_H - INV_GRID_H;

constexpr int LOG_MAX_LINES   = 7;
constexpr int TILE_SIZE       = 64;
constexpr int MAP_COLS        = 50;
constexpr int MAP_ROWS        = 50;
constexpr int VIEW_RADIUS     = 10;
constexpr int MAX_ENEMIES     = 12;
constexpr int RESPAWN_TURNS   = 15;
constexpr int BOSS_KILL_THRESHOLD = 50;

// สร้างโครงสร้าง FinalStats รวม stat ที่คำนวณแล้วทั้งหมด
struct FinalStats {
    int maxHp = 0;
    int atk = 0;
    int def = 0;
    int dodge = 0;
    int maxMana = 0;
    int matk = 0;      // magic attack
    int mdef = 0;      // magic resist
    int spd = 0;       // speed bonus (อาจติดลบ)
    int slashDmgBonus  = 0;
    int pierceDmgBonus = 0;
    int bluntDmgBonus  = 0;
    int cleaveDmgBonus = 0;
    int bleedBonus  = 0;
    int poisonBonus = 0;
    int burnBonus   = 0;
    int resistBleed  = 0;
    int resistPoison = 0;
    int resistBurn   = 0;
    int resistStun   = 0;
    int resistSlow   = 0;
    int bleedDmgReduce  = 0;
    int bleedDurReduce  = 0;
    int poisonDmgReduce = 0;
    int poisonDurReduce = 0;
    int burnDmgReduce   = 0;
    int burnDurReduce   = 0;
    int stunDurReduce   = 0;
    int slowDurReduce   = 0;
    int maxStamina = 100;
    int staminaRegen = 0;
    float body = 0.f;
    float mentality = 0.f;
    float battleIndex = 0.f;  // เปลี่ยนจาก int
    int itemLevel = 0;
};

class Game
{
public:
    Game();
    ~Game();
    void run();

private:
    // event, update, render
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
    void renderStatusEffects();
    void renderTargeting();
    void renderStatsOverlay();
    void renderDeathScreen();

    //Race
    std::string m_selectedRace = "";
    bool m_inRaceSelect = true;  // เริ่มที่หน้าเลือกเผ่า

    // player actions
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
    void renderRaceSelect();

    void executeSkill(int hotbarIdx);
    void executeAoe(SkillInstance* sk);                 // ใช้กับตำแหน่งผู้เล่น (ของเดิม ไว้เผื่อ legacy call)
    void executeAoeAt(SkillInstance* sk, int col, int row);  // ← เพิ่ม: ปล่อย AOE ที่ตำแหน่ง targeting

    void executeWarp(int col, int row);

    void enterTargetingMode();
    void moveTargetCursor(int dc, int dr);
    void confirmTarget();
    void cancelTargeting();
    // ── Bow system ──
    void enterBowTargeting();
    void fireBow();

    void processTurn();
    void tryRespawnEnemies();
    void spawnEnemy(int floor);
    void spawnEnemies(int count);
    void spawnItems();
    void clearEnemies();
    void playerAttack(Enemy* enemy);
    void enemyAttack(Enemy* enemy);
    void recalcSpeed();         // คำนวณ spd + stamina regen
    void regenStamina();        // ฟื้น stamina ต่อเทิร์น

    std::string pickRandomMonster(int floor);
    std::set<std::string> m_firstKillDone;  // id ที่เคย first kill แล้ว
    void onEnemyKilled(Enemy* enemy);
    void spawnBoss(const std::string& family);



    // ตัวช่วยคำนวณ FinalStats
    void recalcAllStats();      // เรียกทุกครั้งที่ equipment / core / skill เปลี่ยน

    int  getItemLevelTotal() const;
    int m_statsScrollOffset = 0;
    void drainMentality();
    void refreshStats();        // เรียก recalcAllStats แล้ว update delta

    // skill helpers (จะใช้ค่าจาก m_finalStats)
    void useSkillBuff(const std::string& skillId);
    void fireRangedAt(int targetCol, int targetRow);
    
    int  getScaledDamage(const SkillEffect& effect) const;
    int  getBuffedAtk() const { return m_finalStats.atk; }
    int  getBuffedDef() const { return m_finalStats.def; }
    int  getBuffedMagicDmg() const { return m_finalStats.matk; }
    int  computeBody() const { return m_finalStats.body; }
    int  computeMentality() const { return m_finalStats.mentality; }
    int  computeBattleIndex() const { return m_finalStats.battleIndex; }

    std::vector<sf::Vector2i> getLine(int x0, int y0, int x1, int y1) const;

    void addLog(const std::string& msg, sf::Color color = sf::Color(200,200,200));
    void newDungeon(bool keepPlayer = false);
    void updateCamera();
    void applyLetterbox();

    // สมาชิก
    sf::RenderWindow m_window;
    sf::Font         m_font;
    bool             m_fontLoaded = false;
    sf::View         m_gameView;
    sf::View         m_uiView;

    std::mt19937                          m_rng{ std::random_device{}() };
    std::uniform_real_distribution<float> m_fDist{ 0.f, 1.f };

    TileMap              m_tileMap;
    FogOfWar             m_fog;
    Player*              m_player   = nullptr;
    std::vector<Enemy*>  m_enemies;
    std::vector<Item>    m_mapItems;
    Inventory            m_inventory;
    Equipment            m_equipment;
    CoreSlots            m_coreSlots;

    UIState m_ui;
    FinalStats m_finalStats;   // รวม stat ที่คำนวณแล้ว

    // dungeon state
    int  m_dungeonFloor  = 1;
    int  m_mapCols       = MAP_COLS;
    int  m_mapRows       = MAP_ROWS;
    int  m_respawnTimer  = 0;
    bool m_playerDead    = false;
    bool m_isFullscreen  = false;

    std::map<std::string, int>  m_familyKillCount;
    std::map<std::string, bool> m_bossActive;

    float m_deltaBody = 0, m_deltaMentality = 0, m_deltaBattleIndex = 0;
    int m_deltaTimer = 0;

    struct LogEntry { std::string text; sf::Color color; };
    std::vector<LogEntry> m_log;
    int m_turnCount = 0;
};