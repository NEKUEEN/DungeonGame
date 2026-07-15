// ==================== Game.hpp (สมบูรณ์) ====================
#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <set>
#include <string>
#include <map>
#include <random>
#include <optional>
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
#include "NPCManager.hpp"
#include "NPCDB.hpp"
#include "PartyUI.hpp"

constexpr int BASE_WINDOW_W   = 800;   // ค่าเริ่มต้นชั่วคราวก่อน constructor เรียก refreshWindowMetrics() ด้วยขนาดจอจริง
constexpr int BASE_WINDOW_H   = 600;
constexpr int RIGHT_PANEL_W   = 250;   // ขนาด panel เป็น pixel คงที่ ไม่ขึ้นกับความละเอียดจอ
constexpr int LOG_PANEL_H     = 170;
constexpr int INV_GRID_H      = 185;

// ── ค่าที่ปรับเปลี่ยนได้ตอนรันไทม์ (เดิมเป็น constexpr คงที่ 800x600 เสมอ) ──
// ตอนนี้เปลี่ยนได้ เพื่อให้เกม (ซึ่งเป็นฟูลสกรีนเสมอ) ใช้ "ความละเอียดจอจริง"
// ของเครื่องนั้นๆ แทนการ scale ภาพ 800x600 เดิมให้ยืด/หด (ดู Game::refreshWindowMetrics)
extern int WINDOW_W;
extern int WINDOW_H;
extern int GAME_VIEW_W;   // = WINDOW_W - RIGHT_PANEL_W
extern int GAME_VIEW_H;   // = WINDOW_H - LOG_PANEL_H
extern int STATUS_PANEL_H; // = GAME_VIEW_H - INV_GRID_H

constexpr int STATUS_PANEL_TOP_PADDING = 20;
constexpr float STATUS_HEADER_SIZE       = 11.8;
constexpr float STATUS_LINE_SIZE         = 8.8;
constexpr int STATUS_LINE_SPACING      = 10;
constexpr int STATUS_HEADER_SPACING    = 16;

constexpr int LOG_MAX_LINES   = 7;
constexpr int TILE_SIZE       = 64;
constexpr int MAP_COLS        = 50;
constexpr int MAP_ROWS        = 50;
constexpr int VIEW_RADIUS     = 10;
constexpr int DARK_ZONE_VIEW_RADIUS = 1;  // ระยะมองเห็นในโซนมืด (เช่น Darkness) — ไม่มีไฟ
constexpr int MAX_ENEMIES     = 1;
constexpr int RESPAWN_TURNS   = 12;
constexpr int BOSS_KILL_THRESHOLD = 50;
constexpr int COMPANION_DETECT_RANGE = 6;  // ระยะที่ companion "เห็น" มอนแล้วแหกแถวไปไล่ (ข้อ 4)

