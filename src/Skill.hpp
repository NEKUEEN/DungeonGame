#pragma once
#include <string>

// ============================================================
//  SkillType
// ============================================================
enum class SkillType
{
    ActiveBuff,    // กด key -> buff ชั่วคราว
    Passive,       // auto-apply ตลอดเวลา
    ActiveRanged,  // กด key -> เลือกทิศ -> ยิง projectile
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
    int         atkPct    = 0;   // % bonus ATK
    int         defPct    = 0;   // % bonus DEF
    int         dodgePct  = 0;   // % bonus Dodge
    int         damage    = 0;   // % of ATK (ranged)
    int         range     = 0;   // tiles (ranged)
};

// ============================================================
//  SkillInstance  -  runtime state ของ skill 1 ตัว
// ============================================================
struct SkillInstance
{
    SkillData   data;
    int         cooldownLeft  = 0;   // turns เหลือก่อนใช้ได้อีก
    int         durationLeft  = 0;   // turns คงเหลือของ buff ปัจจุบัน
    bool        buffActive    = false;

    bool isReady()   const { return cooldownLeft <= 0; }
    bool isPassive() const { return data.type == SkillType::Passive; }

    // เรียกทุก turn
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
