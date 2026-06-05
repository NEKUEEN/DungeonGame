# Patch: AP System + Hotbar + Skill Types

## ไฟล์ที่แก้ใหม่ทั้งหมด (อยู่ใน patch แล้ว)
- src/Skill.hpp     — เพิ่ม SkillType ใหม่ + SkillEffect struct
- src/SkillDB.hpp   — รองรับ format ใหม่ + backward-compat เก่า
- src/Player.hpp    — เพิ่ม AP fields, hotbar[9], setPos(), atkSpeedCost, atkAPAccum
- src/Player.cpp    — hotbar init จาก hotbarSlot ใน JSON
- src/Game.hpp      — เพิ่ม executeSkill, recalcAP, executeWarp, executeAoe, renderHotbar
- src/Game.cpp      — AP logic, hotbar input (Num1-9), hotbar render
- src/Skill.cpp     — ว่าง (ไม่เปลี่ยน)
- src/main.cpp      — ไม่เปลี่ยน
- assets/data/skills.json — format ใหม่ (effect object + hotbar_slot)

## ไฟล์ที่ต้อง copy จาก project เดิม (ไม่ได้แก้)
- src/TileMap.hpp / TileMap.cpp
- src/Enemy.hpp / Enemy.cpp
- src/FogOfWar.hpp / FogOfWar.cpp
- src/Item.hpp / Item.cpp
- src/Inventory.hpp / Inventory.cpp
- src/Equipment.hpp / Equipment.cpp
- src/CoreSlots.hpp / CoreSlots.cpp
- src/MonsterDB.hpp
- src/TextureManager.hpp
- src/DropTable.hpp
- src/lib/json.hpp
- CMakeLists.txt
- assets/data/items.json
- assets/data/monsters.json

## สิ่งที่เปลี่ยนหลักๆ
1. SkillType เพิ่ม: ActiveHeal, ActiveWarp, ActiveAoe
2. SkillEffect struct รวม effect ทุกประเภท
3. Stats เพิ่ม: maxAP, currentAP, atkSpeedCost, atkAPAccum
4. Player.m_hotbar[9] — กด 1-9 ใช้ skill
5. executeSkill(hotbarIdx) แทน Q/F key เดิม
6. recalcAP() — คำนวณ AP ใหม่ทุก turn
7. playerAttack() หัก AP ตาม atkSpeedCost
8. handlePlayerMove() เช็ค AP ก่อนเดิน
9. renderHotbar() — แถบ skill ด้านล่าง game view
