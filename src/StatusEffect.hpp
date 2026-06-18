#pragma once
#include <string>

enum class StatusType {
    Bleed, Poison, Burn, Stun, Slow, Regen, Shield,
};

struct StatusEffect {
    StatusType  type;
    int         duration;
    int         power;
    std::string sourceId;

    bool tick() { return --duration > 0; }
    bool isActive() const { return duration > 0; }

    std::string typeId() const
    {
        switch(type) {
            case StatusType::Bleed:  return "bleed";
            case StatusType::Poison: return "poison";
            case StatusType::Burn:   return "burn";
            case StatusType::Stun:   return "stun";
            case StatusType::Slow:   return "slow";
            case StatusType::Regen:  return "regen";
            case StatusType::Shield: return "shield";
            default:                 return "unknown";
        }
    }
};