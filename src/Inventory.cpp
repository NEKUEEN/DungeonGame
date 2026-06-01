#include "Inventory.hpp"
#include <algorithm>

bool Inventory::addItem(const Item& item)
{
    if (isFull()) return false;
    m_items.push_back(item);
    return true;
}

void Inventory::removeItem(int index)
{
    if (index < 0 || index >= (int)m_items.size()) return;
    m_items.erase(m_items.begin() + index);
}

Item* Inventory::getItem(int index)
{
    if (index < 0 || index >= (int)m_items.size()) return nullptr;
    return &m_items[index];
}

// ============================================================
//  Render  –  popup กลางจอ
// ============================================================
void Inventory::render(sf::RenderWindow& window, const sf::Font& font,
                       int selectedIndex)
{
    // หาขนาดหน้าต่าง
    auto winSize = window.getSize();
    float popW = 320.f, popH = 360.f;
    float popX = (winSize.x - popW) / 2.f;
    float popY = (winSize.y - popH) / 2.f;

    // พื้นหลัง
    sf::RectangleShape bg({popW, popH});
    bg.setFillColor(sf::Color(15, 12, 8, 230));
    bg.setOutlineColor(sf::Color(120, 90, 50));
    bg.setOutlineThickness(2.f);
    bg.setPosition({popX, popY});
    window.draw(bg);

    // Title
    sf::Text title(font, "[ INVENTORY ]", 13);
    title.setFillColor(sf::Color::Yellow);
    title.setPosition({popX + 10.f, popY + 8.f});
    window.draw(title);

    sf::Text hint(font, "[W/S] Select  [U] Use/Equip  [D] Drop  [I] Close", 8);
    hint.setFillColor(sf::Color(120, 120, 120));
    hint.setPosition({popX + 10.f, popY + 28.f});
    window.draw(hint);

    // Divider
    sf::RectangleShape divider({popW - 20.f, 1.f});
    divider.setFillColor(sf::Color(80, 60, 40));
    divider.setPosition({popX + 10.f, popY + 42.f});
    window.draw(divider);

    // Items
    float iy = popY + 50.f;
    if (m_items.empty())
    {
        sf::Text empty(font, "  (empty)", 11);
        empty.setFillColor(sf::Color(100,100,100));
        empty.setPosition({popX + 10.f, iy});
        window.draw(empty);
        return;
    }

    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        const Item& item = m_items[i];
        bool selected = (i == selectedIndex);

        // Highlight
        if (selected)
        {
            sf::RectangleShape hl({popW - 16.f, 20.f});
            hl.setFillColor(sf::Color(60, 50, 30, 180));
            hl.setPosition({popX + 8.f, iy - 2.f});
            window.draw(hl);
        }

        // Color dot
        sf::CircleShape dot(4.f);
        dot.setFillColor(item.color());
        dot.setPosition({popX + 14.f, iy + 4.f});
        window.draw(dot);

        // Name + value
        std::string label = "  " + item.name;
        if (item.type == ItemType::Food)   label += " (+" + std::to_string(item.value) + " hunger)";
        if (item.type == ItemType::Potion) label += " (+" + std::to_string(item.value) + " HP)";
        if (item.type == ItemType::Weapon) label += " (+" + std::to_string(item.value) + " ATK)";
        if (item.type == ItemType::Helmet   ||
            item.type == ItemType::BodyArmor ||
            item.type == ItemType::Gloves    ||
            item.type == ItemType::Greaves   ||
            item.type == ItemType::Boots)
            label += " (+" + std::to_string(item.value) + " DEF)";
        if (item.type == ItemType::Weapon)    label += " (+" + std::to_string(item.value) + " ATK)";
        if (item.type == ItemType::OffWeapon) label += " (+" + std::to_string(item.value) + " ATK)";

        sf::Text txt(font, label, 11);
        txt.setFillColor(selected ? sf::Color::White : sf::Color(180,180,180));
        txt.setPosition({popX + 22.f, iy});
        window.draw(txt);

        iy += 22.f;
    }

    // Slot count
    sf::Text slots(font, std::to_string(m_items.size()) + "/" + std::to_string(MAX_INVENTORY), 9);
    slots.setFillColor(sf::Color(100,100,100));
    slots.setPosition({popX + popW - 40.f, popY + popH - 18.f});
    window.draw(slots);
}
