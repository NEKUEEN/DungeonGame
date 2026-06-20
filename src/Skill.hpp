#pragma once
#include <string>

// ============================================================
//  SkillType
// ============================================================
enum class SkillType
{
    ActiveBuff,    // กด key -> buff ชั่วคราว
    Passive,       // auto-apply ตลอดเวลา
    ActiveRanged,  // กด key -> targeting -> ยิง projectile
    ActiveHeal,    // กด key -> heal ทันที
    ActiveWarp,    // กด key -> targeting -> teleport
    ActiveAoe,     // กด key -> AOE เลือกตำแหน่งได้ (ขยับ cursor ได้)
    ActiveAoeSelf, // กด key -> AOE ล็อกอยู่กับตัว ขยับ cursor ไม่ได้ กดอีกทีเพื่อปล่อย
};

// ============================================================
//  SkillEffect  -  ค่า effect ทุกประเภทรวมกัน
// ============================================================
struct SkillEffect
{
    int atkPct       = 0;   // % bonus ATK (buff)
    int matkPct      = 0;
    int hpPct        = 0;
    int magicDmgPct  = 0;   // % bonus Magic Damage (buff/passive)
    int manaCost     = 0;   // AP cost ของสกิลเวท (buff/passive)
    int staminaCost  = 0;
    int defPct       = 0;   // % bonus DEF (buff/passive)
    int dodgePct     = 0;   // % bonus Dodge
    int healFlat     = 0;   // heal คงที่
    int healPct      = 0;   // heal % of maxHp
    int damagePct    = 0;   // % of ATK (ranged/aoe)
    int range        = 0;   // tiles (ranged/warp)
    int speedBonus   = 0;   // AP bonus (+1 AP per turn)
    int atkSpeedPct  = 100; // AP cost ของการโจมตี x100 (100=ปกติ, 50=ถูกกว่า)
    int aoeRadius    = 0;   // radius ของ AOE
    std::string scalingStat = "atk"; // "atk", "def", "dodge", "magic_dmg" - stat ที่ใช้ในการคำนวณ damage ของ ranged/aoe
    // ── Status Effect on hit ──
    std::string applyStatus  = "";   // "bleed", "poison", "burn", "stun", "slow"
    int         statusPower  = 0;    // damage per tick / magnitude
    int         statusDuration = 0;  // จำนวนเทิร์น
};

// ============================================================
//  SkillData  -  ข้อมูลจาก skills.json
// ============================================================
struct SkillData
{
    std::string id;
    std::string name;
    std::string desc;
    SkillType   type      = SkillType::Passive;
    int         cooldown  = 0;   // turns
    int         duration  = 0;   // turns (สำหรับ buff)
    int         hotbarSlot = -1; // slot 0-8, -1 = ไม่ได้ assign
    std::string icon = "";
    SkillEffect effect;

    // backward-compat aliases (ชี้ไปที่ effect)
    int& atkPct    () { return effect.atkPct; }
    int& matkPct   () { return effect.matkPct;}
    int& hpPct     () { return effect.hpPct;} 
    int& defPct    () { return effect.defPct; }
    int& dodgePct  () { return effect.dodgePct; }
    int& damage    () { return effect.damagePct; }
    int& range     () { return effect.range; }
    const int& atkPct () const { return effect.atkPct; }
    const int& matkPct () const { return effect.matkPct; }
    const int& hpPct () const { return effect.hpPct;}
    const int& defPct () const { return effect.defPct; }
    const int& range  () const { return effect.range; }
    const std::string& scalingStat () const { return effect.scalingStat; }
};

// ============================================================
//  SkillInstance  -  runtime state ของ skill 1 ตัว
// ============================================================
struct SkillInstance
{
    SkillData   data;
    int         cooldownLeft  = 0;
    int         durationLeft  = 0;
    bool        buffActive    = false;
    bool fromCore = false;
    
    bool isReady()   const { return cooldownLeft <= 0; }
    bool isPassive() const { return data.type == SkillType::Passive; }

    void tick()
    {
        if (cooldownLeft > 0) cooldownLeft--;
        if (durationLeft > 0)
        {
            durationLeft--;
            if (durationLeft == 0) buffActive = false;
        }
    }
};
