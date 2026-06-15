#pragma once
#include <string>
#include <SFML/Graphics.hpp>

enum class StatusType {
    Bleed,      // เลือดไหล — tick damage
    Poison,     // พิษ — tick damage (ต่างจาก bleed ตรง resist)
    Burn,       // ไฟ — tick damage + ลด defense
    Stun,       // หยุดเคลื่อนที่
    Slow,       // ลด speed
    Regen,      // ฟื้น HP ต่อเทิร์น (buff)
    Shield,     // ดูด damage (buff)
};

struct StatusEffect {
    StatusType  type;
    int         duration;   // เหลือกี่เทิร์น
    int         power;      // damage/heal per tick หรือ magnitude
    std::string sourceId;   // skill/item ที่ปล่อยมา (debug)

    // tick คืน true ถ้ายังมีผลอยู่
    bool tick() { return --duration > 0; }
    bool isActive() const { return duration > 0; }

    // สำหรับ UI
    sf::Color color() const {
        switch(type) {
            case StatusType::Bleed:  return sf::Color(200, 30, 30);
            case StatusType::Poison: return sf::Color(80, 200, 80);
            case StatusType::Burn:   return sf::Color(255, 140, 0);
            case StatusType::Stun:   return sf::Color(255, 255, 80);
            case StatusType::Slow:   return sf::Color(80, 80, 200);
            case StatusType::Regen:  return sf::Color(80, 255, 160);
            case StatusType::Shield: return sf::Color(180, 180, 255);
            default:                 return sf::Color::White;
        }
    }

    std::string icon() const {
        switch(type) {
            case StatusType::Bleed:  return "🩸";
            case StatusType::Poison: return "☠";
            case StatusType::Burn:   return "🔥";
            case StatusType::Stun:   return "★";
            case StatusType::Slow:   return "❄";
            case StatusType::Regen:  return "♥";
            case StatusType::Shield: return "🛡";
            default:                 return "?";
        }
    }
};