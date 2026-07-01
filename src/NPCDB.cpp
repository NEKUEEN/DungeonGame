#include "NPCDB.hpp"
#include <fstream>
#include <iostream>

bool NPCDB::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "[NPCDB] Could not open " << path << "\n";
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    auto root = nlohmann::json::parse(content);
    if (!root.is_array() && !root.is_object())
    {
        std::cerr << "[NPCDB] Invalid JSON format\n";
        return false;
    }

    // Handle both direct array and object with "npcs" key
    auto npcArray = root.is_array() ? root : root["npcs"];
    if (!npcArray.is_array())
    {
        std::cerr << "[NPCDB] No 'npcs' array found\n";
        return false;
    }

    for (size_t i = 0; i < npcArray.size(); i++)
    {
        auto elem = npcArray[i];
        NPCData data;
        data.id       = elem.contains("id")       ? elem["id"].get<std::string>()       : "";
        data.name     = elem.contains("name")     ? elem["name"].get<std::string>()     : "";
        data.type     = elem.contains("type")     ? elem["type"].get<std::string>()     : "QuestGiver";
        data.role     = elem.contains("role")     ? elem["role"].get<std::string>()     : "Warrior";
        data.level    = elem.contains("level")    ? elem["level"].get<int>()            : 1;
        data.desc     = elem.contains("desc")     ? elem["desc"].get<std::string>()     : "";
        data.sprite   = elem.contains("sprite")   ? elem["sprite"].get<std::string>()   : "";
        data.recruitCost = elem.contains("recruitCost") ? elem["recruitCost"].get<int>() : 0;

        // Merchant items
        if (elem.contains("items"))
        {
            auto itemsArr = elem["items"];
            if (itemsArr.is_array())
            {
                for (size_t j = 0; j < itemsArr.size(); j++)
                    data.items.push_back(itemsArr[j].get<std::string>());
            }
        }

        if (!data.id.empty())
            m_npcs[data.id] = data;
    }

    std::cout << "[NPCDB] Loaded " << m_npcs.size() << " NPC templates\n";
    return true;
}

const NPCData* NPCDB::get(const std::string& npcId) const
{
    auto it = m_npcs.find(npcId);
    if (it == m_npcs.end()) return nullptr;
    return &it->second;
}

std::shared_ptr<NPC> NPCDB::createNPC(const std::string& npcId)
{
    const NPCData* data = get(npcId);
    if (!data) return nullptr;

    std::shared_ptr<NPC> npc;

    if (data->type == "Companion")
    {
        NPCRole role = stringToRole(data->role);
        npc = std::make_shared<NPC>(data->id, data->name, role, data->level, data->desc);
        npc->setSprite(data->sprite);  // เซ็ต sprite จาก JSON
    }
    else if (data->type == "Merchant")
    {
        npc = std::make_shared<NPC>(data->id, data->name, NPCType::Merchant, data->desc);
        npc->setMerchantItems(data->items);
        npc->setSprite(data->sprite);  // เซ็ต sprite จาก JSON
    }
    else
    {
        npc = std::make_shared<NPC>(data->id, data->name, NPCType::QuestGiver, data->desc);
        npc->setSprite(data->sprite);  // เซ็ต sprite จาก JSON
    }

    return npc;
}

NPCRole NPCDB::stringToRole(const std::string& roleStr) const
{
    if (roleStr == "Warrior") return NPCRole::Warrior;
    if (roleStr == "Mage")    return NPCRole::Mage;
    if (roleStr == "Healer")  return NPCRole::Healer;
    if (roleStr == "Rogue")   return NPCRole::Rogue;
    return NPCRole::Warrior;  // default
}
