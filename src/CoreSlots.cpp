// ==================== CoreSlots.cpp (สมบูรณ์ เปลี่ยน magicDmg -> matk) ====================
#include "CoreSlots.hpp"
#include <algorithm>

bool CoreSlots::equip(int slotIdx, const Item& core) {
    if (slotIdx < 0 || slotIdx >= (int)m_slots.size()) return false;
    if (core.type != ItemType::Core) return false;
    m_slots[slotIdx] = core;
    return true;
}

Item CoreSlots::unequip(int slotIdx) {
    if (slotIdx < 0 || slotIdx >= (int)m_slots.size()) return Item{};
    if (!m_slots[slotIdx].has_value()) return Item{};
    Item item = m_slots[slotIdx].value();
    m_slots[slotIdx].reset();
    return item;
}

bool CoreSlots::hasCore(int slotIdx) const {
    if (slotIdx < 0 || slotIdx >= (int)m_slots.size()) return false;
    return m_slots[slotIdx].has_value();
}

const Item* CoreSlots::getCore(int slotIdx) const {
    if (slotIdx < 0 || slotIdx >= (int)m_slots.size()) return nullptr;
    if (!m_slots[slotIdx].has_value()) return nullptr;
    return &m_slots[slotIdx].value();
}

StatBonus CoreSlots::readStats(const Item& core) const {
    StatBonus cs;
    if (core.id.empty()) return cs;

    const ItemData* data = DropTable::instance().getItem(core.id);
    if (!data) return cs;

    cs.hp    = data->coreHp;
    cs.atk   = data->coreAtk;
    cs.def   = data->coreDef;
    cs.dodge = data->coreDodge;
    cs.mana  = data->coreMana;
    cs.matk  = data->coreMagicDmg;     // เปลี่ยน
    cs.magicRes = data->coreMagicRes;
    cs.spd   = data->coreSpdBonus;
    cs.slashDmgBonus  = data->coreSlashDmg;
    cs.pierceDmgBonus = data->corePierceDmg;
    cs.bluntDmgBonus  = data->coreBluntDmg;
    cs.cleaveDmgBonus = data->coreCleaveDmg;
    cs.bleedBonus   = data->coreBleedBonus;
    cs.poisonBonus  = data->corePoisonBonus;
    cs.burnBonus    = data->coreBurnBonus;
    cs.resistBleed  = data->coreResistBleed;
    cs.resistPoison = data->coreResistPoison;
    cs.resistBurn   = data->coreResistBurn;
    cs.resistStun   = data->coreResistStun;
    cs.resistSlow   = data->coreResistSlow;
    cs.bleedDmgReduce  = data->coreBleedDmgReduce;
    cs.bleedDurReduce  = data->coreBleedDurReduce;
    cs.poisonDmgReduce = data->corePoisonDmgReduce;
    cs.poisonDurReduce = data->corePoisonDurReduce;
    cs.burnDmgReduce   = data->coreBurnDmgReduce;
    cs.burnDurReduce   = data->coreBurnDurReduce;
    cs.stunDurReduce   = data->coreStunDurReduce;
    cs.slowDurReduce   = data->coreSlowDurReduce;
    return cs;
}

StatBonus CoreSlots::getTotalBonus() const {
    StatBonus total;
    for (const auto& slot : m_slots) {
        if (!slot.has_value()) continue;
        total += readStats(slot.value());
    }
    return total;
}

void CoreSlots::render(sf::RenderWindow& window, const sf::Font& font,
                       float startX, float startY, float panelW,
                       int selectedSlot, bool focused) {
    sf::Text title(font, "CORE SLOTS", 9);
    title.setFillColor(sf::Color(100,200,255));
    title.setPosition({startX, startY});
    window.draw(title);

    float sy = startY + 16.f;
    const float SZ = 26.f, GAP = 4.f;

    for (int i = 0; i < (int)m_slots.size(); ++i) {
        float sx = startX + i * (SZ + GAP);
        bool  sel = (i == selectedSlot && focused);

        sf::RectangleShape slot({SZ, SZ});
        slot.setFillColor(sel ? sf::Color(30,60,80) : sf::Color(8, 8, 8));
        slot.setOutlineColor(sel ? sf::Color(100,200,255) : sf::Color(120, 120, 120));
        slot.setOutlineThickness(1.5f);
        slot.setPosition({sx, sy});
        window.draw(slot);

        if (m_slots[i].has_value()) {
            const Item& core = m_slots[i].value();
            sf::Texture tex;
            if (!core.spriteName.empty() && tex.loadFromFile("assets/textures/"+core.spriteName)) {
                sf::Sprite spr(tex);
                auto sz = tex.getSize();
                float sc = SZ*0.85f / std::max((float)sz.x, (float)sz.y);
                spr.setScale({sc,sc});
                spr.setPosition({sx+SZ*0.08f, sy+SZ*0.08f});
                window.draw(spr);
            } else {
                sf::CircleShape dot(SZ*0.3f);
                dot.setFillColor(core.color());
                dot.setOutlineColor(sf::Color(100,200,255,150));
                dot.setOutlineThickness(1.f);
                dot.setPosition({sx+SZ*0.2f, sy+SZ*0.2f});
                window.draw(dot);
            }
        } else {
            sf::Text num(font, std::to_string(i+1), 8);
            num.setFillColor(sf::Color(50,80,100));
            num.setPosition({sx+SZ/2.f-4.f, sy+SZ/2.f-5.f});
            window.draw(num);
        }
    }

    if (selectedSlot >= 0 && selectedSlot < (int)m_slots.size() && m_slots[selectedSlot].has_value()) {
        const Item& core = m_slots[selectedSlot].value();
        StatBonus cs = readStats(core);
        std::string info = core.name;
        if (cs.hp)    info += " HP+"+std::to_string(cs.hp);
        if (cs.atk)   info += " ATK+"+std::to_string(cs.atk);
        if (cs.def)   info += " DEF+"+std::to_string(cs.def);
        if (cs.dodge) info += " DOD+"+std::to_string(cs.dodge);
        if (cs.mana)  info += " MANA+"+std::to_string(cs.mana);
        if (cs.matk)  info += " MATK+"+std::to_string(cs.matk);    // เปลี่ยน
        if (cs.magicRes) info += " MRES+"+std::to_string(cs.magicRes);
        if (cs.slashDmgBonus)  info += " SLA+"+std::to_string(cs.slashDmgBonus);
        if (cs.pierceDmgBonus) info += " PIE+"+std::to_string(cs.pierceDmgBonus);
        if (cs.bluntDmgBonus)  info += " BLU+"+std::to_string(cs.bluntDmgBonus);
        if (cs.cleaveDmgBonus) info += " CLE+"+std::to_string(cs.cleaveDmgBonus);

        sf::Text infoTxt(font, info, 8);
        infoTxt.setFillColor(sf::Color::White);
        infoTxt.setPosition({startX, sy + SZ + 4.f});
        window.draw(infoTxt);
    } else {
        sf::Text empty(font, "Slot "+std::to_string(selectedSlot+1)+": empty", 8);
        empty.setFillColor(sf::Color::White);
        empty.setPosition({startX, sy + SZ + 4.f});
        window.draw(empty);
    }
}