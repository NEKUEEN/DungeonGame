#pragma once
#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <iostream>

// ============================================================
//  TextureManager  –  Singleton โหลด texture ครั้งเดียว
//  ใช้งาน: TextureManager::instance().load("name","path.png")
//           TextureManager::instance().get("name")
// ============================================================
class TextureManager
{
public:
    static TextureManager& instance()
    { static TextureManager tm; return tm; }

    void load(const std::string& name, const std::string& path)
    {
        if (m_textures.count(name)) return;  // โหลดแล้ว
        sf::Texture tex;
        if (!tex.loadFromFile(path))
            std::cerr<<"[TextureManager] Failed: "<<path<<"\n";
        else
            m_textures[name] = std::move(tex);
    }

    const sf::Texture* get(const std::string& name) const
    {
        auto it = m_textures.find(name);
        if (it == m_textures.end()) return nullptr;
        return &it->second;
    }

    

    void loadAll()
    {
        load("player",       "assets/textures/player.png");
        load("rat",          "assets/textures/rat.png");
        load("goblin",       "assets/textures/goblin.png");
        load("goblinSwordman",       "assets/textures/goblinSwordman.png");
        load("goblinArcher",       "assets/textures/goblinArcher.png");
        load("orc",          "assets/textures/orc.png");
        load("tile_floor",   "assets/textures/tile_floor.png");
        load("tile_wall",    "assets/textures/tile_wall.png");
        load("barbarian",       "assets/textures/barbarian.png");
        load("furry",       "assets/textures/furry.png");
        load("dwarf",       "assets/textures/dwarf.png");
        load("fairy",       "assets/textures/fairy.png");
        load("human",       "assets/textures/human.png");
        load("dummy",       "assets/textures/dummy.png");
        // zone floor textures
        load("darkness_floor",      "assets/textures/darkness_floor_tile.png");
        load("crystalBright_floor", "assets/textures/crystalBright_floor_tile.png");
        load("deadMan_floor",       "assets/textures/deadMan_floor_tile.png");
        load("blackRock_floor",     "assets/textures/blackRock_floor_tile.png");
        // zone wall textures
        load("crystalBright_wall",  "assets/textures/crystalBright_wall_tile.png");
        load("stairs_down",  "assets/textures/stairs_down.png");
        load("stairs_up",    "assets/textures/stairs_up.png");
        load("item_food",    "assets/textures/item_food.png");
        load("item_potion",  "assets/textures/item_potion.png");
        load("item_weapon",  "assets/textures/item_weapon.png");
        load("item_offweapon","assets/textures/item_offweapon.png");
        load("item_helmet",  "assets/textures/item_helmet.png");
        load("item_armor",   "assets/textures/item_armor.png");
        load("item_gloves",  "assets/textures/item_gloves.png");
        load("item_greaves", "assets/textures/item_greaves.png");
        load("item_boots",       "assets/textures/item_boots.png");
        load("item_magic_stone", "assets/textures/item_magic_stone.png");
        load("item_goblin_core", "assets/textures/item_goblin_core.png");
        load("item_orc_core",    "assets/textures/item_orc_core.png");
        load("item_rat_core",    "assets/textures/item_rat_core.png");
        load("item_goblinWizard_core", "assets/textures/item_goblinWizard_core.png");
        load("item_dagger", "assets/textures/item_dagger.png");
        //passive
        load("ironSkin", "assets/textures/ironSkin.png");
        //skill
        load("arrow", "assets/textures/arrow.png");
        load("goundSlam", "assets/textures/goundSlam.png");
        //Debuff icon
        load("debuff_bleed", "assets/textures/debuff_bleed.png");
        load("debuff_poison",    "assets/textures/debuff_poison.png");
        load("debuff_burn",    "assets/textures/debuff_burn.png");
        load("debuff_stun", "assets/textures/debuff_stun.png");
        load("debuff_slow", "assets/textures/debuff_slow.png");
        //weapon
        load("weapon_mace", "assets/textures/weapon_mace.png");
        load("greatHammer", "assets/textures/greatHammer.png");
        load("greatSword",    "assets/textures/greatSword.png");
        load("shield",    "assets/textures/shield.png");
        load("sword", "assets/textures/sword.png");
        load("bow", "assets/textures/bow.png");
        load("itemArrow", "assets/textures/itemArrow.png");
        


        
    }

private:
    TextureManager() = default;
    std::map<std::string, sf::Texture> m_textures;
};
