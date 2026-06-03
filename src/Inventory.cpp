#include "Inventory.hpp"
#include <algorithm>

// ============================================================
//  canStack  –  stack ได้เฉพาะ item ที่ stackable และเหมือนกัน
// ============================================================
bool Inventory::canStack(const Item& a, const Item& b) const
{
    if (!a.stackable || !b.stackable) return false;
    return a.type == b.type && a.name == b.name && a.spriteName == b.spriteName;
}

// ============================================================
//  addItem  –  ถ้า stackable หาช่องที่มีอยู่แล้ว ถ้าไม่มีสร้างใหม่
// ============================================================
bool Inventory::addItem(const Item& item)
{
    // ลองหา stack ที่มีอยู่แล้ว
    if (item.stackable)
    {
        for (auto& stack : m_stacks)
            if (canStack(stack.item, item))
            { stack.count++; return true; }
    }

    // ต้องสร้าง slot ใหม่
    if ((int)m_stacks.size() >= MAX_INVENTORY) return false;
    m_stacks.push_back({item, 1});
    return true;
}

// ============================================================
//  removeItem  –  ลด count 1 ถ้า count > 1 ไม่งั้นลบ slot
// ============================================================
void Inventory::removeItem(int index)
{
    if (index < 0 || index >= (int)m_stacks.size()) return;
    m_stacks[index].count--;
    if (m_stacks[index].count <= 0)
        m_stacks.erase(m_stacks.begin() + index);
}

void Inventory::removeStack(int index)
{
    if (index < 0 || index >= (int)m_stacks.size()) return;
    m_stacks.erase(m_stacks.begin() + index);
}

Item* Inventory::getItem(int index)
{
    if (index < 0 || index >= (int)m_stacks.size()) return nullptr;
    return &m_stacks[index].item;
}

int Inventory::getCount(int index) const
{
    if (index < 0 || index >= (int)m_stacks.size()) return 0;
    return m_stacks[index].count;
}

bool Inventory::isFull() const
{
    return (int)m_stacks.size() >= MAX_INVENTORY;
}
