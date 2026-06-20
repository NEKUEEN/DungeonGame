#pragma once
#include <string>

struct UIState {
    enum class Panel { Inventory, Equipment, Cores, Stats };
    Panel activePanel = Panel::Inventory;

    int selectedInvSlot = 0;
    int selectedEquipSlot = 0;
    int selectedCoreSlot = 0;
    int selectedRace = 0;

    bool levelUpFlash = false;
    int levelUpTimer = 0;

    struct Targeting {
        bool active = false;
        int targetCol = 0, targetRow = 0;
        std::string skillId;
        bool isArea     = false;
        int  areaRadius = 0;
        void reset() { active = false; skillId.clear(); isArea = false; areaRadius = 0; }
    } targeting;

    void togglePanel(Panel p) {
        if (activePanel == p)
            activePanel = Panel::Inventory;
        else
            activePanel = p;
    }

    bool isPanelOpen(Panel p) const { return activePanel == p; }
};