struct ItemData;  // fwd decl (นิยามเต็มอยู่ใน DropTable.hpp ซึ่ง Game.cpp include ไว้แล้ว)

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
    void renderPartyUI();

    //Race
    std::string m_selectedRace = "";
    bool m_inRaceSelect = true;  // เริ่มที่หน้าเลือกเผ่า
    int m_hoverRaceIdx = -1;

    // player actions
    void handlePlayerMove(int dc, int dr);
    std::vector<std::pair<int,int>> findPath(int sc, int sr, int ec, int er);
    void handleMouseMove(int col, int row);
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
    void executeAoeWarp(SkillInstance* sk, int col, int row);  // ← สกิลโดด: วาร์ป + AOE รอบจุดที่ลง

    void enterTargetingMode();
    void moveTargetCursor(int dc, int dr);
    void confirmTarget();
    void cancelTargeting();
    // ── Bow system ──
    void enterBowTargeting();
    void fireBow();

    void processTurn();
    // ── แตกออกจาก processTurn เดิม (เคยยาว ~330 บรรทัดในฟังก์ชันเดียว) ──
    void processEnemyTurn(Enemy* e, long long playerTime, int pc, int pr,
                           const std::vector<std::pair<int,int>>& partyBlockedTiles);
    void processCompanionActions(long long playerTime);
    void tryRespawnEnemies();
    void spawnEnemy(int floor);
    void spawnEnemies(int count);
    void spawnItems();
    void spawnNPCs(int count = 3);  // Spawn random NPCs
    void recruitNPC(const std::string& npcId);  // Add NPC to party
    void tryInteractNPC();  // Try to interact with adjacent NPC
    // ── PartyUI commands (ข้อ 5) ──
    void dismissSelectedCompanion();        // ไล่ companion ที่เลือกใน PartyUI ออก กลับไปยืนบนแมพที่ตำแหน่งผู้เล่น
    void reorderSelectedCompanion(int dir);  // สลับลำดับแถวของ companion ที่เลือก (dir = -1 ขึ้น / +1 ลง)
    void updatePartyFollowPositions(const std::vector<bool>& skip = {});  // Update companion positions (follow player); skip[i]=true → engaged, ข้าม
    void clearEnemies();
    void playerAttack(Enemy* enemy);
    void enemyAttack(Enemy* enemy);
    void enemyAttackCompanion(Enemy* enemy, std::shared_ptr<NPC> npc);  // ← เพิ่มบรรทัดนี้
    void companionAttack(std::shared_ptr<NPC> npc, Enemy* enemy);       // ← companion ฟันมอนกลับ (ข้อ 2: passive exchange)
    bool tryMoveCompanion(std::shared_ptr<NPC> npc, int dc, int dr);    // ← เดินทีละก้าว (ข้อ 4: active AI), false ถ้าเดินไม่ได้
    void recalcSpeed();         // คำนวณ spd + stamina regen
    void regenStamina();        // ฟื้น stamina ต่อเทิร์น
    long long m_globalTime = 0;  // Aut สะสมของเกม

    // ── Vision / Fog ──
    int  getCurrentVisionRadius() const;  // ลด radius ถ้า player อยู่ในโซนมืด (เช่น Darkness)
    void recomputeFog();                  // wrapper: m_fog.compute() ด้วย radius ที่ถูกต้องตาม zone ปัจจุบัน
    void recomputeFog(int col, int row);  // overload สำหรับกรณีคำนวณก่อน setPos เสร็จ (เช่น warp)

    std::string pickRandomMonster(int floor);
    std::set<std::string> m_firstKillDone;  // id ที่เคย first kill แล้ว
    void onEnemyKilled(Enemy* enemy);
    void grantPartyExp(int expGain);  // ข้อ 6: แจก EXP ให้ player + companion ทุกคนที่ยังไม่ตายเท่ากัน
    void spawnBoss(const std::string& family);

    // ── Enemy death / loot: รวมศูนย์จาก 5 จุดที่เคยก็อปแปะ
    //    (playerAttack, fireBow, fireRangedAt, companionAttack, processTurn DOT) ──
    Item makeDropItem(const ItemData* idata, int col, int row) const;
    void handleEnemyDeath(Enemy* enemy, const std::string& killerName = "");
    
    //tileMousse sel
    int m_hoverCol = -1;
    int m_hoverRow = -1;
    std::vector<std::pair<int,int>> m_travelPath;
    int m_travelStep = 0;
    bool m_travelMode = false; // true = travel, false = single step
    sf::Clock m_hoverLockClock;
    bool m_hoverLocked = false;
    int m_lockedHoverCol = -1, m_lockedHoverRow = -1;

    // ตัวช่วยคำนวณ FinalStats
    void recalcAllStats();      // เรียกทุกครั้งที่ equipment / core / skill เปลี่ยน

    int  getItemLevelTotal() const;
    int m_statsScrollOffset = 0;
    void drainMentality();
    void refreshStats();        // เรียก recalcAllStats แล้ว update delta

    // skill helpers (จะใช้ค่าจาก m_finalStats)
    void useSkillBuff(const std::string& skillId);
    void fireRangedAt(int targetCol, int targetRow);
    void applyStatusOnHit(SkillInstance* sk, Enemy* e);  // ← ใช้ apply status effect ตอน AOE โดน (ใช้ pattern เดียวกับ fireRangedAt)
    void applyKnockback(Enemy* e, int sourceCol, int sourceRow, int tiles);  // ← ผลักศัตรูออกจากจุดศูนย์กลาง sourceCol/Row
    
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
    void refreshWindowMetrics(int w, int h);  // ← อัปเดต WINDOW_W/H, GAME_VIEW_W/H ฯลฯ ตามความละเอียดจอจริง (เรียกตอนเปิดเกม)


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
    std::map<int, std::vector<Enemy*>> m_floorEnemiesSaved;  // ← enemies ที่เหลือรอดต่อชั้น (ไม่ delete ตอนย้ายชั้น)
    std::vector<Item>    m_mapItems;
    std::map<int, std::vector<Item>> m_floorItemsSaved;  // ← เก็บของที่ตกพื้นแยกตามชั้น กันหายตอนย้ายชั้นแบบ DCSS
    Inventory            m_inventory;
    Equipment            m_equipment;
    CoreSlots            m_coreSlots;
    NPCManager           m_npcManager{TILE_SIZE};
    std::map<int, std::vector<std::shared_ptr<NPC>>> m_floorNPCsSaved;  // ← NPC (ที่ยังไม่ recruit) ต่อชั้น

    UIState m_ui;
    FinalStats m_finalStats;   // รวม stat ที่คำนวณแล้ว

    // dungeon state
    int  m_dungeonFloor  = 1;
    int  m_mapCols       = MAP_COLS;
    int  m_mapRows       = MAP_ROWS;
    int  m_respawnTimer  = 0;
    bool m_playerDead    = false;

    std::map<std::string, int>  m_familyKillCount;
    std::map<std::string, bool> m_bossActive;

    float m_deltaBody = 0, m_deltaMentality = 0, m_deltaBattleIndex = 0;
    int m_deltaTimer = 0;

    struct LogEntry {
        std::string text;
        sf::Color color;
        sf::Text renderText;   // cache: สร้าง sf::Text ครั้งเดียวตอน addLog ไม่ต้องสร้างใหม่ทุกเฟรม
        LogEntry(const sf::Font& font, std::string t, sf::Color c)
            : text(std::move(t)), color(c), renderText(font, text, 8)
        {
            renderText.setFillColor(color);
        }
    };
    std::vector<LogEntry> m_log;
    int m_turnCount = 0;

    // ── Hotbar render cache (ลด sf::Text/RectangleShape allocation ทุกเฟรม) ──
    // หนึ่ง slot คงที่ 9 ช่อง สร้าง sf::Text/Sprite ครั้งเดียวตอนเปิดเกม (lazy, ใน renderHotbar)
    // แล้ว frame ต่อไปแค่ setString/setFillColor/setPosition แทนสร้างใหม่
    struct HotbarSlotCache {
        sf::RectangleShape slotBg{ {32.f, 32.f} };
        std::optional<sf::Text> numTxt;       // เลขช่อง 1-9 (ไม่เปลี่ยน เซ็ตครั้งแรกพอ)
        std::optional<sf::Sprite> icon;       // ไอคอนสกิล
        sf::RectangleShape cooldownOverlay{ {32.f, 32.f} };
        std::optional<sf::Text> cooldownTxt;  // เลข cooldown เหลือ (เปลี่ยนบ่อย → setString)
        sf::RectangleShape activeGlow{ {32.f, 32.f} };
        std::optional<sf::Text> nameTxt;      // ชื่อสกิลย่อ (เปลี่ยนเมื่อสลับสกิลใน hotbar)
        std::string lastSkillId;              // ใช้เช็คว่าต้อง rebuild icon/nameTxt ไหม
    };
    std::vector<HotbarSlotCache> m_hotbarCache;  // size = 9, lazy-built ใน renderHotbar()

    // ── Status effect icon cache (passive / buff / debuff แถบล่างขวา) ──
    struct StatusIconCache {
        sf::RectangleShape slotBg{ {32.f, 32.f} };
        std::optional<sf::Sprite> icon;
        std::optional<sf::Text> fallbackTxt;   // ใช้เมื่อไม่มี icon texture
        std::optional<sf::Text> durTxt;        // ตัวเลข duration (เปลี่ยนทุกเทิร์น → setString)
        std::string lastKey;                   // skill id / status typeId ปัจจุบันของ slot นี้
    };
    std::vector<StatusIconCache> m_passiveCache;
    std::vector<StatusIconCache> m_buffCache;
    std::vector<StatusIconCache> m_statusEffectCache;
    
    // Party UI and follow system
    PartyUI m_partyUI;
    std::vector<std::pair<int, int>> m_partyFollowPath;  // For smooth party movement
};