#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "Item.hpp"

constexpr int MAX_INVENTORY = 12;

// ============================================================
//  ItemStack  –  item + จำนวน
// ============================================================
struct ItemStack
{
    Item item;
    int  count = 1;
};

class Inventory
{
public:
    Inventory() = default;

    bool addItem(const Item& item);      // stack ถ้าเป็นประเภทเดียวกัน
    void removeItem(int index);          // ลด count ทีละ 1 ถ้า count > 1 ถึงจะลบ slot
    void removeStack(int index);         // ลบทั้ง stack
    Item* getItem(int index);            // return item ใน slot
    int   getCount(int index) const;     // จำนวนใน slot
    int   size() const { return (int)m_stacks.size(); }
    bool  isFull() const;                // เต็มเมื่อ slot ครบ MAX และ stack ไม่ตรง

    // weapon/armor bonus
    int equippedWeaponBonus = 0;
    int equippedArmorBonus  = 0;

private:
    bool canStack(const Item& a, const Item& b) const;  // stack ได้เฉพาะ usable

    std::vector<ItemStack> m_stacks;
};
