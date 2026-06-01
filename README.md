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
- [x] Phase 1: Project Structure + Window
- [ ] Phase 2: TileMap + Dungeon Floor
- [ ] Phase 3: Player + Grid Movement + Turn System
- [ ] Phase 4: Stats Panel (body/mentality/ability)
- [ ] Phase 5: Log Window
- [ ] Phase 6: HP Bar ใต้ตัวละคร
- [ ] Phase 7: Inventory System
- [ ] Phase 8: Hunger System
- [ ] Phase 9: Enemy + AI
- [ ] Phase 10: Combat System
