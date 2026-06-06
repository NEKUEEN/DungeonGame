# DungeonGame – Setup Guide

## โครงสร้างโปรเจ็กต์
```
DungeonGame/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── Game.hpp
│   └── Game.cpp
└── assets/
    ├── fonts/         ← วางไฟล์ font.ttf ที่นี่
    └── textures/      ← วางไฟล์ tileset ที่นี่ (Phase 2)
```

## วิธี Build

### ต้องติดตั้งก่อน
- CMake (https://cmake.org/download/)
- MinGW-w64 (https://www.mingw-w64.org/)
- SFML 3.1.0 ที่ C:\SFML-3.1.0

### ขั้นตอน (ใน VS Code)
1. เปิดโฟลเดอร์ DungeonGame ใน VS Code
2. ติดตั้ง Extension: **CMake Tools** (ms-vscode.cmake-tools)
3. กด `Ctrl+Shift+P` → `CMake: Configure`
4. กด `Ctrl+Shift+P` → `CMake: Build`
5. ไฟล์ .exe จะอยู่ใน `build/`

### เพิ่ม Font
- Download font ฟรีได้จาก https://fonts.google.com
- เลือกไฟล์ .ttf แล้วเปลี่ยนชื่อเป็น `font.ttf`
- วางไว้ที่ `assets/fonts/font.ttf`

## Roadmap
## Roadmap — DungeonGame (อัพเดท)

### ✅ เสร็จแล้ว
- Dungeon generation, Camera, Fog of War
- Player: Move, Turn, HP Regen, Hunger, Level Up, EXP
- Enemy: 3 ประเภท, 3 Rank, AI, Floor scaling, Respawn
- MonsterDB / DropTable (JSON-driven)
- Inventory, Equipment (7 slots), Core Slots (level-based)
- Sprite system + TextureManager
- หลายชั้น, บันไดขึ้น/ลง
- Stats: Body, Mentality (skeleton), Ability, BattleIndex, ItemLevel
- **Skill System ใหม่** — Num1-9 hotbar, executeSkill data-driven
- **Action Point System** — Speed buff, Attack Speed buff
- **Core → Stats** — CoreStats เป็น hp/atk/def/dodge อ่านจาก JSON
- **Core → Skill** — equip core → สกิลขึ้น hotbar
- **Mana System** — mana_cost, regen per turn
- **Skill Scaling** — scaling_stat (atk/magic_dmg)
- **getBuffedMagicDmg()**

### 🔴 Priority 1
- [ ] New Ai Enemy
    
- [ ] Death Screen / Game Over UI
- [ ] Kill Count System (per family/id)
- [ ] Boss Trigger
- [ ] เลือกเผ่า

### 🟡 Priority 2
- [ ] Map hand craft
- [ ] Mentality sub-stats เพิ่มเติม
- [ ] Stamina System
- [ ] Monster Skills
- [ ] Monster Encyclopedia
 เงื่อนไขที่ 1 — First Kill EXP
ฆ่ามอน id ใหม่ที่ไม่เคยฆ่ามาก่อน → ได้ EXP ตาม rank
Normal +1, Elite +2, Boss +3 เหมือนเดิม
ฆ่าซ้ำ → ไม่ได้ EXP แบบนี้อีก
 เงื่อนไขที่ 2 — Kill Count EXP
ฆ่ามอน id เดิมครบ 100 ตัว → Level Up ทันที
นับแยกต่างหากแต่ละ id
เช่น Goblin Warrior 100 ตัว = Level Up
- [ ] NPC + Shop
- [ ] Save/Load

### 🟠 Priority 3
- [ ] Party System
- [ ] Town Loop
